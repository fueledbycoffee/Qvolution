#!/usr/bin/env python3
"""Real CMake/Ninja dependency regression, with an authored dummy generator.

Uses the exact supplied mig() macro and bootstrap wrapper-producing command.
This checks rebuild scheduling, not the semantics of real MIG output.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('--mig-macro', type=Path, required=True)
p.add_argument('--bootstrap-cmake', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
bootstrap = a.bootstrap_cmake.read_text()
begin = bootstrap.index('add_custom_command(\n\tOUTPUT "${CMAKE_BINARY_DIR}/build-mig"')
end = bootstrap.index('add_dependencies(migexe migcom)', begin)
wrapper_rule = bootstrap[begin:end] + 'add_dependencies(migexe migcom)\n'
property_line = 'set_property(TARGET migcom PROPERTY MIGCOM_PATH "${MIGCOM_PATH}")'
property_rule = property_line if property_line in bootstrap else ''
macro = a.mig_macro.resolve()
records = []
failures = []

dummy_tool = r'''#!/usr/bin/env python3
import hashlib,json,os,sys
from pathlib import Path
args=sys.argv[1:]; flags={}
for key in ['-user','-header','-server','-sheader','-xtracemig','-arch','-target']:
    flags[key]=args[args.index(key)+1]
tool=Path(os.environ.get('QV_DUMMY_TOOL_BINARY',__file__))
payload={'defs':Path(args[-1]).read_text(),'tool':hashlib.sha256(tool.read_bytes()).hexdigest(),
         'wrapper':hashlib.sha256(Path(os.environ['QV_DUMMY_WRAPPER_FILE']).read_bytes()).hexdigest(),
         'arch':flags['-arch'],'target':flags['-target']}
for key in ['-user','-header','-server','-sheader','-xtracemig']:
    Path(flags[key]).write_text(json.dumps(payload,sort_keys=True)+'\n')
with open(os.environ['QV_DUMMY_LOG'],'a') as log: log.write(json.dumps(payload)+'\n')
'''
dummy_wrapper = '''#!/bin/sh
# Authored dependency fixture; never calls real MIG.
export QV_DUMMY_WRAPPER_FILE="$0"
export QV_DUMMY_LOG="$(dirname "$0")/invocations.jsonl"
exec "$MIGCOM" "$@"
'''
dummy_awk = '''# Authored fixture path insertion, not a MIG semantic implementation.
NR == 2 { print "MIGCOM=\\\"" migcom "\\\"" }
{ print }
'''
launcher = r'''#include <stdlib.h>
#include <unistd.h>
static const char revision[]="v1";
int main(int argc,char **argv) {
    char **next=calloc((size_t)argc+2,sizeof(*next));if(!next)return 2;
    setenv("QV_DUMMY_TOOL_BINARY",argv[0],1);setenv("QV_DUMMY_TOOL_REVISION",revision,1);
    next[0]=DUMMY_PYTHON;next[1]=DUMMY_SCRIPT;
    for(int i=1;i<argc;i++)next[i+1]=argv[i];
    execv(next[0],next);return 3;
}
'''

def run(command, cwd, log):
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'{command[0]} failed; see {log}')
    return result.stdout

for mode in ['custom', 'native']:
    case = a.output / mode
    source = case / 'source'
    build = case / 'build'
    for directory in ['tool/migcom.tproj', 'tool/darling/src', 'consumer/input']:
        (source / directory).mkdir(parents=True, exist_ok=True)
    tool_source = source / 'tool/dummy-tool.py'
    wrapper_source = source / 'tool/migcom.tproj/mig.sh'
    awk_source = source / 'tool/darling/src/mig.awk'
    definitions = source / 'consumer/input/task.defs'
    tool_source.write_text(dummy_tool)
    wrapper_source.write_text(dummy_wrapper)
    awk_source.write_text(dummy_awk)
    definitions.write_text('authored definitions revision 1\n')
    (source / 'tool/launcher.c').write_text(launcher)
    (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.18)
project(mig_dependency_fixture C)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_subdirectory(tool)
add_subdirectory(consumer)
''')
    tool_cmake = '''if (FIXTURE_NATIVE)
  add_executable(migcom launcher.c)
  target_compile_definitions(migcom PRIVATE DUMMY_PYTHON="${Python3_EXECUTABLE}" DUMMY_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/dummy-tool.py")
  set(MIGCOM_PATH "${CMAKE_CURRENT_BINARY_DIR}/migcom")
else()
  set(MIGCOM_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin/migcom")
  add_custom_command(OUTPUT "${MIGCOM_PATH}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/dummy-tool.py" "${MIGCOM_PATH}"
    COMMAND chmod 0755 "${MIGCOM_PATH}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/dummy-tool.py")
  add_custom_target(migcom DEPENDS "${MIGCOM_PATH}")
endif()
'''
    (source / 'tool/CMakeLists.txt').write_text(tool_cmake + property_rule + '\n' + wrapper_rule)
    (source / 'consumer/CMakeLists.txt').write_text('''set(MIG_NO_XTRACE TRUE)
set(APPLE_TARGET_TRIPLET_PRIMARY "fixture-apple-darwin")
set(MIG_MULTIARCH x86_64 i386)
include("''' + str(macro) + '''")
mig(input/task.defs)
add_custom_target(generated ALL DEPENDS
  "${CMAKE_CURRENT_BINARY_DIR}/input/task-x86_64-User.c"
  "${CMAKE_CURRENT_BINARY_DIR}/input/task-i386-User.c")
''')
    run(['cmake', '-G', 'Ninja', '-S', str(source), '-B', str(build),
         '-DFIXTURE_NATIVE=' + ('ON' if mode == 'native' else 'OFF')], case, case / 'configure.log')
    run(['ninja', '-C', str(build)], case, case / 'initial-build.log')
    log_file = build / 'invocations.jsonl'
    outputs = [build / f'consumer/input/task-{arch}-User.c' for arch in ['x86_64', 'i386']]
    def count():
        return len(log_file.read_text().splitlines())
    if count() != 2:
        failures.append(mode + ': initial architecture count')
    for output, arch in zip(outputs, ['x86_64', 'i386']):
        value = json.loads(output.read_text())
        if value['arch'] != arch or value['target'] != 'fixture-apple-darwin':
            failures.append(mode + ': architecture/target flags')
    query = run(['ninja', '-C', str(build), '-t', 'query', 'consumer/input/task-x86_64-User.c'],
                case, case / 'query-initial.txt')
    before = count()
    idle = run(['ninja', '-C', str(build)], case, case / 'idle-build.log')
    if count() != before or 'no work to do' not in idle:
        failures.append(mode + ': initial no-op stability')
    for label in ['defs', 'tool', 'wrapper', 'awk']:
        before = count()
        previous_time = max(path.stat().st_mtime_ns for path in outputs)
        if label == 'defs':
            edited = definitions
            edited.write_text(edited.read_text() + 'authored definitions revision 2\n')
        elif label == 'tool' and mode == 'custom':
            edited = tool_source
            edited.write_text(edited.read_text() + '# authored tool revision 2\n')
        elif label == 'tool':
            edited = source / 'tool/launcher.c'
            edited.write_text(edited.read_text().replace('revision[]="v1"', 'revision[]="v2"'))
        elif label == 'wrapper':
            edited = wrapper_source
            edited.write_text(edited.read_text() + '# authored wrapper revision 2\n')
        else:
            edited = awk_source
            edited.write_text(edited.read_text() + 'END { print "# authored awk revision 2" }\n')
        # Wait for actual clock/filesystem progress; never assign future mtimes.
        deadline = time.monotonic() + 5
        while edited.stat().st_mtime_ns <= previous_time:
            if time.monotonic() >= deadline:
                raise RuntimeError('Filesystem clock did not advance for ' + label)
            time.sleep(0.02)
            edited.touch()
        run(['ninja', '-C', str(build)], case, case / (label + '-build.log'))
        after = count()
        regenerated = after - before
        records.append({'mode': mode, 'edit': label, 'invocations_added': regenerated})
        if regenerated != 2:
            failures.append(mode + ': stale after ' + label)
        idle_count = count()
        idle = run(['ninja', '-C', str(build)], case, case / (label + '-idle.log'))
        if count() != idle_count or 'no work to do' not in idle:
            failures.append(mode + ': no-op after ' + label)

summary = {'purpose': 'authored dummy scheduling test, not MIG output semantics',
           'test_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
           'cmake_version': subprocess.check_output(['cmake', '--version'], text=True).splitlines()[0],
           'ninja_version': subprocess.check_output(['ninja', '--version'], text=True).strip(),
           'macro_sha256': hashlib.sha256(a.mig_macro.read_bytes()).hexdigest(),
           'bootstrap_cmake_sha256': hashlib.sha256(a.bootstrap_cmake.read_bytes()).hexdigest(),
           'results': records, 'failures': failures}
(a.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
raise SystemExit(bool(failures))
