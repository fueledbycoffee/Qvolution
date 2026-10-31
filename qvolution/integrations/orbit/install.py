#!/usr/bin/env python3
"""Offline, version-locked Orbit runtime patch. No settings or process control."""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import stat
import tempfile

RULES = [{'root': 'extension',
  'path': 'extension.js',
  'before': '582bf7ffabb074c25fc772524cdb5b4739d9ac1d0fd5c2efb523e64bb0fa14c9',
  'after': 'b73b4ad41547b43634a5d4d6e963003196867473e089c4e4ed1aeff33602149d'},
 {'root': 'extension',
  'path': 'windowData.js',
  'before': None,
  'after': 'ac6f22a78d4614ce6653c29843e5f3c15aaab9b0f8ab11dcb906d47ad329b5d9'},
 {'root': 'daemon',
  'path': 'globalmenu/menu_model/menu_model.py',
  'before': 'b5c011b88b6298545006e83389b3746382e7d8d257e4c4a58b47fa8453a59d77',
  'after': '59eac73521cad74fe4a2407278367ed963f01a6f83c6c1297a0917a06d47c327'},
 {'root': 'daemon',
  'path': 'globalmenu/utils/service.py',
  'before': '2c82ec92b99dd0d31a9905612d0e99c491600a4f0b05fa4f236d9502285f92fc',
  'after': '979d0a9ba15c74990a8c0af5d4a56f72b1ea96d4f552d183b4594ef6ee8bd5b6'},
 {'root': 'daemon',
  'path': 'globalmenu/utils/wayland_menu.py',
  'before': None,
  'after': '174bd241daac2fc581ae87776abd766d94ec822b4ad9e157185c8598c5e729ff'}]
BUNDLE_ID = hashlib.sha256(json.dumps(RULES, sort_keys=True).encode()).hexdigest()


class Refuse(RuntimeError):
    pass


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sync_directory(path):
    directory = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)


