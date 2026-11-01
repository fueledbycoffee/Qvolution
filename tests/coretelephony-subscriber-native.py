#!/usr/bin/env python3
"""Generate a native test of our subscriber diagnostic, not of Objective-C dispatch.
Only the method's C body is reused; the dispatch_once block syntax is adapted to a lambda.
"""
from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[1]; out=Path(sys.argv[1]).resolve(); out.mkdir(parents=True,exist_ok=True)
p='src/frameworks/CoreTelephony/src/CTSubscriberInfo.m'
prefix='''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using dispatch_once_t=int;
template<class F> static void dispatch_once(dispatch_once_t *once,F body) { if(!*once) { *once=1; body(); } }
#define nil nullptr
'''
rules=[]
for phase in ('before','after'):
    s=subprocess.check_output(['git','-C',str(root),'show',f'cd53f527ea:{p}'],text=True) if phase=='before' else (root/p).read_text()
    body=s[s.index('+ (CTSubscriber *)subscriber'):s.index('\n@end')]; body=body[body.index('{'):]
    body=body.replace('dispatch_once(&once, ^{','dispatch_once(&once, [&]{')
    source=out/(phase+'.cpp'); source.write_text(prefix+'void *subscriber()\n'+body+'\nint main() { return subscriber()!=nullptr || subscriber()!=nullptr; }\n')
    rules.append(f'build {out}/{phase}: cxx {source}\n')
(out/'build.ninja').write_text('rule cxx\n  command = c++ -std=c++14 -O1 -Wall $in -o $out\n'+''.join(rules))
