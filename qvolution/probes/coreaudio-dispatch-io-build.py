#!/usr/bin/env python3
"""Build original real-dispatch controls from an explicit Darwin compile record."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-record', required=True, type=Path,
                    help='JSON records containing the production registry command array')
parser.add_argument('--sysroot', required=True, type=Path)
parser.add_argument('--linker', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
project = Path(__file__).resolve().parents[2]
out = args.output.resolve()
if out == project or project in out.parents:
    parser.error('output must be outside the source checkout')
if not args.sysroot.is_dir() or not args.linker.is_file():
    parser.error('sysroot directory and Darwin linker must exist')
records = json.loads(args.compile_record.read_text())
if isinstance(records, dict):
    records = [records]
template = next(record['command'] for record in records
                if Path(record['source']).name == 'AudioPropertyListeners.cpp')
# A recorded build command must never write dependency files into its old build.
command = []
index = 0
while index < len(template):
    value = template[index]
    if value in ('-MD', '-MMD', '-MP'):
        index += 1
    elif value in ('-MF', '-MT', '-MQ'):
        index += 2
    else:
        command.append(value)
        index += 1
out.mkdir(parents=True, exist_ok=True)
registry = project / 'src/CoreAudio/CoreAudio/AudioPropertyListeners.cpp'
fixture = project / 'qvolution/probes/coreaudio-dispatch-io.cpp'
header = registry.with_suffix('.h')
result = {'compile_record': str(args.compile_record.resolve()), 'compiles': [],
          'registry_header': {'path': str(header), 'sha256': hashlib.sha256(header.read_bytes()).hexdigest()}}
for source in (registry, fixture):
    invocation = command.copy()
    invocation[invocation.index('-c') + 1] = str(source)
    invocation[invocation.index('-o') + 1] = str(out / (source.name + '.o'))
    invocation[1:1] = ['-fblocks', '-I' + str(project / 'src/CoreAudio/include'),
                      '-I' + str(registry.parent)]
    compiled = subprocess.run(invocation, text=True, capture_output=True)
    (out / (source.name + '.compile.log')).write_text(compiled.stdout + compiled.stderr)
    result['compiles'].append({'source': str(source), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                               'command': invocation, 'exit': compiled.returncode})
    if compiled.returncode:
        (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        raise SystemExit(compiled.returncode)
stage = args.sysroot.resolve()
binary = out / 'coreaudio-dispatch-io'
link = [str(args.linker.resolve()), '-syslibroot', str(stage), '-arch', 'x86_64',
        '-macosx_version_min', '11.0', '-L' + str(stage / 'usr/lib'),
        '-L' + str(stage / 'usr/lib/swift'), '-rpath', '/usr/lib/swift', '-lSystem', '-lc++',
        str(out / (fixture.name + '.o')), str(out / (registry.name + '.o')), '-o', str(binary)]
linked = subprocess.run(link, text=True, capture_output=True)
(out / 'link.log').write_text(linked.stdout + linked.stderr)
result['link'] = {'command': link, 'exit': linked.returncode}
if not linked.returncode:
    result['binary'] = {'path': str(binary), 'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}
(out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
print('real dispatch fixture: compile exits', [record['exit'] for record in result['compiles']],
      'link exit', linked.returncode)
raise SystemExit(linked.returncode)
