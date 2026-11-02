#!/usr/bin/env python3
"""Exercise the actual initial TSD helper and both kernel TLS setter copies."""
from pathlib import Path
import argparse, json, resource, signal, subprocess
p = argparse.ArgumentParser()
p.add_argument('--all', action='store_true', help='external executor: require ELF32 too')
a = p.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
lane = Path(__file__).resolve().parent
rows = []
for bits in ((64, 32) if a.all else (64,)):
    for mode in ([], ['--no-install']):
        r = subprocess.run([str(lane / f'build/initial-tsd-{bits}'), *mode], capture_output=True, text=True)
        rows.append(dict(bits=bits, mode=mode or ['install'], status=r.returncode,
                         expected=-signal.SIGSEGV if mode else 0, stdout=r.stdout, stderr=r.stderr))
print(json.dumps(rows, indent=2))
raise SystemExit(any(r['status'] != r['expected'] for r in rows))
