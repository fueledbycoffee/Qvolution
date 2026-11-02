"""Strict result classification shared by run.py and native runner checks."""
import re


def problems(row, stdout, stderr, expected_exit=0, required=''):
    bad = []
    if row['timeout']: bad.append('job exceeded its original budget')
    if row['exit'] != expected_exit: bad.append(f"exit {row['exit']} expected {expected_exit}")
    combined = stdout + '\n' + stderr
    if expected_exit == 0:
        if re.search(r'^FAIL\b', combined, re.M): bad.append('fixture reported FAIL')
        if 'reference mismatch' in combined: bad.append('EGL framebuffer differs from CPU/shm oracle')
        if 'Requested AppKit EGL presentation failed' in combined: bad.append('strict EGL presentation failed')
        if 'Wayland damage verification:' in combined: bad.append('stale shm pixels')
        if re.search(r'\bfailures=[1-9][0-9]*\b', combined): bad.append('nonzero fixture failure count')
        for result in row['results']:
            if result.get('failures', 0): bad.append('nonzero JSON failure count')
            if row.get('renderer') == 'egl-verify' and 'swaps' in result:
                if not result.get('checks') or result['checks'] != result['swaps']: bad.append('missing per-frame framebuffer verification')
            if result.get('mode') == 'soak':
                if result.get('frames') != 1000: bad.append('soak did not complete 1000 frames')
                if result.get('fdAt100', -1) < 0 or result.get('fdAtEnd', -1) > result.get('fdAt100', -1): bad.append('soak descriptor growth/unavailable')
                if result.get('rssKbAt100', -1) < 0 or result.get('rssKbAtEnd', -1) - result.get('rssKbAt100', -1) > 8192: bad.append('soak RSS growth/unavailable')
    if required and not re.search(required, combined, re.M): bad.append('required evidence missing: ' + required)
    return bad
