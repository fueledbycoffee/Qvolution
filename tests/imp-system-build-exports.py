#!/usr/bin/env python3
"""Inspect only this lane's source-built Mach-O output; no runtime claim."""
from pathlib import Path
import json, subprocess
L=Path(__file__).resolve().parents[2]
libs={}
for line in (L/'superproject/tests/imp-system-guest-exports.tsv').read_text().splitlines():
 guest,symbol=line.split('\t')
 name=Path(guest).name
 relative={'libSystem.B.dylib':'src/external/libsystem/libSystem.B.dylib','CFNetwork':'src/external/cfnetwork/src/CFNetwork','CoreFoundation':'src/external/corefoundation/CoreFoundation','SystemConfiguration':'src/external/configd/SystemConfiguration.fproj/SystemConfiguration','DiskArbitration':'src/frameworks/DiskArbitration/DiskArbitration','CoreServices':'src/frameworks/CoreServices/CoreServices'}[name]
 libs.setdefault(name,[relative,guest,[]])[2].append(symbol)
leaves={
 'AERemoveEventHandler':'src/frameworks/CoreServices/src/AE/AE',
 'LSSetDefaultRoleHandlerForContentType':'src/frameworks/CoreServices/src/LaunchServices/LaunchServices',
 '_kLSOpenOptionBackgroundLaunchKey':'src/frameworks/CoreServices/src/LaunchServices/LaunchServices',
 'LSCopyDisplayNameForRef':'src/frameworks/CoreServices/src/LaunchServices/LaunchServices',
 'LSCopyItemInfoForRef':'src/frameworks/CoreServices/src/LaunchServices/LaunchServices',
 'LSCopyKindStringForTypeInfo':'src/frameworks/CoreServices/src/LaunchServices/LaunchServices',
 'LSSetItemAttribute':'src/frameworks/CoreServices/src/LaunchServices/LaunchServices',
 '_CSCheckFixDisable':'src/frameworks/CoreServices/src/CarbonCore/CarbonCore',
 'CSCopyMachineName':'src/frameworks/CoreServices/src/CarbonCore/CarbonCore',
 'FSResolveAliasFileWithMountFlags':'src/frameworks/CoreServices/src/CarbonCore/CarbonCore'}
checked=0
for arch in ['i386','x86_64']:
 cache={}
 for library,(relative,guest,symbols) in libs.items():
  for symbol in symbols:
   leaf=leaves.get(symbol,relative); path=L/'build'/leaf
   if path not in cache:
    assert path.is_file(),path
    output=subprocess.check_output(['llvm-nm','--arch='+arch,'-g','--defined-only',str(path)],text=True)
    cache[path]={line.split()[-1] for line in output.splitlines() if line.split()}
   expected = '.objc_class_name_NSMachPort' if arch == 'i386' and symbol == 'OBJC_CLASS_$_NSMachPort' else '_' + symbol
   assert expected in cache[path],(arch,library,symbol,path)
   print('PASS',arch,library,symbol,'defined in',leaf)
   checked+=1
print(f'RESULT own-build-export-definitions checked={checked}; dlopen pending')

for arch, name in [('x86_64', '_OBJC_CLASS_$_NSMachPort'), ('i386', '.objc_class_name_NSMachPort')]:
 output=subprocess.check_output(['llvm-nm','--arch='+arch,'-g','--defined-only',str(L/'build/src/external/foundation/Foundation')], text=True)
 assert name not in {line.split()[-1] for line in output.splitlines() if line.split()}, 'duplicate Foundation class'
print('PASS Foundation defines no second NSMachPort class in either architecture')
