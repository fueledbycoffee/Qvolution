#!/usr/bin/env python3
"""gpu-present guest runs on L/stage (hardlink overlay of stage41c3), one fresh prefix per invocation.
Run correctness under `qv-slot guest`. For timing use --timing without an outer
slot: initialization/shutdown take guest slots; only each measurement takes perf.

  run.py TAG JOB [JOB...]
  JOB = label|renderer|budget-seconds|command words separated by spaces[|ENV=VALUE,...]
  renderer is shm, egl or egl-verify (egl plus DARLING_APPKIT_EGL_VERIFY=1).
  A command word starting with @ names a lane fixture binary (copied into /Applications);
  a word starting with / is used as a guest path.

Each job's stdout/stderr go to runs/TAG-<time>/<label>.out|.err, a summary to summary.json.
Containers stop only through `darling shutdown` on this run's exact prefix; nothing is
signalled. Host folders are snapshotted before and verified after (qv-host-guard).
"""
from pathlib import Path
import datetime, hashlib, json, os, re, shutil, subprocess, sys, time, uuid
from gate_checks import problems

W = Path('/home/sean/Documents/Codex/Projects/Qvolution-work')
L = W / 'parallel/gpu-present'
STAGE = (L / 'stage').resolve()
LAUNCHER = W / 'parallel/stage41c/build/src/startup/darling'
timing = '--timing' in sys.argv
if timing: sys.argv.remove('--timing')
assert not os.environ.get('QV_SLOT') if timing else os.environ.get('QV_SLOT', '').startswith('guest-'), 'timing has no outer slot; correctness requires qv-slot guest'

def launch_command(args, kind='guest'):
    command = [str(LAUNCHER), *args]
    return [str(W / 'dashboard/qv-slot'), kind, *command] if timing else command
assert not os.stat(LAUNCHER).st_mode & 0o4000, 'launcher must be non-setuid'
manifest = json.loads((STAGE / 'qvolution-runtime-source.json').read_text())
assert manifest['stage_label'] == 'gpu-present'
fixture_receipt = json.loads((L / 'out/fixtures.json').read_text())
assert manifest['built_heads'] == fixture_receipt['heads'], 'stale fixtures'
for repo, head in manifest['built_heads'].items():
    assert subprocess.check_output(['git','-C',str(L/repo),'rev-parse','HEAD'],text=True).strip() == head, 'stale stage'
    assert not subprocess.check_output(['git','-C',str(L/repo),'status','--porcelain','--untracked-files=no'],text=True).strip(), 'uncommitted source'
fixtures = fixture_receipt['fixtures']
for f in fixtures.values():
    assert hashlib.sha256(Path(f['binary']).read_bytes()).hexdigest() == f['sha256'], 'fixture hash mismatch' 
tag, jobs = sys.argv[1], sys.argv[2:]
assert re.fullmatch(r'[a-zA-Z0-9_-]+', tag), 'safe run tag required'
run = L / 'runs' / (tag + '-' + datetime.datetime.now().strftime('%m%d%H%M%S') + '-' + uuid.uuid4().hex[:6])
run.mkdir(parents=True)
prefix = W / 'p' / ('gp-' + uuid.uuid4().hex[:8])
assert len(os.fsencode(prefix)) <= 83 and not prefix.exists()
env = {k: v for k, v in os.environ.items() if not k.startswith(('DYLD_', 'DARLING_', '__mldr_')) and k != 'DPREFIX'}
env.update(DPREFIX=str(prefix), DARLING_INSTALL_PREFIX=str(STAGE / 'usr/local'), DARLING_NONROOT='1',
           DARLING_APPKIT_BACKEND='wayland', WAYLAND_DISPLAY=os.environ.get('WAYLAND_DISPLAY', 'wayland-0'))


def shutdown():
    command = [str(W / 'dashboard/qv-shutdown'), str(LAUNCHER)]
    if timing:
        command = [str(W / 'dashboard/qv-slot'), 'guest', *command]
    r = subprocess.run(command, env=env, capture_output=True, text=True)
    (run / ('shutdown-helper-%s.json' % uuid.uuid4().hex[:8])).write_text(json.dumps(dict(exit=r.returncode, stdout=r.stdout, stderr=r.stderr), indent=1)+'\n')
    return r.returncode


def boot():
    for _ in range(2):
        r = subprocess.run(launch_command(['shell', '/usr/bin/true']), env=env, capture_output=True, text=True)
        (run / ('boot-attempt-%d-%s.json' % (_, uuid.uuid4().hex[:6]))).write_text(json.dumps(dict(exit=r.returncode, stdout=r.stdout, stderr=r.stderr), indent=1)+'\n')
        if r.returncode == 0:
            return
        shutdown()
    raise SystemExit('prefix initialization failed: ' + r.stderr[-400:])


