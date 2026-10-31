"""Temporary-directory tests; ORBIT_BASELINE_REPO supplies offline git objects."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('orbit_installer', HERE / 'install.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)

class InstallerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        repo = os.environ.get('ORBIT_BASELINE_REPO')
        if not repo:
            raise unittest.SkipTest('set ORBIT_BASELINE_REPO to the offline Orbit source checkout')
        cls.originals = {}
        for index, rule in enumerate(helper.RULES):
            if rule['before'] is not None:
                path = ('global-menu@unmade.space/' if rule['root'] == 'extension' else '') + rule['path']
                cls.originals[index] = subprocess.check_output(['git', '-C', repo, 'show',
                    'aa5a9934edf5ee88c651d76a80ecbf936b9500a0:' + path])
                if helper.digest(cls.originals[index]) != rule['before']:
                    raise RuntimeError('offline fixture object hash mismatch')

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='qvolution-orbit-installer-')
        self.addCleanup(self.temp.cleanup)
        base = Path(self.temp.name)
        self.extension, self.daemon, self.work = base/'extension with spaces', base/'daemon files', base/'Qvolution-work'
        for directory in (self.extension, self.daemon): directory.mkdir()
        (self.extension/'metadata.json').write_text(json.dumps({'uuid': 'global-menu@unmade.space', 'version-name': '1.1.3'}))
        self.installer = helper.Installer(self.extension, self.daemon, self.work, HERE)
        for index, data in self.originals.items():
            target = self.installer.target(index)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            target.chmod(0o640)

    def assert_original(self):
        for index, rule in enumerate(helper.RULES):
            target = self.installer.target(index)
            if index in self.originals:
                self.assertEqual(target.read_bytes(), self.originals[index])
                self.assertEqual(target.stat().st_mode & 0o7777, 0o640)
            else:
                self.assertFalse(target.exists())

    def assert_installed(self):
        for index, rule in enumerate(helper.RULES):
            self.assertEqual(helper.digest(self.installer.target(index).read_bytes()), rule['after'])

    def test_dry_run_is_read_only_and_checks_all_baselines(self):
        self.assertIn('dry-run', self.installer.apply(dry_run=True))
        self.assertFalse(self.work.exists())
        self.assert_original()

    def test_cli_dry_run_apply_and_rollback(self):
        command = [sys.executable, str(HERE/'install.py'), 'apply',
                   '--extension-root', str(self.extension), '--daemon-root', str(self.daemon),
                   '--work-root', str(self.work)]
        result = subprocess.run(command + ['--dry-run'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(self.work.exists())
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_installed()
        command[2] = 'rollback'
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_original()

    def test_deleted_added_file_is_an_external_change(self):
        self.installer.apply()
        self.installer.target(1).unlink()
        with self.assertRaises(helper.Refuse): self.installer.rollback()
        self.assertEqual(self.installer.load()['state'], 'installed')
        self.assertTrue(self.installer.target(4).is_file())

    def test_invalid_manifest_is_refused_without_target_changes(self):
        self.installer.apply()
        self.installer.receipt.write_text('[]')
        with self.assertRaises(helper.Refuse): self.installer.rollback()
        self.assert_installed()

    def test_apply_repeat_and_rollback_repeat(self):
        self.installer.apply()
        self.assert_installed()
        state = self.installer.load()
        self.assertEqual(state['state'], 'installed')
        self.assertIn(self.work, self.installer.receipt.parents)
        self.assertEqual(self.installer.apply(), 'already applied')
        self.installer.rollback(dry_run=True)
        self.assert_installed()
        self.installer.rollback()
        self.assert_original()
        self.assertEqual(self.installer.rollback(), 'already rolled back')
        self.installer.apply()
        self.assert_installed()

    def test_modified_baseline_refused_before_backups_or_writes(self):
        self.installer.target(3).write_bytes(b'host local modification')
        for dry_run in (True, False):
            with self.assertRaises(helper.Refuse): self.installer.apply(dry_run=dry_run)
        self.assertFalse(self.work.exists())
        self.assertEqual(self.installer.target(3).read_bytes(), b'host local modification')
        self.assertFalse(self.installer.target(1).exists())

    def test_other_orbit_version_is_refused(self):
        (self.extension/'metadata.json').write_text(json.dumps({'uuid': 'global-menu@unmade.space', 'version-name': '1.2.0'}))
        with self.assertRaises(helper.Refuse): self.installer.apply()
        self.assertFalse(self.work.exists())
        self.assert_original()

    def test_existing_added_path_is_never_overwritten(self):
        target = self.installer.target(1)
        target.write_bytes(b'pre-existing user module')
        with self.assertRaises(helper.Refuse): self.installer.apply()
        self.assertEqual(target.read_bytes(), b'pre-existing user module')

    def test_payload_tampering_refused(self):
        bundle = Path(self.temp.name)/'bundle'
        shutil.copytree(HERE/'payload', bundle/'payload')
        (bundle/'payload/extension/windowData.js').write_bytes(b'tampered')
        installer = helper.Installer(self.extension, self.daemon, self.work, bundle)
        with self.assertRaises(helper.Refuse): installer.apply()
        self.assertFalse(self.work.exists())
        self.assert_original()

    def test_partial_apply_can_resume(self):
        def fail(index):
            if index == 1: raise OSError('injected write failure')
        with self.assertRaises(OSError): self.installer.apply(fault=fail)
        self.assertEqual(self.installer.load()['state'], 'partial')
        self.installer.apply()
        self.assert_installed()
        self.installer.rollback()
        self.assert_original()

    def test_partial_apply_can_rollback_without_finishing_apply(self):
        def fail(index):
            if index == 1: raise OSError('injected write failure')
        with self.assertRaises(OSError): self.installer.apply(fault=fail)
        self.installer.rollback()
        self.assert_original()

    def test_rollback_refuses_content_or_permission_changes(self):
        self.installer.apply()
        target = self.installer.target(1)
        installed = target.read_bytes()
        target.write_bytes(installed + b'\nuser change')
        with self.assertRaises(helper.Refuse): self.installer.rollback()
        self.assertEqual(self.installer.load()['state'], 'installed')
        target.write_bytes(installed)
        target.chmod(0o600)
        with self.assertRaises(helper.Refuse): self.installer.rollback()
        self.assert_installed()

    def test_partial_rollback_can_resume_and_blocks_apply(self):
        self.installer.apply()
        def fail(index):
            if index == 0: raise OSError('injected rollback failure')
        with self.assertRaises(OSError): self.installer.rollback(fault=fail)
        self.assertEqual(self.installer.load()['state'], 'rollback_partial')
        with self.assertRaises(helper.Refuse): self.installer.apply()
        self.installer.rollback()
        self.assert_original()

    def test_changed_backup_refuses_rollback(self):
        self.installer.apply()
        (self.installer.directory/'00.original').write_bytes(b'changed backup')
        with self.assertRaises(helper.Refuse): self.installer.rollback()
        self.assert_installed()

    def test_target_and_backup_symlinks_are_rejected(self):
        target = self.installer.target(0)
        data = target.read_bytes()
        target.unlink()
        other = Path(self.temp.name)/'other'; other.write_bytes(data)
        target.symlink_to(other)
        with self.assertRaises(helper.Refuse): self.installer.apply()
        target.unlink(); target.write_bytes(data); target.chmod(0o640)
        self.work.mkdir()
        outside = Path(self.temp.name)/'outside'; outside.mkdir()
        (self.work/'backups').symlink_to(outside, target_is_directory=True)
        with self.assertRaises(helper.Refuse): self.installer.apply()
        self.assertEqual(list(outside.iterdir()), [])

if __name__ == '__main__': unittest.main()
