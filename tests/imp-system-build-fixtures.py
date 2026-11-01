#!/usr/bin/env python3
from pathlib import Path
import os, shlex, subprocess
L=Path(__file__).resolve().parents[2]
assert os.environ.get('QV_SLOT','').startswith('build-')
B=L/'build'; S=L/'stage/usr/local/libexec/darling'; O=L/'fixtures'; O.mkdir(exist_ok=True)
commands=subprocess.check_output(['ninja','-C',str(B),'-t','commands','src/external/libsystem/libSystem.B.dylib'],text=True).splitlines()
base=next(shlex.split(c) for c in commands if ' -c ' in c and c.endswith('/ImportCompatibility.c'))
# The production compile command contains both architecture flags; fixtures are x86_64.
args=[]; i=0
while i < len(base):
 if base[i]=='-arch': i+=2
 else: args.append(base[i]); i+=1
args[1:1]=['-arch','x86_64']
graph=['rule run','  command = cd '+shlex.quote(str(B))+' && $cmd']
sources={'load-exports':L/'superproject/tests/imp-system-load-exports.c','behavior':L/'superproject/tests/imp-system-behavior.c','port-binding':L/'superproject/tests/imp-system-port-binding.c','libsystem-control':L/'superproject/src/external/libsystem/tests/import-compatibility.c','cfnetwork-control':L/'superproject/src/external/cfnetwork/tests/netdiagnostics-unsupported.c'}
for name,src in sources.items():
 obj=O/f'{name}.o'; binary=O/f'imp-system-{name}'
 compile_args=list(args)
 if name=='cfnetwork-control': compile_args[1:1]=['-I'+str(L/'superproject/src/external/cfnetwork/src/Headers')]
 for flag,value in [('-c',src),('-o',obj),('-MF',O/f'{name}.d'),('-MT',obj)]:
  compile_args[compile_args.index(flag)+1]=str(value)
 link=[str(B/'src/external/cctools-port/cctools/ld64/src/ld'),'-syslibroot',str(S),'-arch','x86_64','-macosx_version_min','10.15','-L'+str(S/'usr/lib'),'-F'+str(S/'System/Library/Frameworks'),'-lSystem']
 if name=='port-binding': link+=['-framework','CoreFoundation','-lobjc']
 if name=='behavior': link+=['-framework','CoreFoundation','-framework','DiskArbitration','-framework','SystemConfiguration']
 link += [str(obj),'-o',str(binary)]
 for output,cmd,inputs in [(obj,compile_args,[src]),(binary,link,[obj])]:
  graph += [f'build {output}: run '+ ' '.join(map(str,inputs)),'  cmd = '+shlex.join(cmd).replace('$','$$')]
(O/'build.ninja').write_text('\n'.join(graph)+'\n')
subprocess.run(['ninja','-C',str(O),'-j6'],check=True)
subprocess.run(['ninja','-C',str(O),'-n'],check=True)
