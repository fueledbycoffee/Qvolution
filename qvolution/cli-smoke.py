#!/usr/bin/env python3
"""Repeat CLI bootstrap/shutdown using only a staged, non-setuid runtime."""
import datetime as dt
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT.parent / (ROOT.name + '-work')
launcher = WORK / 'build/src/startup/darling'
stage = WORK / 'stage/usr/local'
prefix = WORK / 'prefixes/cli-smoke'


def main():
    if not launcher.is_file() or not (stage / 'libexec/darling').is_dir():
        raise RuntimeError('build and stage the runtime before running the CLI smoke test')
    clean = subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT,
                           capture_output=True, text=True, check=True)
    if clean.stdout:
        raise RuntimeError('record/commit intended source changes before testing a staged runtime')
    env = os.environ.copy()
    env.pop('DARLING_PROFILE', None)
    env.update(DPREFIX=str(prefix), DARLING_INSTALL_PREFIX=str(stage), DARLING_NONROOT='1')
    WORK.mkdir(exist_ok=True)
    records = []
    for attempt in range(1, 4):
        marker = 'QVOLUTION_CLI_' + uuid.uuid4().hex
        commands = [([str(launcher), 'shell', '/bin/echo', marker], 45),
                    ([str(launcher), 'shutdown'], 30)]
        for index, (command, deadline) in enumerate(commands):
            try:
                run = subprocess.run(command, env=env, text=True, capture_output=True,
                                     timeout=deadline)
                passed = run.returncode == 0 and (index == 1 or marker in run.stdout.splitlines())
                record = {'attempt': attempt, 'kind': 'launch' if index == 0 else 'shutdown',
                          'command': command, 'exit_code': run.returncode,
                          'stdout': run.stdout, 'stderr': run.stderr, 'passed': passed}
            except subprocess.TimeoutExpired as exc:
                record = {'attempt': attempt, 'kind': 'launch' if index == 0 else 'shutdown',
                          'command': command, 'passed': False, 'error': 'deadline exceeded',
                          'stdout': (exc.stdout or b'').decode(errors='replace'),
                          'stderr': (exc.stderr or b'').decode(errors='replace')}
            records.append(record)
            print(f"attempt {attempt} {record['kind']}: {'pass' if record['passed'] else 'fail'}", flush=True)
        # Always attempt the prefix-scoped shutdown, including after failed launch.
        if not all(r['passed'] for r in records[-2:]):
            break
    report = {'created_utc': dt.datetime.now(dt.timezone.utc).isoformat(),
              'scope': 'three nonroot CLI boot/shutdown cycles; no GUI claim',
              'prefix': str(prefix), 'records': records,
              'passed': len(records) == 6 and all(r['passed'] for r in records)}
    path = WORK / 'cli-smoke.json'
    path.write_text(json.dumps(report, indent=2) + '\n')
    print('Evidence: ' + str(path))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError) as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
