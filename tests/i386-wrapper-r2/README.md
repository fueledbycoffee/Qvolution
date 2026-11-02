The native entry point is `bash run-review.sh --native-only`. It requires clean
committed source and runner snapshots, retains the old low-mapping rejection,
checks old and current wrappers with EMFILE, runs a native Linux control of the
installed test logic, rebuilds runtime/fixtures and reuses the existing hardlink
farm. Builds/install work takes build slots and ninja -n must be idle.

The external entry point is `bash run-external.sh`. It consumes the completed
native evidence, verifies provenance/farm/mappings, then runs 116 guest cases in
two rounds. The runner initializes a fresh short rootless prefix before copying
fixtures, guards the six host folders, retries shellspawn once and shuts down
only its own prefix with qv-shutdown. No nested slots, deletion or host changes.
A rerun requires a new native evidence directory, preserving the previous logs.

The i386 fixture links use -pagezero_size 0x10000, verified by llvm-otool -l
on these authored binaries only. Both installed architectures test pipe data
transfer and path-free dup exhaustion, then require syscall(SYS_pipe) and the
named pipe wrapper to fail with EMFILE without touching either output array.
The old open-based test could run out of temporary path-expansion descriptors;
this is a hypothesis, not a measured explanation of the previous guest result.

The existing pipe wrapper already compares EAX with -4095 independently of EDX.
Its expanded injected EMFILE controls pass before any round-two runtime edit.
No runtime instructions were changed without a failing regression. Installed
Darling behavior remains unverified until the external command passes; if it
fails, its direct/wrapper diagnostics narrow the next investigation.
