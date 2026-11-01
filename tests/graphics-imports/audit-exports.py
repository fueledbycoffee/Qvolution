#!/usr/bin/env python3
"""Read only Darling output generated in this lane, never system/SDK binaries."""
from pathlib import Path
import argparse, json, subprocess, re
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path, help='Darling build containing our own outputs only')
parser.add_argument('evidence', type=Path, help='output directory for the manifest and static results')
args=parser.parse_args()
build=args.build.resolve(); evidence=args.evidence.resolve()
evidence.mkdir(parents=True, exist_ok=True)
rows=json.loads(Path(__file__).with_name('exports.json').read_text())
paths={
 'CoreGraphics':'src/external/cocotron/CoreGraphics/CoreGraphics',
 'CoreText':'src/external/cocotron/CoreText/CoreText',
 'QuartzCore':'src/external/cocotron/QuartzCore/QuartzCore',
 'ImageIO':'src/frameworks/ImageIO/ImageIO',
 'CoreDisplay':'src/frameworks/CoreDisplay/CoreDisplay',
 'QuickLookThumbnailing':'src/frameworks/QuickLookThumbnailing/QuickLookThumbnailing'}
results=[];host=[];guest=[]
for lib,symbols in rows.items():
 symbols=list(symbols)
 for name in list(symbols):
  if name.startswith('_OBJC_CLASS_$_'):
   meta=name.replace('_OBJC_CLASS_$_','_OBJC_METACLASS_$_')
   if meta not in symbols:symbols.append(meta)
 binary=build/paths[lib]
 if not binary.is_file():raise SystemExit(f'missing own build output: {binary}')
 def exported_names(path):
  output=subprocess.check_output(['llvm-objdump-21','--macho','--arch=x86_64','--exports-trie',str(path)],text=True)
  names=set()
  for line in output.splitlines():
   if line.startswith('0x') or line.startswith('[re-export]'):
    match=re.search(r'\s(_\S+)',line)
    if match:names.add(match.group(1))
  return names
 defined=exported_names(binary)
 if lib == 'CoreGraphics':
  supplement=build/'src/external/cocotron/CoreGraphics/libswiftCoreGraphicsImports.dylib'
  load_commands=subprocess.check_output(['llvm-objdump-21','--macho','--arch=x86_64','--private-headers',str(binary)],text=True)
  if not re.search(r'cmd LC_REEXPORT_DYLIB\s+cmdsize \d+\s+name /usr/lib/swift/libswiftCoreGraphicsImports\.dylib',load_commands):
   raise SystemExit('CoreGraphics is missing the own-built Swift supplement reexport')
  supplement_defined=exported_names(supplement)
  defined.update(supplement_defined)
  # Check every actually exported Swift entry emitted for the added source,
  # including default arguments, metadata and synthesized conformances.
  for entry in sorted(supplement_defined):
   if entry.startswith('_$s') and entry not in symbols:symbols.append(entry)
  # The dlopen harness queries the parent, so runtime reexport resolution
  # remains a real load gate even after this static load-command check.
 missing=sorted(set(symbols)-defined)
 results.append(dict(library=lib,build=str(binary),checked=len(symbols),missing=missing))
 for symbol in symbols:
  host.append(str(binary)+'\t'+symbol[1:])
  guest.append('/System/Library/Frameworks/'+lib+'.framework/Versions/A/'+lib+'\t'+symbol[1:])
(evidence/'exports.json').write_text(json.dumps(results,indent=2)+'\n')
(evidence/'host-load.tsv').write_text('\n'.join(host)+'\n')
(evidence/'guest-load.tsv').write_text('\n'.join(guest)+'\n')
print(json.dumps(results,indent=2))
if any(row['missing'] for row in results):raise SystemExit(1)
