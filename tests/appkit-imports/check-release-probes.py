#!/usr/bin/env python3
"""Check actual probe translation units keep behavior checks under NDEBUG."""
import argparse
from pathlib import Path
import re
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('build', type=Path)
parser.add_argument('probe', choices=['appkit', 'accessibility'])
args = parser.parse_args()
objects = {
    'appkit': 'src/external/cocotron/AppKit/CMakeFiles/appkit_import_behavior.dir/tests/import-behavior.m.o',
    'accessibility': 'src/frameworks/Accessibility/CMakeFiles/accessibility_import_behavior.dir/tests/import-behavior.m.o',
}
command = shlex.split(subprocess.check_output([
    'ninja', '-C', str(args.build), '-t', 'commands', '-s', objects[args.probe]
], text=True).strip())
filtered = []
index = 0
while index < len(command):
    flag = command[index]
    if flag in ['-o', '-MT', '-MF']:
        index += 2
        continue
    if flag not in ['-MD', '-c']:
        filtered.append(flag)
    index += 1
# Keep the real compiler flags and headers; inspect only the authored main.
output = subprocess.check_output(filtered + ['-DNDEBUG', '-E', '-P'], cwd=args.build, text=True)
main = output[output.index('int main(int argc, char **argv)'):]
# Error message strings alone must not count as evaluated checks.
main = re.sub(r'@?"(?:\\.|[^"\\])*"', '""', main)
main = re.sub(r'\s+', '', main)
expected = {'appkit': ['[[item stringValue] isEqualToString: ""]', '[[entryCopy badge] itemCount] == 12',
                       '[writing isWritingToolsAvailable]' ],
            'accessibility': ['[[object label] isEqualToString: ""]', '[[object value] isEqualToString: ""]',
                              '[copy importance] == AXCustomContentImportanceHigh',
                              '[copy attributedLabel]']}
for expression in ['argc == 2', *expected[args.probe]]:
    if re.sub(r'\s+', '', expression) not in main:
        raise SystemExit(f'FAIL {args.probe}: NDEBUG removed check: {expression}')
subprocess.run(filtered + ['-DNDEBUG', '-fsyntax-only'], cwd=args.build, check=True)
print(f'PASS {args.probe}: NDEBUG preserves argument/behavior checks and syntax compilation succeeds')
