#!/usr/bin/env python3
"""Guarded Foundation, Terminal90 and Chess rerun on an immutable Stage41e overlay."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import pwd
import re
import subprocess
import threading
import time
import uuid

from contracts import (FOUNDATION_COMPLETIONS, check_terminal, fail, payload_root,
                       prepare_foundation_directory, shell_present)
from processes import server_records, snapshot


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value, message):
    if not value:
        raise RuntimeError(message)


def wait(process, seconds):
    deadline = time.monotonic() + seconds
    while process.poll() is None and time.monotonic() < deadline:
        time.sleep(.1)
    return process.poll()


class Runner:
    def __init__(self, args):
        self.args = args
        self.rows = []
        self.base = {k: v for k, v in os.environ.items()
                     if not k.startswith(('DYLD_', 'DARLING_', 'DSERVER_', '__mldr_'))
                     and k not in ('DPREFIX', 'WAYLAND_DEBUG')}
        self.base.update(DARLING_ISOLATED_HOME='1', DARLING_NONROOT='1',
                         DARLING_INSTALL_PREFIX=str(args.stage / 'usr/local'),
                         DARLING_APPKIT_BACKEND='wayland', DARLING_APPKIT_RENDERER='shm',
                         DSERVER_LOG_LEVEL='info', DSERVER_LOG_STDERR='0',
                         WAYLAND_DISPLAY=os.environ.get('WAYLAND_DISPLAY', 'wayland-0'))

    def preflight(self):
        a = self.args
        manifest = json.loads((a.stage / 'qvolution-runtime-source.json').read_text())
        head = subprocess.check_output(['git', '-C', str(a.source), 'rev-parse', 'HEAD'], text=True).strip()
        require(head == manifest['built_superproject'], 'source HEAD differs from installed runtime')
        require(not a.launcher.stat().st_mode & 0o6000, 'launcher must be non-setuid')
        require(sha(a.launcher) == manifest['build_tools']['launcher']['sha256'], 'launcher hash')
        for rel, digest in manifest['fingerprints'].items():
            require(sha(a.stage / rel) == digest, 'runtime fingerprint: ' + rel)
        for rel, mode in manifest['directory_modes'].items():
            require((a.stage / rel).stat().st_mode & 0o7777 == int(mode, 8), 'directory mode: ' + rel)
        receipts = json.loads((a.fixtures / 'receipts.json').read_text())
        for row in receipts.values():
            for key in ('source', 'binary'):
                require(sha(row[key]) == row[key + '_sha256'], 'fixture ' + key + ': ' + row[key])
        inputs = [a.work / 'dashboard/qv-host-guard', a.work / 'dashboard/qv-shutdown',
                  a.stage / 'qvolution-runtime-source.json', a.fixtures / 'receipts.json',
                  Path(__file__), Path(__file__).with_name('contracts.py'),
                  Path(__file__).with_name('processes.py'), Path(__file__).with_name('shutdown-logged.py')]
        for label, command, files, options in self.cases():
            inputs.extend(Path(item[0] if isinstance(item, tuple) else item) for item in files)
        hashes = {}
        for path in inputs:
            # Hashing guest app executables is allowed; never parse them or their output.
            binary = path / 'Contents/MacOS' / path.stem if path.is_dir() else path
            require(binary.is_file(), 'missing input: ' + str(binary))
            hashes[str(binary)] = sha(binary)
        if a.input_lock:
            require(hashes == json.loads(a.input_lock.read_text()), 'input hashes differ from the reviewed dry run')
        report = dict(runtime_manifest_sha256=sha(a.stage / 'qvolution-runtime-source.json'),
                      runtime_fingerprints=len(manifest['fingerprints']), inputs=hashes,
                      cases=[{'label': c[0], 'command': c[1], **c[3]} for c in self.cases()])
        (a.evidence / 'preflight.json').write_text(json.dumps(report, indent=2) + '\n')
        print('PASS preflight:', len(report['cases']), 'cases;', len(hashes), 'inputs;',
              len(manifest['fingerprints']), 'runtime hashes', flush=True)

    def cases(self):
        a = self.args
        guest = '/Applications/Stage41e/'
        if a.mode in ('all', 'foundation'):
            source = a.source.parent / 'build/src/external/foundation/test/imports/foundation_import_exports'
            manifests = [(a.source / 'src/external/foundation/test/imports/strong-exports.txt', 'Foundation.exports'),
                         (a.source / 'src/external/swift/tests/foundation-imports/exports.txt', 'SwiftFoundation.exports'),
                         (a.source / 'src/external/swift/tests/darwin-foundation-imports/exports.txt', 'Darwin.exports')]
            yield 'full-strong', [guest + source.name,
                  '/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation', guest + 'Foundation.exports',
                  '/usr/lib/swift/libswiftFoundation.dylib', guest + 'SwiftFoundation.exports',
                  '/usr/lib/swift/libswift_DarwinFoundation1.dylib', guest + 'Darwin.exports'], [source, *manifests], {
                      'required': r'^exports checked=1415 failures=0$'}
            for name, pattern in FOUNDATION_COMPLETIONS.items():
                yield name, [guest + name], [a.fixtures / name], {'required': pattern}
            yield 'stub-forwarding', [guest + 'stub-forwarding', payload_root(a.stage)], [a.fixtures / 'stub-forwarding'], {
                'required': r'^PASS guest typed unknown-selector exception paths$'}
            for name, expected in [('foundation_import_strong31_values', 0), ('foundation_import_extension_unavailable', 1),
                                   ('foundation_import_string_encodings', 0), ('foundation_import_lines_directories', 0)]:
                source = a.source.parent / 'build/src/external/foundation/test/imports' / name
                yield name, [guest + name], [source], {'expected': expected, 'required':
                    'NSExtensionMain: extension host lifecycle is unavailable' if expected else None}
        if a.mode in ('all', 'terminal'):
            yield 'terminal90', [guest + 'Terminal.app/Contents/MacOS/Terminal'], [a.terminal], {'desktop': 'terminal', 'seconds': 90}
        if a.mode in ('all', 'chess'):
            yield 'chess-dyld', [guest + 'Chess.app/Contents/MacOS/Chess'], [a.chess], {'desktop': 'chess', 'seconds': 20}

    def case(self, label, command, files, *, expected=0, required=None, seconds=90, desktop=None):
        a = self.args
        prefix = a.work / 'p' / ('s41eg-' + uuid.uuid4().hex[:8])
        require(not prefix.exists() and len(os.fsencode(prefix)) <= 83, 'invalid fresh prefix')
        evidence = a.evidence / label
        evidence.mkdir()
        env = {**self.base, 'DPREFIX': str(prefix)}
        record = dict(label=label, command=command, prefix=str(prefix), passed=True, failures=[], shutdowns=[])
        used = False
        shutdown_failed = False

        def save(name, data):
            (evidence / name).write_text(json.dumps(data, indent=2) + '\n')

        def stop():
            nonlocal shutdown_failed
            number = len(record['shutdowns'])
            save(f'processes-before-shutdown-{number}.json', snapshot(prefix))
            # The helper still owns all three retries. Its launcher wrapper retains
            # only OUR shutdown output, which qv-shutdown otherwise discards.
            rc = subprocess.run([str(a.work / 'dashboard/qv-shutdown'),
                                 str(Path(__file__).with_name('shutdown-logged.py'))],
                                env={**env, 'QV_REAL_LAUNCHER': str(a.launcher),
                                     'QV_SHUTDOWN_LOG': str(evidence / f'shutdown-{number}.log')},
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
            record['shutdowns'].append(rc)
            save(f'processes-after-shutdown-{number}.json', snapshot(prefix))
            shutdown_failed |= rc != 0
            if rc:
                fail(record, 'shutdown', f'qv-shutdown exit {rc}; see shutdown-{number}.log')
            return rc

        subprocess.run([str(a.work / 'dashboard/qv-host-guard'), 'snapshot', str(evidence / 'host-before.json')], check=True)
        try:
            used = True
            for attempt in range(2):
                with (evidence / f'init-{attempt}.log').open('w') as log:
                    init = subprocess.Popen([str(a.launcher), 'shell', '/usr/bin/true'], env=env,
                                            stdout=log, stderr=subprocess.STDOUT)
                    rc = wait(init, 75)
                require(stop() == 0, 'initialization shutdown failed')
                init.wait(30)
                if rc == 0:
                    break
            require(rc == 0, 'initialization failed twice')
            dest = prefix / 'Applications/Stage41e'
            dest.mkdir(parents=True, exist_ok=True)
            for item in files:
                src, name = item if isinstance(item, tuple) else (item, Path(item).name)
                subprocess.run(['cp', '-a', str(src), str(dest / name)], check=True)
            if label == 'strong31':
                record['work_directory'] = str(prepare_foundation_directory(prefix))
            if desktop:
                if desktop == 'terminal':
                    home = prefix / 'Users' / pwd.getpwuid(os.getuid()).pw_name
                    require(home.is_dir() and not home.is_symlink(), 'isolated home missing')
                    profile = home / '.bash_profile'
                    require(not profile.exists() and not profile.is_symlink(), 'profile already exists')
                    profile.write_text("printf '%s\\n' qv-stage41e-shell-executed > /Applications/Stage41e/shell-marker\n")
                self.desktop(record, desktop, command, env, evidence, dest, seconds, stop)
            else:
                for attempt in range(2):
                    logpath = evidence / f'guest-{attempt}.log'
                    with logpath.open('w') as log:
                        process = subprocess.Popen([str(a.launcher), 'shell', *command], env=env,
                                                   stdout=log, stderr=subprocess.STDOUT)
                        rc = wait(process, seconds)
                    require(rc is not None, 'authored fixture exceeded its guest budget')
                    output = logpath.read_text(errors='replace')
                    if attempt == 0 and 'Timed out waiting for shellspawn in container' in output:
                        require(stop() == 0, 'shellspawn retry shutdown failed')
                        continue
                    break
                record.update(exit=rc, expected=expected, required=required)
                if rc != expected:
                    fail(record, 'exit', f'expected {expected}, got {rc}')
                if required and not re.search(required, output, re.M):
                    fail(record, 'completion', 'missing exact fixture completion: ' + required)
        except Exception as error:
            fail(record, 'exception', f'{type(error).__name__}: {error}')
        finally:
            if used and not shutdown_failed:
                stop()
            save('server-records.json', server_records(prefix))
            with (evidence / 'host-verify.log').open('w') as log:
                guard = subprocess.run([str(a.work / 'dashboard/qv-host-guard'), 'verify', str(evidence / 'host-before.json')],
                                       stdout=log, stderr=subprocess.STDOUT).returncode
            record['shutdown_exit'] = record['shutdowns'][-1] if record['shutdowns'] else None
            record['host_guard_exit'] = guard
            if guard:
                fail(record, 'host_guard', f'exit {guard}; see host-verify.log')
            self.rows.append(record)
            (a.evidence / 'results.json').write_text(json.dumps(self.rows, indent=2) + '\n')
            print(label, 'PASS' if record['passed'] else 'FAIL', json.dumps(record['failures']), flush=True)
        require(not shutdown_failed and not guard, 'cleanup failed; no further prefixes will be started')

    def desktop(self, record, kind, command, env, evidence, dest, seconds, stop):
        pattern = re.compile(rb'(xdg_surface|xdg_toplevel|wl_surface)[@#][0-9]+\.(get_toplevel|configure|attach|commit)\(')
        record['attempts'] = []
        for attempt in range(2):
            tokens = set()
            startup_timeout = []
            app = subprocess.Popen([str(self.args.launcher), 'shell', *command], env={**env, 'WAYLAND_DEBUG': 'client'},
                                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            def capture():
                with (evidence / f'window-protocol-{attempt}.txt').open('wb') as protocol:
                    # Keep protocol tokens only; never retain or display app text.
                    for raw in app.stderr:
                        if b'Timed out waiting for shellspawn in container' in raw:
                            startup_timeout.append(True)
                        for match in pattern.finditer(raw):
                            token = match[1] + b'.' + match[2]
                            tokens.add(token)
                            protocol.write(token + b'\n')
                            protocol.flush()
            thread = threading.Thread(target=capture, daemon=True)
            thread.start()
            start = time.monotonic()
            probes = []
            try:
                while time.monotonic() - start < seconds and app.poll() is None:
                    if kind == 'terminal' and (not probes or time.monotonic() - probes[-1]['when'] >= 10):
                        probe = subprocess.Popen([str(self.args.launcher), 'shell', '/bin/ps', '-axo', 'comm='],
                                                 env=env, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
                        rc = wait(probe, 10)
                        probes.append({'when': time.monotonic(), 'exit': rc, 'shell': False})
                        if rc is None:
                            fail(record, 'shell_probe_timeout', 'prefix ps did not complete in 10 seconds')
                            break
                        names = probe.stdout.read().decode('utf8', 'replace').splitlines()
                        probes[-1]['shell'] = any(re.fullmatch(r'-?bash', Path(n.strip()).name) for n in names)
                        (evidence / f'shell-probes-{attempt}.json').write_text(json.dumps(probes, indent=2) + '\n')
                    time.sleep(.2)
            finally:
                observation = dict(alive_at_end=app.poll() is None, exit_before_shutdown=app.poll(),
                                   observed_seconds=time.monotonic() - start,
                                   window=all(t in tokens for t in (b'xdg_surface.get_toplevel', b'xdg_toplevel.configure',
                                                                   b'wl_surface.attach', b'wl_surface.commit')))
                marker = dest / 'shell-marker'
                if kind == 'terminal':
                    observation.update(shell=shell_present(probes), shell_executed=marker.is_file() and
                                       marker.read_text().strip() == 'qv-stage41e-shell-executed')
                record.update(observation)
                record['attempts'].append(observation)
                (evidence / f'shell-probes-{attempt}.json').write_text(json.dumps(probes, indent=2) + '\n')
                (evidence / f'observation-{attempt}.json').write_text(json.dumps(observation, indent=2) + '\n')
                # Observations are durable even when shutdown fails or a wait raises.
                stopped = stop() == 0
                if stopped:
                    app.wait(30)
                    if kind == 'terminal' and probes and probes[-1]['exit'] is None:
                        probe.wait(30)
                    thread.join(30)
                    require(not thread.is_alive(), 'protocol drain did not exit after shutdown')
            record['startup_timeout'] = bool(startup_timeout)
            if startup_timeout and attempt == 0 and stopped:
                continue
            if kind == 'terminal':
                check_terminal(record, seconds)
            elif not record['alive_at_end'] and record['exit_before_shutdown'] != 0:
                fail(record, 'guest_exit', f'Chess exited {record["exit_before_shutdown"]} before 20 seconds')
            break


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('work', 'source', 'stage', 'fixtures', 'launcher', 'terminal', 'chess', 'evidence'):
        p.add_argument('--' + name, required=True, type=Path)
    p.add_argument('--mode', choices=('all', 'foundation', 'terminal', 'chess'), default='all')
    p.add_argument('--input-lock', type=Path, help='require the exact input hashes approved by the native dry run')
    p.add_argument('--dry-run-test-only', action='store_true', help='validate all inputs and stop at the first boot boundary')
    args = p.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=False)
    runner = Runner(args)
    runner.preflight()
    if args.dry_run_test_only:
        print('TEST ONLY: stopped immediately before qv-slot guest/container initialization; no prefix created')
        return 0
    require(os.environ.get('QV_SLOT', '').startswith('guest-'), 'run through qv-slot guest')
    for label, command, files, options in runner.cases():
        runner.case(label, command, files, **options)
    return int(any(not row['passed'] for row in runner.rows))


if __name__ == '__main__':
    raise SystemExit(main())
