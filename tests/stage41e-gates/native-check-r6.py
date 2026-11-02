#!/usr/bin/env python3
"""Exercise real native SIGSEGV with OUR and foreign mapped executables."""
import json
import os
from pathlib import Path
import subprocess
from diagnostics import fatal_records
L = Path(__file__).resolve().parents[3]
B = L/'native-r6/build'
subprocess.run([str(B/'signal-native'), '--classify'], check=True)
for label, executable, enabled, expected in (
        ('own', 'signal-native', '1', 'signal-native'),
        ('foreign', 'signal-foreign', '1', 'app'),
        ('disabled', 'signal-native', '0', None)):
    root = L/'evidence/r6'/('native-signal-'+label)
    log = root/'private/var/log/qv-terminal-boundaries.log'
    log.parent.mkdir(parents=True, exist_ok=False)
    log.write_text('')
    env = {**os.environ, 'QV_PREVIEW_FATAL': enabled,
           '__mldr_sockpath': str(root/'.darlingserver.sock')}
    result = subprocess.run([str(B/executable)], env=env)
    assert result.returncode == 139, (label, result.returncode)
    records = fatal_records(root)
    assert records['rejected_records'] == 0
    expected_records = [] if expected is None else [dict(label='fatal.origin', kind='signal.SIGSEGV',
                                                        value=expected, monotonic_ns=0)]
    assert records['records'] == expected_records, (label, records)
    print('PASS real native SIGSEGV', label, json.dumps(records))
