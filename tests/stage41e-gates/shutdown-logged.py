#!/usr/bin/env python3
"""qv-shutdown launcher adapter: retain only our shutdown command's output."""
import os
import subprocess
import sys

if sys.argv[1:] != ['shutdown']:
    raise SystemExit('shutdown-logged accepts only the helper shutdown operation')
flags = os.O_WRONLY | os.O_CREAT | os.O_APPEND | os.O_NOFOLLOW
with os.fdopen(os.open(os.environ['QV_SHUTDOWN_LOG'], flags, 0o600), 'ab') as log:
    result = subprocess.run([os.environ['QV_REAL_LAUNCHER'], 'shutdown'], stdout=log, stderr=subprocess.STDOUT)
raise SystemExit(result.returncode)
