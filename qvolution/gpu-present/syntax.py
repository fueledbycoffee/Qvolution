#!/usr/bin/env python3
"""Syntax-check lane sources with Stage41c's own compile lines (-fsyntax-only, no outputs)."""
import shlex,subprocess,sys
from pathlib import Path
W=Path('/home/sean/Documents/Codex/Projects/Qvolution-work');L=W/'parallel/gpu-present';B=W/'parallel/stage41c/build';S=W/'parallel/stage41c/superproject'
TREES={str(S/'src/external/cocotron'):L/'cocotron',str(S/'src/frameworks/OpenGL'):L/'superproject/src/frameworks/OpenGL'}
target,*files=sys.argv[1:]
cmds=[shlex.split(c) for c in subprocess.check_output(['ninja','-C',B,'-t','commands',target],text=True).splitlines()]
rc=0
for f in files:
 fixture=f.startswith('tests/')
 a=list(next(c for c in cmds if '-c' in c and c[c.index('-c')+1].endswith('/NSColor.m' if fixture else f)))
 if fixture:a[a.index('-c')+1]=str(L/'cocotron'/f)
 for k,v in TREES.items():a=[x.replace(k+'/',str(v)+'/') for x in a]
 i=a.index('-o');del a[i:i+2];i=a.index('-MF');del a[i:i+2];i=a.index('-MT');del a[i:i+2];a.remove('-MD')
 a=[x for x in a]
 # one arch is enough for a syntax check
 while '-arch' in a:i=a.index('-arch');del a[i:i+2]
 a[1:1]=['-arch','x86_64','-fsyntax-only','-I'+str(L/'out/include-overlay')]
 r=subprocess.run(a,cwd=B);rc|=r.returncode;print(f,'rc',r.returncode)
sys.exit(rc)
