#!/usr/bin/env python3
from pathlib import Path
import json, shutil, subprocess, sys, tempfile
L = Path('/home/sean/Documents/Codex/Projects/Qvolution-work/parallel/i386-guest')
step = json.loads((L / 'build/steps.json').read_text())[sys.argv[1]]
for command in step:
    if command[0] == 'archive':
        args = command[1:]
        output = Path(args[2])
        generation = Path(tempfile.mkdtemp(prefix='archive-', dir=L / 'build')) / output.name
        args[2] = str(generation)
        subprocess.run(args, cwd=L.parent / 'stage41d/build', check=True)
        subprocess.run([str(L.parent / 'stage41d/build/src/external/cctools-port/cctools/ar/x86_64-apple-darwin20-ranlib'), str(generation)], check=True)
        shutil.copyfile(generation, output)
    else:
        subprocess.run(command, cwd=L.parent / 'stage41d/build', check=True)
