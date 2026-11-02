import errno
import importlib.util
import json
import os
from pathlib import Path
import pwd
import re
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from contracts import (FOUNDATION_COMPLETIONS, check_terminal, payload_root,
                       prepare_foundation_directory, shell_present)
from processes import server_records, snapshot

spec = importlib.util.spec_from_file_location('gate_runner', Path(__file__).with_name('run.py'))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class Contracts(unittest.TestCase):
    def setUp(self):
        # Keep fixtures as evidence; the lane's never-delete rule also covers tests.
        self.root = Path(tempfile.mkdtemp(prefix='stage41e-gates-'))

    def test_missing_strong31_parent_then_real_create_truncate_read(self):
        path = self.root / 'Applications/r2-foundation-stub/foundation-strong31-io'
        (self.root / 'Applications/Stage41e').mkdir(parents=True)
        with self.assertRaises(OSError) as failure:
            os.open(path, os.O_CREAT | os.O_TRUNC | os.O_RDWR, 0o600)
        self.assertEqual(failure.exception.errno, errno.ENOENT)
        prepare_foundation_directory(self.root)
        for contents in (b'original-long-content', b'new'):
            fd = os.open(path, os.O_CREAT | os.O_TRUNC | os.O_RDWR, 0o600)
            os.write(fd, contents)
            os.lseek(fd, 0, os.SEEK_SET)
            self.assertEqual(os.read(fd, 100), contents)
            os.close(fd)
        self.assertEqual(path.stat().st_mode & 0o777, 0o600)

    def test_strong31_workdir_rejects_symlink(self):
        (self.root / 'Applications').mkdir()
        (self.root / 'Applications/r2-foundation-stub').symlink_to(self.root)
        with self.assertRaisesRegex(RuntimeError, 'symlink'):
            prepare_foundation_directory(self.root)

    def test_completion_lines_and_negative_controls(self):
        for name, pattern in FOUNDATION_COMPLETIONS.items():
            output = 'PASS strong31 Swift behavior' if name == 'strong31' else name + ': passed'
            self.assertIsNotNone(re.search(pattern, output + '\n', re.M))
            self.assertIsNone(re.search(pattern, 'CHECK ' + output + '\n', re.M))
            self.assertIsNone(re.search(pattern, output + ' incomplete\n', re.M))
            self.assertIsNone(re.search(pattern, 'PASS an earlier check\n', re.M))
            if name != 'strong31':
                # The old generic expectation rejects the fixture's own success.
                self.assertIsNone(re.search('PASS', output, re.M))

    def test_payload_root_matches_loader_host_mapping(self):
        root = payload_root(Path('/example/stage'))
        suffix = '/System/Library/Frameworks/CoreTelephony.framework/Versions/A/CoreTelephony'
        self.assertEqual(root + suffix, '/Volumes/SystemRoot/example/stage/usr/local/libexec/darling' + suffix)
        self.assertNotEqual(suffix, root + suffix)

    def test_terminal_first_probe_race_and_later_failure(self):
        probes = [{'exit': 0, 'shell': False}, {'exit': 0, 'shell': True}, {'exit': 0, 'shell': True}]
        self.assertTrue(shell_present(probes))
        self.assertFalse(shell_present([]))
        self.assertFalse(shell_present([{'exit': 0, 'shell': False}]))
        self.assertFalse(shell_present(probes + [{'exit': None, 'shell': False}]))
        record = dict(passed=True, shell_executed=True, alive_at_end=True, window=True,
                      shell=True, observed_seconds=90)
        check_terminal(record, 90)
        self.assertTrue(record['passed'])
        for field in ('shell_executed', 'alive_at_end', 'window', 'shell', 'observed_seconds'):
            failed = {**record, field: False, 'failures': []}
            check_terminal(failed, 90)
            self.assertFalse(failed['passed'])
            self.assertTrue(all(f['detail'] for f in failed['failures']))

    def test_terminal_probe_timeout_and_shutdown_failure_are_durable(self):
        work = self.root
        (work / 'p').mkdir()
        evidence = work / 'evidence'
        evidence.mkdir()
        args = SimpleNamespace(work=work, stage=work / 'stage', launcher=work / 'launcher', evidence=evidence)
        gate = runner.Runner(args)
        init = SimpleNamespace(wait=lambda seconds: 0)
        app = SimpleNamespace(poll=lambda: None, stderr=[])
        probe = SimpleNamespace()
        opened = []

        def popen(command, **kw):
            opened.append(command)
            if command[-1] == '/usr/bin/true':
                home = Path(kw['env']['DPREFIX']) / 'Users' / pwd.getpwuid(os.getuid()).pw_name
                home.mkdir(parents=True)
                return init
            return probe if '/bin/ps' in command else app

        shutdowns = iter([0, 1])

        def run(command, **kw):
            return SimpleNamespace(returncode=next(shutdowns) if 'qv-shutdown' in command[0] else 0)

        with patch.object(runner.subprocess, 'Popen', side_effect=popen), \
             patch.object(runner.subprocess, 'run', side_effect=run), \
             patch.object(runner, 'wait', side_effect=[0, None]), \
             patch.object(runner, 'snapshot', return_value={'processes': []}), \
             patch.object(runner, 'server_records', return_value={}):
            with self.assertRaisesRegex(RuntimeError, 'cleanup failed'):
                gate.case('terminal90', ['/Applications/Stage41e/Terminal.app/Contents/MacOS/Terminal'], [], desktop='terminal')
        row = json.loads((evidence / 'results.json').read_text())[0]
        self.assertFalse(row['passed'])
        self.assertTrue(row['alive_at_end'])
        self.assertEqual(row['shutdown_exit'], 1)
        self.assertEqual(row['host_guard_exit'], 0)
        checks = [item['check'] for item in row['failures']]
        self.assertIn('shell_probe_timeout', checks)
        self.assertIn('shutdown', checks)
        self.assertTrue((evidence / 'terminal90/observation-0.json').is_file())
        self.assertEqual(len(opened), 3)  # no new prefix or launch after cleanup failure

    def test_shutdown_adapter_keeps_our_launcher_error_and_exit(self):
        launcher = self.root / 'fake-launcher'
        launcher.write_text('#!/bin/sh\nprintf "fixture shutdown refusal\\n" >&2\nexit 7\n')
        launcher.chmod(0o755)
        log = self.root / 'shutdown.log'
        env = {**os.environ, 'QV_REAL_LAUNCHER': str(launcher), 'QV_SHUTDOWN_LOG': str(log)}
        result = subprocess.run([str(Path(__file__).with_name('shutdown-logged.py')), 'shutdown'], env=env)
        self.assertEqual(result.returncode, 7)
        self.assertEqual(log.read_text(), 'fixture shutdown refusal\n')

    def test_desktop_shellspawn_timeout_retries_exactly_once(self):
        evidence = self.root / 'evidence'
        evidence.mkdir()
        gate = runner.Runner(SimpleNamespace(stage=self.root, launcher=self.root / 'launcher'))
        first = SimpleNamespace(poll=lambda: 1, wait=lambda seconds: 1,
                                stderr=[b'Timed out waiting for shellspawn in container\n'])
        second = SimpleNamespace(poll=lambda: 0, wait=lambda seconds: 0, stderr=[])
        record = {'passed': True, 'failures': []}
        with patch.object(runner.subprocess, 'Popen', side_effect=[first, second]) as launch, \
             patch.object(runner, 'snapshot', return_value={}):
            gate.desktop(record, 'chess', ['/Applications/Chess.app/Contents/MacOS/Chess'],
                         {'DPREFIX': str(self.root)}, evidence, self.root, 0, lambda: 0)
        self.assertEqual(launch.call_count, 2)
        self.assertEqual(len(record['attempts']), 2)
        self.assertTrue(record['passed'])

    def test_process_snapshot_excludes_other_prefix_and_addresses(self):
        proc = self.root / 'proc'
        proc.mkdir()
        prefix = self.root / 'prefix'
        prefix.mkdir()
        for pid, owner in [('100', prefix), ('200', self.root / 'other')]:
            path = proc / pid
            path.mkdir()
            (path / 'comm').write_text('fixture\n')
            (path / 'environ').write_bytes(('__mldr_sockpath=' + str(owner) + '/.darlingserver.sock').encode() + b'\0')
            (path / 'status').write_text('State:\tS\nPPid:\t1\nThreads:\t1\n')
            (path / 'exe').symlink_to('/our/mldr')
            (path / 'task' / pid).mkdir(parents=True)
            (path / 'task' / pid / 'wchan').write_text('futex_wait')
            (path / 'maps').write_text('dead-beef r-xp 0000 01:01 1 /our/Library.framework/Library\n')
        result = snapshot(prefix, proc)
        self.assertEqual([p['pid'] for p in result['processes']], [100])
        self.assertNotIn('dead-beef', json.dumps(result))

    def test_server_records_keep_only_fixed_numeric_lifecycle(self):
        path = self.root / 'private/var/log/dserver.log'
        path.parent.mkdir(parents=True)
        path.write_text('[1.0](process, Info) New process created with ID 10 and NSID 10\n'
                        '[2.0](kprintf, Info) arbitrary guest text\n'
                        '[3.0](process, Warning) Address 0xdead not found\n')
        result = server_records(self.root)
        self.assertEqual(len(result['lifecycle']), 1)
        self.assertNotIn('arbitrary guest text', json.dumps(result))
        self.assertNotIn('0xdead', json.dumps(result))


if __name__ == '__main__':
    unittest.main()
