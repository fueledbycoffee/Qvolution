#!/usr/bin/env python3
"""Native checks of gate classification; no launcher, container or GPU access."""
from gate_checks import problems
base=dict(timeout=False,exit=0,results=[],renderer='egl-verify')
assert not problems(base,'RESULT failures=0','',required='^RESULT failures=0$')
for stdout,stderr in [('FAIL hidden failure',''),('RESULT failures=2',''),('','AppKit EGL: reference mismatch 1/6144 pixels'),('','Wayland damage verification: 1 stale pixels')]:
    assert problems(base,stdout,stderr)
assert problems(base,'','',required='^RESULT failures=0$')
assert problems(dict(base,exit=1),'RESULT failures=0','')
assert not problems(dict(base,exit=1),'','reference mismatch 1/6144 pixels',expected_exit=1,required='reference mismatch [1-9][0-9]*/')
soak=dict(mode='soak',frames=1000,fdAt100=48,fdAtEnd=47,rssKbAt100=10000,rssKbAtEnd=11000,failures=0)
assert not problems(dict(base,renderer='egl',results=[soak]),'','')
for changed in [dict(soak,frames=999),dict(soak,fdAtEnd=49),dict(soak,rssKbAtEnd=18193)]:
    assert problems(dict(base,results=[changed]),'','')
assert problems(dict(base,results=[dict(swaps=101,checks=0)]),'','')
assert not problems(dict(base,results=[dict(swaps=101,checks=101)]),'','')
print('PASS native gate classifier: fixture errors, missing evidence, negative control, per-frame verification, FD/RSS/1000-frame soak')
