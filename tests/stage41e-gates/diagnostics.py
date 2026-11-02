"""Read only image basenames and fixed fatal sites written by OUR runtime."""
from pathlib import Path
import re

KINDS = {'abort', '__abort', 'abort_report_np', 'kernel.abort_with_payload',
         'kernel.terminate_with_payload', 'kernel.simple_abort'}
SITES = {'abort', '__abort', 'abort_report_np', 'sys_abort_with_payload',
         'sys_terminate_with_payload', '__simple_abort'}


def fatal_records(prefix):
    path = Path(prefix) / 'private/var/log/qv-terminal-boundaries.log'
    result = dict(present=path.is_file(), records=[], rejected_records=0)
    if path.is_symlink():
        raise RuntimeError('fatal record file must not be a symlink')
    if not result['present']:
        return result
    pattern = re.compile(r'^QV2 (fatal.origin|fatal.site) ([0-9]+) ([0-9a-f]+) ([0-9a-f]+)\n?$')
    with path.open(errors='replace') as stream:
        for line in stream:
            match = pattern.fullmatch(line)
            if not match:
                # All other diagnostic fields are deliberately discarded.
                continue
            label, timestamp, kind, value = match.groups()
            try:
                kind, value = (bytes.fromhex(v).decode('ascii') for v in (kind, value))
            except (ValueError, UnicodeDecodeError):
                result['rejected_records'] += 1
                continue
            safe = (kind in KINDS and
                    (value in SITES if label == 'fatal.site' else
                     re.fullmatch(r'[A-Za-z_][A-Za-z0-9_.+\-]{0,127}', value)
                     and not re.search(r'0x|[0-9a-fA-F]{8}', value)))
            if not safe:
                result['rejected_records'] += 1
                continue
            result['records'].append(dict(label=label, kind=kind, value=value,
                                          monotonic_ns=int(timestamp)))
    return result
