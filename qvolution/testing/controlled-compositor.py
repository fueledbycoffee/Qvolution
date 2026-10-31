#!/usr/bin/env python3
"""Prepare/launch only an explicitly requested, dedicated Qvolution headless shell.

Default is plan-only. No desktop input, screenshots, existing socket discovery,
package installation or changes to the real session. Root coordinates launches.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time

MODE = {
    'hasWindows': True,
    'hasOverview': False,
    'showWelcomeDialog': False,
    'allowSettings': False,
    'allowScreencast': False,
    'hasRunDialog': False,
    'hasWorkspaces': False,
    'hasNotifications': False,
    'hasWmMenus': False,
    'components': [],
    'enabledExtensions': [],
    'panel': {'left': [], 'center': [], 'right': []},
}
MODE_NAME = 'qvolution-controlled'
SOCKET_NAME = 'qvolution-controlled'


def write_private(path, data):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_CLOEXEC, 0o600)
    with os.fdopen(fd, 'w') as stream:
        stream.write(data)


def inside(run):
    # This branch runs only inside the NEW dbus-run-session child.
    if os.environ.pop('QVOLUTION_CONTROLLED_COMPOSITOR_RUN', None) != str(run):
        raise SystemExit('Internal child marker absent')
    if not os.environ.get('DBUS_SESSION_BUS_ADDRESS'):
        raise SystemExit('Dedicated session bus absent')
    write_private(run / 'client-env.json', json.dumps({
        key: os.environ[key] for key in [
            'DBUS_SESSION_BUS_ADDRESS', 'XDG_RUNTIME_DIR',
            'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME', 'XDG_STATE_HOME',
            'XDG_DATA_DIRS', 'GSETTINGS_BACKEND']
    } | {'WAYLAND_DISPLAY': SOCKET_NAME}, indent=2) + '\n')
    os.execv('/usr/bin/gnome-shell', [
        'gnome-shell', '--wayland', '--headless', '--no-x11',
        '--virtual-monitor=1024x768@60', '--wayland-display=' + SOCKET_NAME,
        '--mode=' + MODE_NAME])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', type=Path, required=True,
                        help='NEW absolute Qvolution scratch directory; never reuse existing runs')
    parser.add_argument('--launch', action='store_true', help='Actually launch; coordinator only')
    parser.add_argument('--inside', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    run = args.run_dir
    if not run.is_absolute():
        parser.error('run-dir must be absolute')
    if run.parent.resolve() != Path(__file__).resolve().parent:
        parser.error('run-dir must be a new direct child of this Qvolution task scratch')
    if args.inside:
        inside(run)
    if not shutil.which('dbus-run-session') or not Path('/usr/bin/gnome-shell').is_file():
        raise SystemExit('Installed gnome-shell and dbus-run-session required')
    plan = {
        'runDirectory': str(run),
        'runtimeDirectory': 'new private /tmp/qv-wl-* directory (0700)',
        'waylandSocket': SOCKET_NAME,
        'virtualOutput': '1024x768@60',
        'sessionMode': MODE_NAME,
        'freshDBus': True,
        'memoryGSettings': True,
        'hostDisplayConnectionRequested': False,
        'hostSettingsWritesRequested': False,
        'softwareQualification': False,
        'launchRequested': args.launch,
    }
    print(json.dumps(plan, indent=2), flush=True)
    if not args.launch:
        return
    # Refuse all existing paths, including symlinks and another task's session.
    run.mkdir(mode=0o700, parents=False, exist_ok=False)
    runtime = Path(tempfile.mkdtemp(prefix='qv-wl-', dir='/tmp'))
    write_private(run / 'runtime-path.txt', str(runtime) + '\n')
    for name in ['config', 'data', 'cache', 'state']:
        (run / name).mkdir(mode=0o700)
    modes = run / 'data' / 'gnome-shell' / 'modes'
    modes.mkdir(mode=0o700, parents=True)
    write_private(modes / (MODE_NAME + '.json'), json.dumps(MODE, indent=2) + '\n')
    env = os.environ.copy()
    for key in list(env):
        if key in {
            'DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
            'DBUS_SESSION_BUS_ADDRESS', 'DBUS_STARTER_ADDRESS', 'DBUS_STARTER_BUS_TYPE',
            'DESKTOP_AUTOSTART_ID', 'GNOME_DESKTOP_SESSION_ID', 'SESSION_MANAGER',
            'GNOME_SHELL_SESSION_MODE', 'GNOME_SETUP_DISPLAY', 'XDG_SESSION_ID',
            'LIBGL_ALWAYS_SOFTWARE', 'MESA_LOADER_DRIVER_OVERRIDE', 'GALLIUM_DRIVER',
        } or key.startswith('MUTTER_DEBUG_'):
            env.pop(key, None)
    env.update({
        'XDG_RUNTIME_DIR': str(runtime),
        'QVOLUTION_CONTROLLED_COMPOSITOR_RUN': str(run),
        'XDG_CONFIG_HOME': str(run / 'config'),
        'XDG_DATA_HOME': str(run / 'data'),
        'XDG_CACHE_HOME': str(run / 'cache'),
        'XDG_STATE_HOME': str(run / 'state'),
        'XDG_DATA_DIRS': str(run / 'data') + ':/usr/local/share:/usr/share',
        'GSETTINGS_BACKEND': 'memory',
        'GIO_USE_VFS': 'local',
        'NO_AT_BRIDGE': '1',
    })
    command = ['dbus-run-session', '--', sys.executable, str(Path(__file__).resolve()),
               '--run-dir', str(run), '--inside']
    log_fd = os.open(run / 'compositor.log', os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    child = None
    try:
        with os.fdopen(log_fd, 'w') as log:
            child = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                                     start_new_session=True)
            def stop(signum, frame):
                # Only the process group this exact Popen created; no name/PID search.
                if child.poll() is None:
                    os.killpg(child.pid, signal.SIGTERM)
            signal.signal(signal.SIGINT, stop)
            signal.signal(signal.SIGTERM, stop)
            deadline = time.monotonic() + 25
            path = runtime / SOCKET_NAME
            while time.monotonic() < deadline and child.poll() is None:
                try:
                    if stat.S_ISSOCK(path.lstat().st_mode):
                        print('SOCKET_READY: client-env.json prepared; compositor/renderer/window readiness still requires evidence', flush=True)
                        break
                except FileNotFoundError:
                    pass
                time.sleep(0.05)
            else:
                stop(signal.SIGTERM, None)
                raise RuntimeError('Dedicated socket startup failed; inspect private compositor.log')
            return child.wait()
    finally:
        if child is not None and child.poll() is None:
            os.killpg(child.pid, signal.SIGTERM)
            try:
                child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
        # Remove only our empty private socket directory; preserve evidence.
        try:
            runtime.rmdir()
        except OSError:
            pass


if __name__ == '__main__':
    raise SystemExit(main())
