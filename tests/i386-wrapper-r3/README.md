# i386 stack allocation collision

Run outside the restricted sandbox:

    bash /home/sean/Documents/Codex/Projects/Qvolution-work/parallel/i386-wrappers-r3/run-external.sh

The candidate is built in this lane; the inherited i386-guest clone had
concurrent uncommitted mldr edits, so this superproject-only sparse worktree
starts from round-2 HEAD 28dcf838b6. No submodule worktree is populated or moved.
Both mldr variants replay the inherited compiler/linker commands into build/;
vendored SDK/generated headers are read from the populated Stage41d inputs.

The fixed mmap below the commpage runs before the Mach-O header is read.
An occupied Linux VMA produces EEXIST. The fixture's image and guest threads
do not yet exist. Native i386 initial-stack ASLR overlap is a hypothesis until
the external native ELF observer records it; the old logs lack a memory map.

The fix uses a hint, keeps the returned address, and reserves the finite
RLIMIT_STACK span so relocation leaves usable stack capacity. Anonymous pages
remain demand-paged. Unlimited limits retain kernel grow-down behavior.
No fixture address workaround, fixed overwrite, or EEXIST retry is used.

Native x86_64 tests exercise the production block: free, full collision and
partial collision; 128 KiB, 8 KiB and unlimited limits; 20 repetitions each.
Another 180 cases use the actual i386 commpage boundary in a 64-bit process.
These are not i386 execution. Sandbox native i386 startup is killed by SIGSYS.
All failed development evidence is retained.

The external runner performs no builds/installs: it verifies committed runner
snapshots, candidate provenance and fixture hashes, runs the prebuilt native
i386 regression and baseline, observes 1024 authored native ELF startups, then
runs 84 guest cases. Each affected wrapper case and its counterpart runs 20x;
old wrappers must reach their expected mismatch diagnostic, new wrappers pass.
Installed i386/x86_64 API controls run twice each. Only the documented
shellspawn startup timeout gets one retry, preserving the first attempt.

It uses a lane-owned cp -al farm with five private runtime images (link count
1), since the inherited farm changed concurrently. It uses the non-setuid
lane launcher, rootless environment, fresh short owned prefix, init/shutdown
before copying fixtures, dashboard/bin PATH, host-folder guard and qv-shutdown.
Guest logs and native observations go to a fresh evidence directory each run.
