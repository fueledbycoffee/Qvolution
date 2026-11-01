Run `bash run-review.sh --native-only` in the restricted sandbox. It runs
the native red/green controls, rebuilds the committed runtime, builds the
real i386 guest fixtures and installs private images into a `cp -al` farm.
Every build/install takes its own build slot; `ninja -n` must be idle.

Outside the sandbox, run `bash run-review.sh --guest-only EVIDENCE` with
the directory printed by the native run, or `bash run-review.sh` for both
phases. No network, root, privileged mounts, process-name signals or
container timeouts are used. The guest phase holds one guest slot, checks
the six host home folders, initializes an absent short prefix before
copying probes, retries shellspawn once and uses prefix-scoped qv-shutdown.
All farms, prefixes, failures and build output are retained.

The guest fixture binaries contain private, renamed expansions of every
affected i386 wrapper from custom and gen, with a deterministic dispatcher
that returns chosen EDX:EAX words and clobbers ECX. They use the production
errno trampolines and libSystem errno storage. Before/after binaries test
each wrapper independently in two rounds. Old vfork must SIGSEGV; every
other old form must reach the probe's result mismatch, rather than merely
fail to launch. The fixed forms must all pass.

Separate installed-library programs in both architectures exercise real
lseek/EBADF, sigaltstack/EINVAL, syscall(close)/EBADF, gettimeofday/EFAULT
and pipe/EMFILE; i386 also checks the unimplemented LDT traps/ENOSYS. Pipe
exhaustion temporarily lowers only the probe process's soft descriptor
limit, then restores it and closes only descriptors it opened. The program
also checks errno in a pthread and real fork/vfork child/parent returns.
It never changes the per-user process limit to force fork errors.

Sigreturn currently always succeeds; thread_selfid has no safe failing
input. Thread_selfusage and kdebug_trace_string are unimplemented. Their
error translation, plus deterministic fork/vfork failures, is tested with
the injected dispatcher. This proves wrapper behavior, not new handler
implementations. Lseek's current handler is the only full-width INT
handler; the other current handlers/fallbacks return long.

Native controls also execute the adjacent x86_64 fork/vfork paths without
an adapter. Other x86_64 LDT handling and the existing i386 sys_lseek offset
split are outside this change. Guest results are pending until the external
phase runs; a successful compile is not a guest pass.
