#!/usr/bin/env python3
"""One short timing fixture, after perf acquisition; no container initialization."""
import json, os, subprocess, sys, time
from pathlib import Path
W = Path('/home/sean/Documents/Codex/Projects/Qvolution-work')
assert os.environ.get('QV_SLOT','').startswith('guest-'), 'qv-slot perf required'
receipt, budget, launcher, *args = sys.argv[1:]
assert os.environ.get('DPREFIX'), 'exact prefix required'
row = dict(loadavg_before=Path('/proc/loadavg').read_text().strip(), command=[launcher,*args], start=time.time(), timeout=False)
try:
    process = subprocess.Popen([launcher,*args])
    start = time.monotonic()
    while process.poll() is None:
        if time.monotonic()-start > float(budget):
            row['timeout'] = True
            subprocess.run([str(W/'dashboard/qv-shutdown'), launcher], check=True) # Same perf slot, exact DPREFIX.
            process.wait()
            break
        time.sleep(0.25)
    row['exit'] = process.returncode
finally:
    row.update(loadavg_after=Path('/proc/loadavg').read_text().strip(), end=time.time())
    Path(receipt).write_text(json.dumps(row, indent=2)+'\n')
sys.exit(124 if row['timeout'] else row['exit'])