def compositor_cpu():
    """CPU seconds used so far by the compositor (gnome-shell), or None."""
    for proc in Path('/proc').iterdir():
        try:
            if proc.name.isdigit() and (proc / 'comm').read_text().strip() == 'gnome-shell' and \
                    proc.stat().st_uid == os.getuid():
                fields = (proc / 'stat').read_text().rsplit(')', 1)[1].split()
                return (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')
        except (FileNotFoundError, PermissionError, ProcessLookupError, IndexError):
            continue
    return None


guard = run / 'host-guard.txt'
subprocess.run([str(W / 'dashboard/qv-host-guard'), 'snapshot', str(guard)], check=True)
rows = []
failed = False
try:
    boot()
    assert shutdown() == 0, 'prefix must be stopped before copying fixtures'
    (prefix / 'Applications').mkdir(exist_ok=True)
    for name, f in fixtures.items():
        subprocess.run(['cp', '-a', f['binary'], str(prefix / 'Applications' / name)], check=True)
    if timing: boot()  # Leave initialized container alive before acquiring perf.
    for job in jobs:
        parts = job.split('|')
        label, renderer, budget, command = parts[0], parts[1], float(parts[2]), parts[3].split()
        extra = dict(kv.split('=', 1) for kv in parts[4].split(',')) if len(parts) > 4 and parts[4] else {}
        expected_exit = int(extra.pop('QV_RUN_EXPECT_EXIT', '0'))
        required = extra.pop('QV_RUN_REQUIRE', '')
        assert re.fullmatch(r'[a-zA-Z0-9_-]+', label), 'safe job label required'
        assert renderer in ('shm','egl','egl-verify'), 'unknown renderer'
        argv = ['/Applications/' + w[1:] if w.startswith('@') else w for w in command]
        # Variables from an earlier `darling shell` in the same container reached later
        # guests (smoke-1101105026), so every job states each one it depends on.
        jenv = dict(env, DARLING_APPKIT_RENDERER='egl' if renderer.startswith('egl') else 'shm',
                    DARLING_APPKIT_EGL_VERIFY='1' if renderer == 'egl-verify' else '0',
                    DARLING_APPKIT_EGL_TEST_CORRUPT='0',
                    DARLING_APPKIT_PRESENT_TIMING='1' if timing else '0',
                    DARLING_APPKIT_DAMAGE_VERIFY='0' if timing else '1',
                    QV_BENCH_EXPECT_RENDERER='egl' if renderer.startswith('egl') else 'shm')
        jenv.update(extra)
        load0, comp0, t0 = Path('/proc/loadavg').read_text().split()[:3], compositor_cpu(), time.monotonic()
        attempts = 0
        while True:
            attempts += 1
            out = open(run / (label + '.attempt%d.out' % attempts), 'w+')
            err = open(run / (label + '.attempt%d.err' % attempts), 'w+')
            command = [str(LAUNCHER), 'shell', *argv]
            if timing:
                command = [str(W/'dashboard/qv-slot'), 'perf', sys.executable, str(L/'measure.py'),
                           str(run/(label+'.measurement.json')), str(budget), *command]
            p = subprocess.Popen(command, env=jenv, stdout=out, stderr=err, text=True)
            expired = False
            while p.poll() is None:
                if not timing and time.monotonic() - t0 > budget:
                    expired = True
                    shutdown()  # ends the job without signalling it
                    p.wait()
                    break
                time.sleep(0.25)
            if timing and (run/(label+'.measurement.json')).exists():
                expired = json.loads((run/(label+'.measurement.json')).read_text())['timeout']
            err.flush(); err.seek(0)
            if attempts == 1 and not expired and 'Timed out waiting for shellspawn' in err.read():
                shutdown(); out.close(); err.close()
                if timing: boot()
                continue
            break
        seconds, comp1 = time.monotonic() - t0, compositor_cpu()
        out.seek(0); text = out.read(); err.seek(0); errtext = err.read(); out.close(); err.close()
        shutil.copyfile(run/(label+'.attempt%d.out' % attempts), run/(label+'.out'))
        shutil.copyfile(run/(label+'.attempt%d.err' % attempts), run/(label+'.err'))
        results = [json.loads(m) for m in re.findall(r'^RESULT (\{.*\})$', text, re.M)]
        row = dict(label=label, renderer=renderer, argv=argv, env=extra, exit=p.returncode, timeout=expired,
                   seconds=round(seconds, 2), loadavg_before=load0, loadavg_after=Path('/proc/loadavg').read_text().split()[:3],
                   compositor_cpu_seconds=None if comp0 is None or comp1 is None else round(comp1 - comp0, 3),
                   results=results, result_lines=re.findall(r'^RESULT .*$', text, re.M),
                   fail_lines=[l for l in (text + errtext).splitlines() if l.startswith('FAIL')][:20],
                   egl_lines=[l for l in errtext.splitlines() if 'AppKit EGL' in l or 'OpenGL:' in l][:20])
        row['problems'] = problems(row, text, errtext, expected_exit, required)
        row['passed'] = not row['problems']
        failed |= not row['passed']
        if expired:
            boot()
            if not timing: shutdown()
        rows.append(row)
        (run / 'summary.json').write_text(json.dumps(dict(stage=str(STAGE), prefix=str(prefix), heads=manifest.get('built_heads'), rows=rows), indent=1) + '\n')
        print(json.dumps({k: row[k] for k in ('label', 'renderer', 'exit', 'timeout', 'seconds', 'loadavg_before', 'compositor_cpu_seconds', 'passed', 'problems')}),
              *(row['result_lines'] or row['fail_lines'][:3]), sep='\n', flush=True)
finally:
    rc = shutdown()
    g = subprocess.run([str(W / 'dashboard/qv-host-guard'), 'verify', str(guard)], capture_output=True, text=True)
    (run / 'final.json').write_text(json.dumps(dict(shutdown_exit=rc, host_guard_exit=g.returncode, host_guard=g.stderr), indent=1) + '\n')
    print('evidence', run, 'shutdown', rc, 'host-guard', g.returncode, flush=True)
    failed |= rc != 0 or g.returncode != 0
    if g.returncode:
        raise SystemExit('host folders changed: ' + g.stderr)

raise SystemExit(1 if failed else 0)
