"""Read-only prefix attribution; no signals, guest memory, or app output."""
from pathlib import Path
import collections
import os
import re


def snapshot(prefix, proc=Path('/proc')):
    prefix = Path(prefix)
    entry = ('__mldr_sockpath=' + str(prefix) + '/.darlingserver.sock').encode()
    result = {'prefix': str(prefix), 'recorded_pids': {}, 'processes': [], 'unreadable': 0}
    for name in ('.init.pid', '.shellspawn.pid'):
        path = prefix / name
        try:
            if path.is_symlink():
                raise ValueError('PID record is a symlink')
            result['recorded_pids'][name] = int(path.read_text().strip())
        except FileNotFoundError:
            result['recorded_pids'][name] = None
        except (OSError, ValueError) as error:
            result['recorded_pids'][name] = {'error': str(error)}
    for path in proc.iterdir():
        if not path.name.isdigit():
            continue
        try:
            comm = (path / 'comm').read_text().strip()
            if comm == 'darlingserver':
                argv = (path / 'cmdline').read_bytes().split(b'\0')
                matches = len(argv) > 1 and argv[1] == os.fsencode(prefix)
            else:
                matches = entry in (path / 'environ').read_bytes().split(b'\0')
            if not matches:
                continue
            row = {'pid': int(path.name), 'comm': comm}
            for line in (path / 'status').read_text().splitlines():
                key, _, value = line.partition(':')
                if key in ('State', 'PPid', 'Uid', 'NSpid', 'Threads'):
                    row[key] = value.strip()
            row['exe'] = os.readlink(path / 'exe')
            row['stdio'] = {}
            for fd in ('0', '1', '2'):
                try:
                    row['stdio'][fd] = os.readlink(path / 'fd' / fd)
                except OSError:
                    row['stdio'][fd] = None
            try:
                fields = (path / 'stat').read_text().rsplit(')', 1)[1].split()
                row['session'] = int(fields[3])
                row['tty_device'] = int(fields[4])
            except (OSError, ValueError, IndexError):
                row['session'] = row['tty_device'] = None
            row['threads'] = []
            for thread in (path / 'task').iterdir():
                try:
                    row['threads'].append({'tid': int(thread.name),
                                           'wchan': (thread / 'wchan').read_text().strip()})
                except OSError:
                    continue
            # Library names only. Never retain addresses, bytes or stack frames.
            row['images'] = sorted(set(
                fields[5] for line in (path / 'maps').read_text().splitlines()
                if len(fields := line.split(maxsplit=5)) == 6 and fields[5].startswith('/')))
            result['processes'].append(row)
        except OSError:
            result['unreadable'] += 1
    return result


def server_records(prefix):
    path = Path(prefix) / 'private/var/log/dserver.log'
    result = {'path': str(path), 'present': path.is_file(), 'categories': {}, 'warning_calls': {}, 'unclassified_warnings': {}, 'lifecycle': []}
    counts = collections.Counter()
    warnings = collections.Counter()
    unclassified = collections.Counter()
    # Fixed formats from OUR call.cpp and duct-tape src/{locks,stubs,thread,memory}.c.
    # Never retain message bodies, argument values, addresses or guest kprintf text.
    formats = {
        'calls': {
            'PthreadCanceled:unimplemented': r'TODO: virtual void DarlingServer::Call::PthreadCanceled::processCall\(\)',
            'PthreadMarkcancel:unimplemented': r'TODO: virtual void DarlingServer::Call::PthreadMarkcancel::processCall\(\)',
        },
        'dtape': {
            'dtape_mutex_lock_or_try_lock:no_active_thread': r'Trying to lock mutex without an active thread!',
            'dtape_mutex_unlock:no_active_thread': r'Trying to unlock mutex without an active thread!',
            'proc_get_effective_thread_policy:unimplemented_flavor': r'stub: proc_get_effective_thread_policy:unimplemented flavor',
            'thread_guard_violation:stub': r'stub: thread_guard_violation',
            'kmem_suballoc:stub': r'stub: kmem_suballoc',
        },
    }
    if not result['present']:
        return result
    header = re.compile(r'^\[[0-9.]+\]\(([^,]+), (Debug|Info|Warning|Error)\)')
    # These fixed numeric lifecycle formats are emitted by our process.cpp/thread.cpp.
    lifecycle = re.compile(
        r'^\[[0-9.]+\]\((?:process|thread), Info\)(?:\[P:\d+\(\d+\)\])?'
        r'(?:\[T:\d+\(\d+\)\])? (?:'
        r'New (?:process|thread) created with ID \d+ and NSID \d+(?: for process with ID \d+ and NSID \d+)?|'
        r'Process \d+ \(\d+\) is now pending replacement|'
        r'\[(?:P|T):\d+\(\d+\)\]: (?:process|thread) (?:dying|being destroyed)|'
        r'\[P:\d+\(\d+\)\]: replacing process with a new task \(with architecture "(?:x86_64|i386)"\))$')
    with path.open(errors='replace') as log:
        for line in log:
            if match := header.match(line):
                counts[match[1] + ':' + match[2]] += 1
                if match[2] == 'Warning':
                    category = match[1]
                    for label, pattern in formats.get(category, {}).items():
                        if re.search(r'(?:^| )' + pattern + r'\s*$', line):
                            warnings[category + ':' + label] += 1
                            break
                    else:
                        unclassified[category] += 1
            if lifecycle.fullmatch(line.rstrip('\n')):
                result['lifecycle'].append(line.rstrip('\n'))
    result['categories'] = dict(counts)
    result['warning_calls'] = dict(warnings.most_common())
    result['unclassified_warnings'] = dict(unclassified)
    return result
