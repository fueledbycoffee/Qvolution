#!/usr/bin/env python3
"""Run the gpu-present gates on the host with Darling/GPU access.
Usage: /home/sean/Documents/Codex/Projects/Qvolution-work/parallel/gpu-present/run-gates.py
Requires the committed heads already built/staged with build.py build|stage|fixtures.
No cleanup/deletion, signals, network access, shared prefixes or nested slots.
"""
from pathlib import Path
import datetime, hashlib, json, os, re, subprocess, sys, uuid
W=Path('/home/sean/Documents/Codex/Projects/Qvolution-work'); L=W/'parallel/gpu-present'
assert not os.environ.get('QV_SLOT'), 'runner acquires its own slots; do not wrap it'
R=L/'runs'/('codex-gates-'+datetime.datetime.now().strftime('%m%d%H%M%S')+'-'+uuid.uuid4().hex[:6]);R.mkdir()
rows=[]
manifest=json.loads(((L/'stage').resolve()/'qvolution-runtime-source.json').read_text())
fixtures=json.loads((L/'out/fixtures.json').read_text())
assert manifest['built_heads']==fixtures['heads'], 'build/stage/fixtures receipts differ'
for repo,head in manifest['built_heads'].items():
    assert subprocess.check_output(['git','-C',str(L/repo),'rev-parse','HEAD'],text=True).strip()==head, 'rebuild current pin before gates'
(R/'source-heads.json').write_text(json.dumps(manifest['built_heads'],indent=2)+'\n')
guard=R/'host-guard.txt'
subprocess.run([str(W/'dashboard/qv-host-guard'),'snapshot',str(guard)],check=True)

def run(tag, jobs, timing=False):
    command=[sys.executable,str(L/'run.py')]
    if timing: command+=['--timing']
    command += [tag,*jobs]
    if not timing: command=[str(W/'dashboard/qv-slot'),'guest',*command]
    with (R/(tag+'.log')).open('x') as log:
        result=subprocess.run(command,cwd=L,stdout=log,stderr=subprocess.STDOUT)
    output=(R/(tag+'.log')).read_text()
    match=re.search(r'^evidence (.+) shutdown ',output,re.M)
    evidence=Path(match.group(1)) if match else None
    rows.append(dict(tag=tag,exit=result.returncode,command=command,evidence=str(evidence) if evidence else None))
    (R/'summary.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(tag,'exit',result.returncode,'evidence',evidence,flush=True)
    if evidence and (evidence/'final.json').exists() and json.loads((evidence/'final.json').read_text())['host_guard_exit']:
        raise RuntimeError('host folders changed; stopping further guests; see '+str(evidence))
    return evidence

def jobs(which,renderer):
    return subprocess.check_output([sys.executable,str(L/'jobs.py'),which,renderer],text=True).splitlines()

def compare(name,left,right):
    a={p.name:p for p in left.glob('*.bgra')}; b={p.name:p for p in right.glob('*.bgra')}
    ok=bool(a) and set(a)==set(b) and all(a[k].read_bytes()==b[k].read_bytes() for k in a)
    receipt=dict(gate=name,passed=ok,left={k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in a.items()},
                 right={k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in b.items()})
    (R/(name+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
    rows.append(dict(tag=name,exit=0 if ok else 1,evidence=str(R/(name+'.json'))))
    print(name,'PASS' if ok else 'FAIL',flush=True)

try:
    # run.py checks committed heads, staged build receipt and every fixture hash.
    for renderer in ('shm','egl-verify'):
        run('codex-te-'+renderer,jobs('textedit',renderer))
        run('codex-theme-'+renderer,jobs('theme',renderer))
    run('codex-pixels',[
        'egl-presentation|egl-verify|90|@appkit-egl-presentation',
        'full-window|egl-verify|90|@appkit-gpu-present-bench present 100|QV_RUN_REQUIRE=^RESULT ',
        'small-damage|egl-verify|90|@appkit-gpu-present-bench damage 100|QV_RUN_REQUIRE=^RESULT ',
        'scroll|egl-verify|90|@appkit-gpu-present-bench scroll 100|QV_RUN_REQUIRE=^RESULT ',
        'layer-tree|egl-verify|90|@quartzcore-layer-gpu-composite 120|QV_RUN_REQUIRE=^RESULT quartzcore-layer-gpu-composite failures=0'])
    # Keep corruption and missing-EGL controls in their own prefix: guest
    # environment variables can persist across shell calls in one container.
    run('codex-negative',['negative|egl-verify|90|@appkit-egl-presentation --negative-control|QV_RUN_EXPECT_EXIT=1,QV_RUN_REQUIRE=reference mismatch [1-9][0-9]*/'])
    run('codex-fallback',['fallback|egl|90|@appkit-gpu-present-bench present 30|__EGL_VENDOR_LIBRARY_FILENAMES=/nonexistent,QV_BENCH_EXPECT_RENDERER=shm,QV_RUN_REQUIRE=^RESULT '])
    for fixture,variable,args in [('appkit-damage-redraw','DAMAGE_EVIDENCE_DIR',' verify'),
                                  ('appkit-theme-gpu-frame','QV_THEME_EVIDENCE',''),
                                  ('appkit-layer-backed-composite','QV_LAYER_EVIDENCE',' 120')]:
        captures={}
        for renderer in ('shm','egl-verify'):
            capture=R/(fixture+'-'+renderer);capture.mkdir();captures[renderer]=capture
            guest_dir='/Volumes/SystemRoot'+str(capture)
            run('codex-'+fixture+'-'+renderer,[f'{fixture}|{renderer}|90|@{fixture}{args}|{variable}={guest_dir},QV_RUN_REQUIRE=^RESULT {fixture}.*failures=0'])
        compare(fixture+'-pixels-vs-shm',captures['shm'],captures['egl-verify'])
    run('codex-soak',[f'soak-{r}|{r}|150|@appkit-gpu-present-bench soak 1000|QV_RUN_REQUIRE=^RESULT ' for r in ('shm','egl')])
    # Each measurement is ~5s, at most 90s, after boot/copy under guest.
    # measure.py records /proc/loadavg AFTER perf acquisition and at completion.
    timing=[]
    for renderer in ('shm','egl'):
        for mode in ('present','damage'):
            timing.append(f'{mode}-{renderer}|{renderer}|90|@appkit-gpu-present-bench {mode} 300|QV_RUN_REQUIRE=^RESULT ')
        timing.append(f'layers-{renderer}|{renderer}|90|@quartzcore-layer-gpu-composite 300|QV_RUN_REQUIRE=^RESULT quartzcore-layer-gpu-composite failures=0')
    run('codex-timings',timing,timing=True)
finally:
    verified=subprocess.run([str(W/'dashboard/qv-host-guard'),'verify',str(guard)],capture_output=True,text=True)
    (R/'host-guard-result.json').write_text(json.dumps(dict(exit=verified.returncode,stdout=verified.stdout,stderr=verified.stderr),indent=2)+'\n')
    rows.append(dict(tag='host-guard',exit=verified.returncode))
    (R/'summary.json').write_text(json.dumps(rows,indent=2)+'\n')
    print('Gate evidence:',R,flush=True)
raise SystemExit(1 if any(row['exit'] for row in rows) else 0)
