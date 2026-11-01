#!/usr/bin/env python3
"""Reject duplicate or misplaced NSMachPort class definitions before linking."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
cf = (root / 'src/external/corefoundation/NSPort.m').read_text()
foundation = (root / 'src/external/foundation/src/NSPort.m').read_text()
definition = r'@implementation\s+NSMachPort\s*(?=\n)'
assert len(re.findall(definition, cf)) == 1, 'CoreFoundation must own the class demanded from it'
assert not re.search(definition, foundation), 'Foundation must not define a second class'
assert '@implementation NSMachPort (NSMachPort)' in foundation
print('PASS one CoreFoundation class, Foundation method category')
