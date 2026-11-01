#!/usr/bin/env python3
"""Run only lane-owned syscall-only programs; never change host settings."""
from pathlib import Path
import json, platform, resource, signal, subprocess
L = Path('/home/sean/Documents/Codex/Projects/Qvolution-work/parallel/i386-guest')
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
report = {'kernel': platform.release(), 'cmdline': Path('/proc/cmdline').read_text().strip()}
config = Path('/boot/config-' + platform.release())
report['config'] = [s for s in config.read_text().splitlines() if s.startswith(('CONFIG_IA32_EMULATION=', 'CONFIG_X86_VSYSCALL_EMULATION=', 'CONFIG_COMPAT=', 'CONFIG_IA32_EMULATION_DEFAULT_DISABLED=', '# CONFIG_IA32_EMULATION_DEFAULT_DISABLED'))] if config.exists() else 'unavailable'
report['process'] = [s for s in Path('/proc/self/status').read_text().splitlines() if s.startswith(('Seccomp:', 'Seccomp_filters:', 'NoNewPrivs:'))]
report['ia32_sysctl'] = Path('/proc/sys/abi/ia32_emulation').read_text().strip() if Path('/proc/sys/abi/ia32_emulation').exists() else 'not exposed'
for arch in ('x86_64', 'i386'):
    result = subprocess.run([str(L / 'build' / ('host-' + arch))], capture_output=True)
    report[arch] = {'returncode': result.returncode, 'signal': signal.Signals(-result.returncode).name if result.returncode < 0 else None}
print(json.dumps(report, indent=2))
raise SystemExit(0 if report['x86_64']['returncode'] == report['i386']['returncode'] == 0 else 1)
