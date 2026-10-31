#!/usr/bin/env python3
"""Lightweight source regressions using an already-configured build's recipes.
Only ninja -t commands is read. No build/configuration/install is invoked.
"""
from pathlib import Path
import argparse,json,os,shlex,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--output',type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[2];tests=Path(__file__).resolve().parent
owned_temp=None
if a.output:out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
else:owned_temp=tempfile.TemporaryDirectory(prefix='darling-iosurface-');out=Path(owned_temp.name)
env=dict(os.environ,DARLING_IOSURFACE_TEST_ROOT=str(root),DARLING_IOSURFACE_TEST_OUT=str(out))
def recipe(target):
 lines=subprocess.check_output(['ninja','-C',str(a.build.resolve()),'-t','commands',target],text=True).splitlines()
 argv=shlex.split(next(x for x in reversed(lines) if ' -c ' in x));args=[];i=0
 while i<len(argv):
  if argv[i] in ['-o','-MT','-MF','-arch']:i+=2;continue
  if argv[i] in ['-MD','-c']:i+=1;continue
  if i==len(argv)-1:i+=1;continue
  args.append(argv[i]);i+=1
 args[1:1]=['-I'+str(root/'src/frameworks/IOSurface/include'),'-I'+str(root/'src/frameworks/CoreVideo/include')]
 return args+['-fsyntax-only','-UNDEBUG','-Werror=implicit-function-declaration','-Werror=incompatible-function-pointer-types','-Werror=incompatible-pointer-types']
cv=recipe('src/frameworks/CoreVideo/CMakeFiles/CoreVideo.dir/src/CVPixelBuffer.c.o')
kernel=recipe('src/external/darlingserver/duct-tape/CMakeFiles/darlingserver_duct_tape.dir/src/memory.c.o')
(out/'compile-typed-callers.c.json').write_text(json.dumps(cv+[str(tests/'typed-callers.c')]))
(out/'compile-memory.c.json').write_text(json.dumps(kernel+[str(root/'src/external/darlingserver/duct-tape/src/memory.c')]))
for name in ['protocol.c','map-lifetime.c']:shutil.copyfile(tests/name,out/name)
# Run frozen public ABI and guest scaffolding as actual target-header checks.
for name in ['typed-callers.c','guest-runner.c']:subprocess.run(cv+[str(tests/name)],check=True)
subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-UNDEBUG','-I'+str(root/'src/frameworks/IOSurface/include'),str(tests/'layout.c'),'-o',str(out/'layout')],check=True)
subprocess.run([str(out/'layout')],check=True)
for name in ['run-protocol.py','broker-lifetime.py','surface-call-errors.py','audit-authorization.py']:
 subprocess.run(['python3',str(tests/name)],check=True,env=env)
# Full actual kernel translation unit plus actual Apple-origin refcount code;
# single-thread/host mapping adapters are explicit in map-lifetime.c.
native=[x for x in kernel if x!='-fsyntax-only']
subprocess.run(native+['-O1','-DCV_SURFACE_MEMORY_SOURCE="'+str(root/'src/external/darlingserver/duct-tape/src/memory.c')+'"','-c',str(out/'map-lifetime.c'),'-o',str(out/'map-lifetime.o')],check=True)
subprocess.run(native+['-O1','-c',str(root/'src/external/darlingserver/duct-tape/xnu/libkern/os/refcnt.c'),'-o',str(out/'refcnt.o')],check=True)
subprocess.run(['clang',str(out/'map-lifetime.o'),str(out/'refcnt.o'),'-Wl,--gc-sections','-o',str(out/'map-lifetime')],check=True)
subprocess.run([str(out/'map-lifetime')],check=True)
print('Native source regressions passed. Real guest/Mach/GPU gates remain separate.')
