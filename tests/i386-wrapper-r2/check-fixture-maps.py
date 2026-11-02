#!/usr/bin/env python3
"""Read only authored fixture load commands; reject mappings below the host floor."""
from pathlib import Path
import argparse
import re
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--out", type=Path, required=True)
parser.add_argument("binaries", type=Path, nargs="+")
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
for binary in args.binaries:
    output = subprocess.check_output(['llvm-otool', '-l', str(binary)], text=True)
    (args.out / (binary.name + '.otool-l.txt')).write_text(output)
    segments = {}
    for block in output.split('Load command ')[1:]:
        name = re.search(r'^\s*segname (\S+)', block, re.M)
        address = re.search(r'^\s*vmaddr (0x[0-9a-fA-F]+)', block, re.M)
        size = re.search(r'^\s*vmsize (0x[0-9a-fA-F]+)', block, re.M)
        if name and address and size:
            segments[name[1]] = (int(address[1], 16), int(size[1], 16))
    assert segments.get('__PAGEZERO', (1, 0))[0] == 0, (binary, segments)
    assert segments['__PAGEZERO'][1] >= 0x10000, (binary, segments)
    assert segments['__TEXT'][0] >= 0x10000, (binary, segments)
    print(f'{binary.name}: __PAGEZERO={segments["__PAGEZERO"][1]:#x} __TEXT={segments["__TEXT"][0]:#x} PASS')
