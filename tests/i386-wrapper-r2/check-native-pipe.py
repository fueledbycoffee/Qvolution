#!/usr/bin/env python3
"""Check the installed test's pipe success/exhaustion logic on native Linux."""
from pathlib import Path
import subprocess
import sys

lane = Path(__file__).resolve().parent
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (lane / 'installed-errors.c').read_text()
check = source[source.index('static int check('):source.index('static void *thread_control')]
pipe = source[source.index('static int pipe_error('):source.index('int main(void)')]
c = out / 'pipe.c'
c.write_text('''#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
''' + check + pipe.replace('kernel_pipe(', 'pipe(') + '''
int main(void) { return pipe_success() || pipe_error(); }
''')
binary = out / 'pipe'
subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True)
print('PASS: native Linux test control only; no Darling installed behavior claimed')
