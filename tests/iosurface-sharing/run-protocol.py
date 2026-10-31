from pathlib import Path
import json,subprocess,os
root=Path(os.environ['DARLING_IOSURFACE_TEST_ROOT'])
b=Path(os.environ['DARLING_IOSURFACE_TEST_OUT'])
args=json.loads((b/'compile-typed-callers.c.json').read_text())[:-1]
out=[];i=0
while i<len(args):
 if args[i] in ['-target','-B']:i+=2;continue
 if args[i]=='-fsyntax-only' or args[i].startswith('-mmacosx-version-min'):i+=1;continue
 out.append(args[i]);i+=1
subprocess.run(out+['-Wno-macro-redefined','-c',str(b/'protocol.c'),'-o',str(b/'protocol.o')],check=True)
subprocess.run(['clang',str(b/'protocol.o'),'-o',str(b/'protocol')],check=True)
r=subprocess.run([str(b/'protocol')],check=True,text=True,stdout=subprocess.PIPE)
(b/'protocol.log').write_text(r.stdout);print(r.stdout,end='')
subprocess.run(out+['-Wno-macro-redefined','-target','i386-apple-darwin20','-fsyntax-only',str(b/'protocol.c')],check=True)
print('i386 wire ABI syntax assertions: passed')
