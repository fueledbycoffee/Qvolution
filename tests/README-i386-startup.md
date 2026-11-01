# i386 startup records

Set `DARLING_I386_STARTUP=1` for a lane-owned i386 executable to emit fixed
`[darling-i386]` stage records to stderr. Other values leave records disabled.
The x86_64 mldr emits only the transition to mldr32; 64-bit guest startup emits
no records. mldr passes an opt-in apple-vector item to both copies of libkernel.
These diagnostics contain only our stage names, no program data or addresses.

Stages distinguish re-exec, mldr32 entry, commpage and stack mapping, server
check-in, dyld mapping and handoff, dyld bootstrap/rebasing/signal setup, Mach
ports/CPU/pthread setup, and the libSystem-to-libkernel initialization boundary.
A missing record only narrows the failure after the preceding confirmed stage;
verify that the candidate files were installed and the environment was passed.
Records are best effort: a closed stderr can suppress them.

Before testing Darling, run a syscall-only ELF32 control outside any restrictive
sandbox. `CONFIG_IA32_EMULATION=y` alone does not prove usable 32-bit syscalls:
check `CONFIG_IA32_EMULATION_DEFAULT_DISABLED`, `/proc/cmdline`, and seccomp.
The i386 kernel path uses `int $0x80`, independent of x86_64 legacy vsyscall.
Never change host settings as part of this test.

The lane's `parallel/i386-guest/build-runtime.sh` replays configured Stage41d
commands in its private build directory. It builds both mldr executables,
universal dyld/libsystem_kernel, syscall controls and libSystem-only hellos.
`tests/mldr-i386-stack.c` checks the complete production 32-bit stack constructor
natively; it is a layout check, not a guest acceptance test. The companion xnu
`darling/tests/i386-startup-records.c` checks exact opt-in parsing.

External rerun (container-capable host only):

```
bash parallel/i386-guest/run-review.sh --mount-overlay
```

This option uses `sudo -n mount` to create a lane-owned overlay of Stage41d;
without it the runner requires an already mounted `parallel/i386-guest/stage`.
It never creates a full stage copy. Mounting requires the external executor's
mount privileges; no kernel settings are changed.
The script enforces the disk floor, commits-before-install, fresh prefixes,
separate build/guest slots, host-folder guards and prefix-scoped shutdown.
Do not use it on an app bundle or replace the hello with an Apple executable.
