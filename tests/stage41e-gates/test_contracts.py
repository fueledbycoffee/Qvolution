import itertools
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
from diagnostics import fatal_records

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
        probes = [{'exit': 0, 'eof': True, 'shell': v} for v in (False, False, True, True)]
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

    def test_terminal_startup_boundary_and_continuous_presence(self):
        def probes(values):
            return [dict(exit=0, eof=True, shell=v, when=100 + 10*i)
                    for i, v in enumerate(values)]
        for values in ((True, True, True), (False, True, True), (False, False, True, True)):
            self.assertTrue(shell_present(probes(values)))
        for values in ((False, False, False, True), (True, False, True), (False, True, False)):
            self.assertFalse(shell_present(probes(values)))
        for key, value in (('exit', 1), ('exit', None), ('eof', False)):
            for index in (0, 2):
                rows = probes((False, False, True, True))
                rows[index][key] = value
                self.assertFalse(shell_present(rows))
        result = runner.shell_observation(probes((False, False, True, True)), 100)
        self.assertEqual(result, dict(shell=True, shell_startup_allowance_probes=3,
                                     shell_first_probe=3, time_to_shell_seconds=20))
        self.assertIsNone(runner.shell_observation(probes((False, False)), 100)['time_to_shell_seconds'])

    def test_terminal_default_retires_inherited_to_explicit_diagnostic(self):
        args = SimpleNamespace(mode='terminal', stage=self.root, terminal=self.root / 'Terminal.app',
                               stdio_control_launcher=self.root / 'inherited')
        cases = list(runner.Runner(args).cases())
        self.assertEqual([c[0] for c in cases], ['terminal90-detached'])
        args.mode = 'terminal-inherited-diagnostic'
        diagnostic = list(runner.Runner(args).cases())
        self.assertEqual([c[0] for c in diagnostic], ['terminal90-inherited'])
        self.assertEqual(cases[0][1:3], diagnostic[0][1:3])
        self.assertEqual(cases[0][3], dict(desktop='terminal', seconds=90, launch_variant='detached'))
        self.assertEqual(diagnostic[0][3], dict(desktop='terminal', seconds=90, launch_variant='inherited'))

    def test_terminal_control_launches_and_probes_with_control_launcher(self):
        evidence = self.root / 'control-evidence'
        evidence.mkdir()
        control = self.root / 'inherited'
        gate = runner.Runner(SimpleNamespace(stage=self.root, launcher=self.root / 'detached'))
        app = SimpleNamespace(poll=lambda: None, stderr=SimpleNamespace(close=lambda: None))
        probe = SimpleNamespace(stdout=SimpleNamespace(fileno=lambda: 1, close=lambda: None), pid=123, poll=lambda: None)
        record = {'passed': True, 'failures': []}
        with patch.object(runner.subprocess, 'Popen', side_effect=[app, probe]) as launch, \
             patch.object(runner.os, 'readlink', return_value='pipe:[123]'), \
             patch.object(runner.time, 'monotonic', side_effect=itertools.count(100)), \
             patch.object(runner.time, 'sleep'), \
             patch.object(runner, 'capture_pipe', return_value=(b'', False)), \
             patch.object(runner, 'wait', return_value=0), \
             patch.object(runner, 'snapshot', return_value={'processes': []}), \
             patch.object(runner, 'chunks', return_value=iter(())):
            gate.desktop(record, 'terminal', ['/Applications/Terminal.app/Contents/MacOS/Terminal'],
                         {'DPREFIX': str(self.root)}, evidence, self.root, 90, lambda: 0, launcher=control)
        self.assertEqual([c.args[0][0] for c in launch.call_args_list], [str(control), str(control)])
        self.assertEqual(launch.call_args_list[-1].kwargs['stdin'], subprocess.DEVNULL)
        self.assertIn('shell_probe_timeout', [f['check'] for f in record['failures']])
        self.assertGreaterEqual(record['observed_seconds'], 90)
        self.assertEqual(len(json.loads((evidence / 'shell-probes-0.json').read_text())), 1)

    def test_terminal_exited_probe_without_eof_and_shutdown_failure_are_durable(self):
        work = self.root
        (work / 'p').mkdir()
        evidence = work / 'evidence'
        evidence.mkdir()
        args = SimpleNamespace(work=work, stage=work / 'stage', launcher=work / 'launcher',
                               stdio_control_launcher=work / 'control', evidence=evidence)
        gate = runner.Runner(args)
        init = SimpleNamespace(wait=lambda seconds: 0)
        app = SimpleNamespace(poll=lambda: None, stderr=SimpleNamespace(close=lambda: None))
        probe = SimpleNamespace(stdout=SimpleNamespace(fileno=lambda: 1, close=lambda: None), pid=123, poll=lambda: None)
        opened = []

        def popen(command, **kw):
            opened.append(command)
            if command[-1] == '/usr/bin/true':
                home = Path(kw['env']['DPREFIX']) / 'Users' / pwd.getpwuid(os.getuid()).pw_name
                home.mkdir(parents=True)
                return init
            if 'qv-shutdown' in command[0]:
                self.assertEqual(kw['env']['QV_REAL_LAUNCHER'], str(args.launcher))
                return SimpleNamespace()
            return probe if '/bin/ps' in command else app

        def run(command, **kw):
            return SimpleNamespace(returncode=0)

        with patch.object(runner.subprocess, 'Popen', side_effect=popen), \
             patch.object(runner.subprocess, 'run', side_effect=run), \
             patch.object(runner, 'wait', side_effect=[0, 0, 0, 1]), \
             patch.object(runner.os, 'readlink', return_value='pipe:[123]'), \
             patch.object(runner.time, 'monotonic', side_effect=itertools.count(100)), \
             patch.object(runner.time, 'sleep'), \
             patch.object(runner, 'capture_pipe', return_value=(b'', False)), \
             patch.object(runner, 'chunks', return_value=iter(())), \
             patch.object(runner, 'snapshot', return_value={'processes': []}), \
             patch.object(runner, 'server_records', return_value={}):
            with self.assertRaisesRegex(RuntimeError, 'cleanup failed'):
                gate.case('terminal90', ['/Applications/Stage41e/Terminal.app/Contents/MacOS/Terminal'], [], desktop='terminal', launch_variant='inherited')
        row = json.loads((evidence / 'results.json').read_text())[0]
        self.assertFalse(row['passed'])
        self.assertTrue(row['alive_at_end'])
        self.assertEqual(row['shutdown_exit'], 1)
        self.assertEqual(row['host_guard_exit'], 0)
        checks = [item['check'] for item in row['failures']]
        self.assertIn('shell_probe_timeout', checks)
        self.assertIn('shutdown', checks)
        self.assertTrue((evidence / 'terminal90/observation-0.json').is_file())
        self.assertEqual(opened[0][0], str(args.stdio_control_launcher))
        self.assertEqual(len(opened), 5)  # no new prefix or launch after cleanup failure

    def test_shutdown_adapter_keeps_our_launcher_error_and_exit(self):
        launcher = self.root / 'fake-launcher'
        launcher.write_text('#!/bin/sh\nprintf "fixture shutdown refusal\\n" >&2\nexit 7\n')
        launcher.chmod(0o755)
        log = self.root / 'shutdown.log'
        env = {**os.environ, 'QV_REAL_LAUNCHER': str(launcher), 'QV_SHUTDOWN_LOG': str(log)}
        result = subprocess.run([str(Path(__file__).with_name('shutdown-logged.py')), 'shutdown'], env=env)
        self.assertEqual(result.returncode, 7)
        self.assertEqual(log.read_text(), 'fixture shutdown refusal\n')

    def test_successful_shutdown_cannot_hide_duplicate_servers_or_survivors(self):
        for survive in (False, True):
            with self.subTest(survive=survive):
                evidence = self.root / ('survive' if survive else 'duplicate')
                evidence.mkdir()
                args = SimpleNamespace(work=self.root, stage=self.root / 'stage',
                                       launcher=self.root / 'launcher', evidence=evidence)
                gate = runner.Runner(args)
                process = SimpleNamespace(wait=lambda seconds: 0)
                rows = [{'processes': []}, {'processes': []},
                        {'processes': [{'pid': 101, 'comm': 'darlingserver'},
                                       {'pid': 102, 'comm': 'darlingserver'}]},
                        {'processes': [{'pid': 103, 'comm': 'mldr'}] if survive else []}]
                with patch.object(runner.subprocess, 'Popen', return_value=process), \
                     patch.object(runner.subprocess, 'run', return_value=SimpleNamespace(returncode=0)), \
                     patch.object(runner, 'wait', return_value=0), \
                     patch.object(runner, 'snapshot', side_effect=rows), \
                     patch.object(runner, 'server_records', return_value={}):
                    if survive:
                        with self.assertRaisesRegex(RuntimeError, 'cleanup failed'):
                            gate.case('authored', ['/Applications/authored'], [])
                    else:
                        gate.case('authored', ['/Applications/authored'], [])
                row = gate.rows[0]
                self.assertFalse(row['passed'])
                checks = [item['check'] for item in row['failures']]
                self.assertIn('duplicate_servers', checks)
                self.assertEqual('shutdown_survivors' in checks, survive)
                self.assertEqual(row['shutdown_exit'], int(survive))

    def test_desktop_shellspawn_timeout_retries_exactly_once(self):
        evidence = self.root / 'evidence'
        evidence.mkdir()
        gate = runner.Runner(SimpleNamespace(stage=self.root, launcher=self.root / 'launcher'))
        first = SimpleNamespace(poll=lambda: 1, wait=lambda seconds: 1,
                                stderr=SimpleNamespace(close=lambda: None))
        second = SimpleNamespace(poll=lambda: 0, wait=lambda seconds: 0, stderr=SimpleNamespace(close=lambda: None))
        record = {'passed': True, 'failures': []}
        with patch.object(runner.subprocess, 'Popen', side_effect=[first, second]) as launch, \
             patch.object(runner, 'chunks', side_effect=[iter([b'Timed out waiting for shellspawn in container\n']), iter([])]):
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
            (path / 'fd').mkdir()
            (path / 'fd/0').symlink_to('/dev/null')
            (path / 'fd/1').symlink_to('pipe:[123]')
            (path / 'stat').write_text(pid + ' (fixture (name)) S 1 100 100 0 0\n')
            (path / 'task' / pid).mkdir(parents=True)
            (path / 'task' / pid / 'wchan').write_text('futex_wait')
            (path / 'maps').write_text('dead-beef r-xp 0000 01:01 1 /our/Library.framework/Library\n')
        result = snapshot(prefix, proc)
        self.assertEqual([p['pid'] for p in result['processes']], [100])
        self.assertNotIn('dead-beef', json.dumps(result))
        self.assertEqual(result['processes'][0]['stdio'], {'0': '/dev/null', '1': 'pipe:[123]', '2': None})
        self.assertEqual(result['processes'][0]['session'], 100)
        self.assertEqual(result['processes'][0]['tty_device'], 0)

    def test_probe_aliases_include_high_fds_without_other_prefix(self):
        proc = self.root / 'proc'
        proc.mkdir()
        prefix = self.root / 'prefix'
        prefix.mkdir()
        for pid, owner in [('100', prefix), ('200', self.root / 'other')]:
            path = proc / pid
            path.mkdir()
            (path / 'comm').write_text('fixture\n')
            (path / 'environ').write_bytes(('__mldr_sockpath=' + str(owner) + '/.darlingserver.sock').encode()+b'\0')
            (path / 'status').write_text('State: S\n')
            (path / 'exe').symlink_to('/our/mldr')
            (path / 'fd').mkdir()
            (path / 'fdinfo').mkdir()
            (path / 'fd/17').symlink_to('pipe:[123]')
            (path / 'fdinfo/17').write_text('pos: 0\nflags: 02000001\n')
            (path / 'stat').write_text(pid+' (fixture) S 1 1 1 0 0\n')
            (path / 'task').mkdir()
            (path / 'maps').write_text('')
        result = snapshot(prefix, proc, pipe_targets=('pipe:[123]',))
        self.assertEqual([p['pid'] for p in result['processes']], [100])
        self.assertEqual(result['processes'][0]['probe_pipe_aliases'],
                         [dict(fd=17, target='pipe:[123]', flags='02000001')])

    def test_fatal_control_enables_only_safe_records_and_checks_emission(self):
        for emit in (True, False):
            evidence = self.root / ('emitted' if emit else 'missing')
            evidence.mkdir()
            args = SimpleNamespace(work=self.root, stage=self.root / 'stage',
                                   launcher=self.root / 'launcher', evidence=evidence)
            gate = runner.Runner(args)
            self.assertNotIn('QV_PREVIEW_FATAL', gate.base)
            def launch(command, **kw):
                prefix = Path(kw['env']['DPREFIX'])
                if command[-1] == '/usr/bin/true':
                    (prefix / 'private/var/log').mkdir(parents=True)
                if command[-1] == '/Applications/Stage41e/fatal-probe':
                    self.assertEqual(kw['env']['QV_PREVIEW_FATAL'], '1')
                    if emit:
                        path = prefix / 'private/var/log/qv-terminal-boundaries.log'
                        path.write_text('QV2 fatal.origin 1 61626f7274 666174616c2d70726f6265\n'
                                        'QV2 fatal.site 2 61626f7274 61626f7274\n')
                return SimpleNamespace(wait=lambda seconds: 0)
            with patch.object(runner.subprocess, 'Popen', side_effect=launch), \
                 patch.object(runner.subprocess, 'run', return_value=SimpleNamespace(returncode=0)), \
                 patch.object(runner, 'wait', side_effect=[0, 0, 134, 0]), \
                 patch.object(runner, 'snapshot', return_value={'processes': []}), \
                 patch.object(runner, 'server_records', return_value={}):
                gate.case('fatal-origin-control', ['/Applications/Stage41e/fatal-probe'], [],
                          expected=134, fatal_probe=True)
            self.assertEqual(gate.rows[-1]['passed'], emit)
            self.assertEqual(gate.rows[-1]['host_guard_exit'], 0)
            self.assertEqual('fatal_control' in [f['check'] for f in gate.rows[-1]['failures']], not emit)

    def test_fatal_records_accept_only_fixed_sites_and_image_basenames(self):
        self.assertFalse(fatal_records(self.root)['present'])
        path = self.root / 'private/var/log/qv-terminal-boundaries.log'
        path.parent.mkdir(parents=True)
        def row(label, kind, value):
            return f'QV2 {label} 123 {kind.encode().hex()} {value.encode().hex()}\n'
        path.write_text(row('fatal.origin', 'abort', 'fatal-probe') +
                        row('fatal.site', 'abort', 'abort') +
                        row('fatal.message', 'abort', 'arbitrary text') +
                        row('fatal.origin', 'abort', '/private/foreign') +
                        row('fatal.site', 'abort', 'foreign_function') +
                        row('fatal.origin', 'foreign-kind', 'Chess') +
                        row('fatal.origin', 'abort', '0xdeadbeef') +
                        'arbitrary guest text\n')
        result = fatal_records(self.root)
        self.assertEqual(len(result['records']), 2)
        self.assertEqual(result['rejected_records'], 4)
        self.assertNotIn('arbitrary', json.dumps(result))
        self.assertNotIn('foreign', json.dumps(result))
        linked = self.root / 'linked'
        (linked / 'private/var/log').mkdir(parents=True)
        (linked / 'private/var/log/qv-terminal-boundaries.log').symlink_to(path)
        with self.assertRaisesRegex(RuntimeError, 'symlink'):
            fatal_records(linked)

    def test_warning_counts_keep_source_names_without_message_bodies(self):
        path = self.root / 'private/var/log/dserver.log'
        path.parent.mkdir(parents=True)
        path.write_text('[1.0](calls, Warning)[P:10(10)][T:11(11)] TODO: virtual void DarlingServer::Call::PthreadCanceled::processCall()\n'
                        '[2.0](dtape, Warning) -1(-1):-1(-1): Trying to lock mutex without an active thread!\n'
                        '[3.0](dtape, Warning) -1(-1):-1(-1): stub: proc_get_effective_thread_policy:unimplemented flavor\n'
                        '[4.0](dtape, Warning) stub: thread_guard_violation\n'
                        '[5.0](dtape, Warning) stub: kmem_suballoc\n'
                        '[6.0](dtape, Warning) -1(-1):-1(-1): Trying to unlock mutex without an active thread!\n'
                        '[7.0](dtape, Warning) arbitrary 0xdeadbeef\n'
                        '[8.0](kprintf, Info) guest supplied text\n')
        result = server_records(self.root)
        self.assertEqual(sum(result['warning_calls'].values()), 6)
        self.assertEqual(result['warning_calls']['calls:PthreadCanceled:unimplemented'], 1)
        self.assertEqual(result['unclassified_warnings'], {'dtape': 1})
        self.assertNotIn('arbitrary', json.dumps(result))
        self.assertNotIn('deadbeef', json.dumps(result))
        self.assertNotIn('guest supplied', json.dumps(result))

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
