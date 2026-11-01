#!/usr/bin/env python3
"""Exercise actual CoreML methods with controlled native Foundation doubles."""
from pathlib import Path
import os, subprocess, sys
t = Path(__file__).resolve().parent
out = Path(sys.argv[1]).resolve(); out.mkdir(parents=True, exist_ok=True)
blocks = Path(os.environ.get('QV_BLOCKS_RUNTIME', str(t.parents[6]/'toolchains/swift-6.3.3-linux-x64/compiler/usr/lib/swift/linux')))
source = t.parent/'src/CoreML.m'
headers = t.parent/'include'
if len(sys.argv) > 2:
    # Compile the exact committed pre-fix unit/header with the current regression.
    root = t.parents[3]
    source = out/'CoreML.m'
    headers = out/'include'
    (headers/'CoreML').mkdir(parents=True, exist_ok=True)
    for relative, destination in [('src/CoreML.m',source), ('include/CoreML/CoreML.h',headers/'CoreML/CoreML.h')]:
        destination.write_bytes(subprocess.check_output(['git','-C',str(root),'show',sys.argv[2]+':src/frameworks/CoreML/'+relative]))
subprocess.run(['clang','-g','-fblocks','-fobjc-runtime=gcc','-fconstant-string-class=NativeConstantString',
    '-fsanitize=address,undefined','-I/usr/lib/gcc/x86_64-linux-gnu/15/include',
    '-I'+str(t/'native'), '-I'+str(headers), str(t/'native/runtime.m'),
    str(source),str(t/'behavior.m'),'-lobjc','-L'+str(blocks),'-Wl,-rpath,'+str(blocks),'-lBlocksRuntime','-o',str(out/'coreml')],check=True)
subprocess.run([str(out/'coreml')],check=True)
