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

Round 3 splits task-self, reply-port and bootstrap-port initialization, MIG
TSD access, socket acquisition, signal masking, and RPC send/receive. The
cached i386 record flag is cleared when mach_init returns; the default is quiet.
There is no host-port lookup in this mach_init; host_self has its own trap.

The i386 loader now installs a temporary FS TSD page before dyld (Linux TLS
stays in GS), matching x86_64's initial GS page. The i386 kernel setter reuses
that descriptor when dyld/libpthread or dynamic libsystem_kernel take over.
`mldr-initial-tsd.c` reads/writes the MIG TSD slot, replaces its base through
two compiled copies of the real kernel setter, and checks native Linux TLS.
Its `--no-install` control must fault on the initial TSD read. ELF32 execution
requires a host without the sandbox's ia32 seccomp restriction.

External rerun (container-capable host only):

```
bash parallel/i386-guest-r3/run-review.sh
```

The runner first builds and executes the native ELF32/ELF64 TSD controls, then
invokes the rootless Stage41d baseline/candidate hello suite. It creates a fresh
hardlink farm and replaces candidate images with private inodes, verifying the
unchanged source manifest and hashes. No root, mounts or system setting changes.
It enforces the disk floor, commits-before-install, fresh short prefixes,
separate build/guest slots, host-folder guards and prefix-scoped shutdown.
Do not use it on an app bundle or replace the hello with an Apple executable.
