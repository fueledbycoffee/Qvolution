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


def shell_present(probes):
    # The first probe races login-shell startup; subsequent probes must all pass.
    return bool(probes) and probes[-1]['shell'] and all(
        p['exit'] == 0 and p['shell'] for p in probes[1:])
