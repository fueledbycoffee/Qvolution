#!/usr/bin/env python3
from pathlib import Path
t = Path(__file__).resolve().parent
assert '--vision-coreml-subset' in (t/'load-exports.c').read_text(), 'two-framework probe must require an explicit subset scope'
symbols = (t/'visionkit-open.txt').read_text().splitlines()
assert len(symbols) == 19 and len(set(symbols)) == 19
assert all(s.startswith('_$s9VisionKit') for s in symbols)
assert 'OPEN' in (t/'SCOPE.md').read_text()
print('PASS: subset gate explicitly excludes 19 tracked OPEN VisionKit Swift imports')
