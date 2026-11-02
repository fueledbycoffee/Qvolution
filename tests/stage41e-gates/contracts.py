"""Contracts of the existing authored Stage41e fixtures."""
from pathlib import Path
import re

FOUNDATION_COMPLETIONS = {
    'strong31': r'^PASS strong31 Swift behavior$',
    **{name: '^' + re.escape(name) + r': passed$' for name in (
        'round3-behavior', 'round3-formatting', 'round3-messages',
        'round3-refinements', 'locale-components')},
}
STRONG31_DIRECTORY = 'Applications/r2-foundation-stub'


def prepare_foundation_directory(prefix):
    """The unchanged strong31 fixture opens a file in this directory."""
    parent = Path(prefix) / STRONG31_DIRECTORY
    if parent.is_symlink():
        raise RuntimeError('strong31 work directory must not be a symlink')
    parent.mkdir(mode=0o755, parents=True, exist_ok=True)
    return parent


def payload_root(stage):
    return '/Volumes/SystemRoot' + str(Path(stage).absolute() / 'usr/local/libexec/darling')


def fail(record, check, detail):
    record.setdefault('failures', []).append({'check': check, 'detail': detail})
    record['passed'] = False


def check_terminal(record, seconds):
    for check in ('shell_executed', 'alive_at_end', 'window', 'shell'):
        if not record.get(check):
            fail(record, check, 'required Terminal observation was false or unavailable')
    if record.get('observed_seconds', 0) < seconds:
        fail(record, 'observation_budget', f'Terminal did not complete {seconds} seconds')


SHELL_STARTUP_PROBES = 3


def shell_present(probes):
    """Login startup may occupy probes 1-2; from first presence none may fail.

    Every probe must still exit successfully and reach EOF within the runner's
    unchanged 10s deadline. The third completed probe is the startup boundary.
    """
    first = next((i for i, p in enumerate(probes) if p['shell']), None)
    return (first is not None and first < SHELL_STARTUP_PROBES
            and all(p['exit'] == 0 and p.get('eof', False) for p in probes)
            and all(p['shell'] for p in probes[first:]))


def shell_observation(probes, start):
    first = next((i for i, p in enumerate(probes) if p['shell']), None)
    return dict(shell=shell_present(probes), shell_startup_allowance_probes=SHELL_STARTUP_PROBES,
                shell_first_probe=None if first is None else first + 1,
                time_to_shell_seconds=None if first is None else probes[first]['when'] - start)