def atomic_write(path, data, mode):
    fd, temporary = tempfile.mkstemp(prefix='.' + path.name + '.qvolution-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as output:
            output.write(data)
            output.flush()
            os.fchmod(output.fileno(), mode)
            os.fsync(output.fileno())
        os.replace(temporary, path)
        sync_directory(path.parent)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


class Installer:
    def __init__(self, extension_root, daemon_root, work_root, bundle=None):
        self.roots = {'extension': Path(extension_root).resolve(), 'daemon': Path(daemon_root).resolve()}
        self.work = Path(work_root).resolve()
        if self.work.name != 'Qvolution-work':
            raise Refuse('--work-root must name the Qvolution-work directory')
        for root in self.roots.values():
            if not root.is_dir():
                raise Refuse(f'Installed root is not a directory: {root}')
        self.bundle = Path(bundle or Path(__file__).parent)
        identity = json.dumps({'bundle': BUNDLE_ID, 'roots': self.root_names}, sort_keys=True)
        self.directory = self.work / 'backups/orbit' / hashlib.sha256(identity.encode()).hexdigest()[:20]
        self.receipt = self.directory / 'manifest.json'

    @property
    def root_names(self):
        return {name: str(root) for name, root in self.roots.items()}

    def check_version(self):
        metadata = self.roots['extension'] / 'metadata.json'
        if metadata.is_symlink() or not metadata.is_file():
            raise Refuse('Orbit metadata.json is missing or a symlink')
        version = json.loads(metadata.read_text())
        if (not isinstance(version, dict) or version.get('uuid') != 'global-menu@unmade.space' or
                version.get('version-name') != '1.1.3'):
            raise Refuse('This offline patch requires Orbit Global Menu 1.1.3')

    def target(self, index):
        rule = RULES[index]
        root = self.roots[rule['root']]
        path = root / rule['path']
        for part in (path, *path.parents):
            if part == root:
                break
            if part.is_symlink():
                raise Refuse(f'Refusing installed symlink: {part}')
        return path

    def fingerprint(self, index):
        path = self.target(index)
        if not path.exists():
            return None, None
        info = path.stat()
        if not stat.S_ISREG(info.st_mode):
            raise Refuse(f'Installed path is not a regular file: {path}')
        return digest(path.read_bytes()), stat.S_IMODE(info.st_mode)

    def guard_backups(self):
        for part in (self.receipt, *self.receipt.parents):
            if part == self.work:
                break
            if part.is_symlink():
                raise Refuse(f'Refusing backup symlink: {part}')

    def original(self, index):
        backup = self.directory / f'{index:02d}.original'
        if backup.is_symlink() or not backup.is_file():
            raise Refuse(f'Original backup is missing or changed: {backup}')
        data = backup.read_bytes()
        if digest(data) != RULES[index]['before']:
            raise Refuse(f'Original backup is missing or changed: {backup}')
        return data

    def load(self):
        self.guard_backups()
        if not self.receipt.exists():
            return None
        if self.directory.is_symlink() or self.receipt.is_symlink():
            raise Refuse('Refusing backup manifest symlink')
        state = json.loads(self.receipt.read_text())
        if (not isinstance(state, dict) or state.get('schema') != 1 or state.get('bundle') != BUNDLE_ID or
                state.get('roots') != self.root_names or not isinstance(state.get('modes'), list) or
                len(state['modes']) != len(RULES)):
            raise Refuse('Backup manifest does not match these roots and this payload')
        for mode in state['modes']:
            if type(mode) is not int or not 0 <= mode <= 0o7777:
                raise Refuse('Invalid original mode in backup manifest')
        if state.get('state') not in {'applying', 'partial', 'installed', 'rolling_back', 'rollback_partial', 'rolled_back'}:
            raise Refuse('Unknown operation state')
        for index, rule in enumerate(RULES):
            if rule['before'] is not None:
                self.original(index)
        return state

    def preflight(self, state, operation):
        if operation == 'apply' and state and state['state'] in {'rolling_back', 'rollback_partial'}:
            raise Refuse('Finish the partial rollback before applying again')
        modes = []
        for index, rule in enumerate(RULES):
            current, mode = self.fingerprint(index)
            modes.append(mode if mode is not None else 0o644)
            phase = state['state'] if state else 'rolled_back'
            allowed = {rule['before']} if phase == 'rolled_back' else {rule['after']}
            if phase in {'applying', 'partial', 'rolling_back', 'rollback_partial'}:
                allowed.add(rule['before'])
            if current not in allowed:
                raise Refuse(f'Unknown installed contents; left untouched: {self.target(index)}')
            if state and mode is not None and mode != state['modes'][index]:
                raise Refuse(f'Installed permissions changed; left untouched: {self.target(index)}')
        return modes

    def save(self, state):
        atomic_write(self.receipt, (json.dumps(state, indent=2) + '\n').encode(), 0o600)

    @contextmanager
    def locked(self):
        base = self.work / 'backups/orbit'
        self.guard_backups()
        base.mkdir(parents=True, exist_ok=True, mode=0o700)
        lock = base / '.installer.lock'
        if base.is_symlink() or lock.is_symlink():
            raise Refuse('Refusing backup lock symlink')
        with lock.open('a+b') as handle:
            try:
                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                raise Refuse('Another Orbit patch operation is running') from None
            yield

    def apply(self, dry_run=False, fault=None):
        self.check_version()
        payloads = []
        for rule in RULES:
            path = self.bundle / 'payload' / rule['root'] / rule['path']
            if path.is_symlink() or not path.is_file():
                raise Refuse(f'Payload file is missing or a symlink: {path}')
            data = path.read_bytes()
            if digest(data) != rule['after']:
                raise Refuse(f'Payload hash mismatch: {path}')
            payloads.append(data)
        state = self.load()
        self.preflight(state, 'apply')
        if dry_run:
            return 'already applied' if state and state['state'] == 'installed' else 'dry-run verified; no files written'
        with self.locked():
            self.check_version()
            state = self.load()
            modes = self.preflight(state, 'apply')
            if state and state['state'] == 'installed':
                return 'already applied'
            self.directory.mkdir(exist_ok=True, mode=0o700)
            if self.directory.is_symlink():
                raise Refuse('Refusing backup directory symlink')
            if not state:
                for index, rule in enumerate(RULES):
                    if rule['before'] is None:
                        continue
                    backup = self.directory / f'{index:02d}.original'
                    data = self.target(index).read_bytes()
                    if digest(data) != rule['before']:
                        raise Refuse('Installed file changed while preparing backups')
                    if backup.is_symlink():
                        raise Refuse('Unrecognized existing original backup symlink')
                    if backup.exists():
                        if digest(backup.read_bytes()) != rule['before']:
                            raise Refuse('Unrecognized existing original backup')
                    else:
                        atomic_write(backup, data, 0o600)
                state = {'schema': 1, 'bundle': BUNDLE_ID, 'roots': self.root_names, 'modes': modes}
            state.update(state='applying', error=None)
            self.save(state)
            try:
                for index, rule in enumerate(RULES):
                    self.preflight(state, 'apply')
                    if self.fingerprint(index)[0] != rule['after']:
                        atomic_write(self.target(index), payloads[index], state['modes'][index])
                        if fault:
                            fault(index)
                state['state'] = 'installed'
                self.save(state)
            except Exception as error:
                state.update(state='partial', error=str(error))
                self.save(state)
                raise
        return 'applied; original bytes and manifest saved'

    def rollback(self, dry_run=False, fault=None):
        self.check_version()
        state = self.load()
        if state is None:
            raise Refuse('No matching patch manifest exists')
        self.preflight(state, 'rollback')
        if dry_run:
            return 'rollback dry-run verified; no files written'
        with self.locked():
            self.check_version()
            state = self.load()
            self.preflight(state, 'rollback')
            if state['state'] == 'rolled_back':
                return 'already rolled back'
            state.update(state='rolling_back', error=None)
            self.save(state)
            try:
                for index, rule in enumerate(RULES):
                    self.preflight(state, 'rollback')
                    if self.fingerprint(index)[0] != rule['before']:
                        if rule['before'] is None:
                            self.target(index).unlink()
                            sync_directory(self.target(index).parent)
                        else:
                            atomic_write(self.target(index), self.original(index), state['modes'][index])
                        if fault:
                            fault(index)
                state['state'] = 'rolled_back'
                self.save(state)
            except Exception as error:
                state.update(state='rollback_partial', error=str(error))
                self.save(state)
                raise
        return 'rolled back; original bytes restored'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['apply', 'rollback'])
    parser.add_argument('--extension-root', required=True)
    parser.add_argument('--daemon-root', required=True)
    parser.add_argument('--work-root', required=True)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    try:
        installer = Installer(args.extension_root, args.daemon_root, args.work_root)
        result = getattr(installer, args.operation)(dry_run=args.dry_run)
        print(result)
        print('Manifest:', installer.receipt)
        return 0
    except (Refuse, OSError, ValueError) as error:
        print('REFUSED/INCOMPLETE:', error)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
