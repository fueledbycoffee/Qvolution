#!/usr/bin/env python3
"""Install real launchd directory rules and exercise its production path guard.

Run under dashboard/qv-slot build on the shared host. No guest boot is needed.
DARLING_LAUNCHD_TEST_OUT retains the generated Ninja project and native guard.
"""
import os
from pathlib import Path
import shlex
import stat
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
DIRECTORIES = [f'{domain}/{kind}' for domain in ('System/Library', 'Library')
               for kind in ('LaunchDaemons', 'LaunchAgents')]


class LaunchdDirectories(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        requested = os.environ.get('DARLING_LAUNCHD_TEST_OUT')
        cls.temporary = None if requested else tempfile.TemporaryDirectory(prefix='launchd-permissions-')
        cls.work = Path(requested or cls.temporary.name).resolve()
        cls.work.mkdir(parents=True, exist_ok=True)
        cls.source = cls.work / 'source'
        cls.source.mkdir(exist_ok=True)
        cls.build = cls.work / 'build'
        # Evaluate the real install declarations; disable only compiled targets.
        man = cls.source / 'man'
        if not man.exists():
            man.symlink_to(ROOT / 'src/launchd/man', target_is_directory=True)
        (cls.source / 'job.plist').write_text('<plist><dict/></plist>\n')
        installs = '\n'.join(f'install(FILES job.plist DESTINATION libexec/darling/{d})'
                             for d in DIRECTORIES)
        (cls.source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.13)
project(launchd_install_regression NONE)
function(add_subdirectory)
endfunction()
''' + installs + f'\ninclude("{ROOT}/src/launchd/CMakeLists.txt")\n' + installs + '\n')
        cls.run_command('cmake', '-G', 'Ninja', '-S', str(cls.source), '-B', str(cls.build),
                        '-DCMAKE_INSTALL_PREFIX=/usr/local')
        cls.run_command('ninja', '-C', str(cls.build), '-j8')
        cls.run_command('ninja', '-C', str(cls.build), '-n')
        source = (ROOT / 'src/launchd/support/launchctl.c').read_text()
        start = source.index('bool\npath_goodness_check(')
        end = source.index('\nvoid\nreadpath(', start)
        # Only logging/program-name calls are adapted; stat and policy are real.
        guard = cls.work / 'path-guard.c'
        guard.write_text('''#define _GNU_SOURCE
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <fnmatch.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#define launchctl_log(level, ...) fprintf(stderr, __VA_ARGS__)
#define getprogname() "launchctl-native-regression"
''' + source[start:end] + '''
int main(int argc, char **argv) {
    return argc == 2 && path_goodness_check(argv[1], false) ? 0 : 1;
}
''')
        cls.guard = cls.work / 'path-guard'
        cls.run_command(*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-Wall', '-Wextra',
                        '-Werror', str(guard), '-o', str(cls.guard))

    @classmethod
    def tearDownClass(cls):
        if cls.temporary:
            cls.temporary.cleanup()

    @staticmethod
    def run_command(*args, **kwargs):
        result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
        if result.returncode:
            raise AssertionError(f'{shlex.join(args)} exited {result.returncode}\n{result.stdout}')
        return result.stdout

    def install_and_check(self, name, mask, existing=False):
        stage = self.work / name
        guest = stage / 'usr/local/libexec/darling'
        if existing:
            for directory in DIRECTORIES:
                path = guest / directory
                path.mkdir(parents=True, exist_ok=True)
                path.chmod(0o775)
        env = dict(os.environ, DESTDIR=str(stage))
        self.run_command('ninja', '-C', str(self.build), '-j8', 'install', env=env, umask=mask)
        for directory in DIRECTORIES:
            path = guest / directory
            with self.subTest(directory=directory, umask=oct(mask), existing=existing):
                result = subprocess.run([str(self.guard), str(path)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o755)
                self.run_command(str(self.guard), str(path / 'job.plist'))

    def test_permissive_install_umask(self):
        self.install_and_check('permissive', 0o002)

    def test_restrictive_install_umask(self):
        self.install_and_check('restrictive', 0o077)

    def test_reinstall_repairs_group_writable_directories(self):
        self.install_and_check('reinstall', 0o002, existing=True)
        self.install_and_check('reinstall', 0o002)

    def test_guard_still_rejects_unsafe_paths(self):
        directory = self.work / 'unsafe'
        directory.mkdir(exist_ok=True)
        for mode in (0o775, 0o757, 0o777):
            directory.chmod(mode)
            self.assertEqual(subprocess.run([str(self.guard), str(directory)], capture_output=True).returncode, 1)
        directory.chmod(0o755)
        job = directory / 'unsafe.plist'
        job.write_text('<plist><dict/></plist>\n')
        for mode in (0o664, 0o646):
            job.chmod(mode)
            self.assertEqual(subprocess.run([str(self.guard), str(job)], capture_output=True).returncode, 1)
        job.chmod(0o644)
        self.run_command(str(self.guard), str(job))


if __name__ == '__main__':
    unittest.main(verbosity=2)
