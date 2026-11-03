# Launcher shutdown isolation

## Prefix initialization state

Run without root or any Darling container:

```sh
cc -Wall -Wextra -Werror -O2 tests/launcher/prefix-state.c -o /tmp/darling-prefix-state-test
/tmp/darling-prefix-state-test
```

An empty pre-created directory is treated like an absent prefix and follows
normal setup. A non-empty directory must contain the structural files created
by `setupPrefix()`; otherwise the launcher fails closed instead of overwriting
user data.

## Profile selection

Run without root or any Darling container:

```sh
cc -Wall -Wextra -Werror -O2 tests/launcher/profile-prefix.c -o /tmp/darling-profile-prefix-test
/tmp/darling-profile-prefix-test
```

This tests the launcher's pure prefix selector. `DPREFIX` remains the explicit
path interface. `DARLING_PROFILE=<name>` selects the isolated sibling prefix
`~/.darling.<name>`; using both selectors or a name containing path separators
is rejected. The harness does not create or access any prefix.

## Root-mode /proc

Run without root or any Darling container:

```sh
cc -Wall -Wextra -Werror -O2 tests/launcher/proc-dir.c -o /tmp/darling-proc-dir-test
/tmp/darling-proc-dir-test
```

Non-root mode makes the prefix's `/proc` a symlink to the host procfs, and root
mode must mount a procfs for launchd's PID namespace on a directory there. The
root-mode launcher replaces exactly that symlink with a directory; any other
entry, including a symlink with a different target, is left alone. The check
runs with the invoking user's ids.

## Shutdown isolation

Run without root or any Darling container:

```sh
cc -Wall -Wextra -Werror -O2 tests/launcher/shutdown-isolation.c -o /tmp/darling-shutdown-isolation-test
/tmp/darling-shutdown-isolation-test
```

This exercises the launcher's actual shutdown helpers on a private process tree.
The target children and grandchildren ignore SIGTERM, checking the SIGKILL phase
and preservation of process handles after reparenting. An unrelated process
named darlingserver and the test caller must survive. A different prefix must
fail the server identity check. The test reaps its children, including orphans.
Linux pidfd syscalls are required; unsupported kernels fail closed.

## Orphans of a dead server

Run without root or any Darling container:

```sh
cc -Wall -Wextra -Werror -O2 tests/launcher/orphan-cleanup.c -o /tmp/darling-orphan-cleanup-test
/tmp/darling-orphan-cleanup-test
```

Nonroot containers have no PID namespace, so when darlingserver dies its guests are
reparented to init and no longer descend from any server. `shutdownOrphans()` finds
them by exact prefix socket environment, the invoking user and an `mldr` executable
path (a replaced binary's ` (deleted)` suffix is ignored). The test copies itself to
two `mldr` paths and checks that a live server for the prefix blocks reaping; that
after it dies the prefix's guests, including a grandchild, a SIGTERM-resistant child
and one running a since-replaced binary, are stopped; and that a non-`mldr` process,
another prefix's guest and a process without the socket environment survive. The
prefix is the run's own temporary directory.

## Inspection failure on /proc/<pid>/cmdline

Run without root or any Darling container:

```sh
cc -Wall -Wextra -Werror -O2 -Wl,--wrap=open tests/launcher/proc-permission.c -o /tmp/darling-proc-permission-test
/tmp/darling-proc-permission-test
```

Under `/proc` mounted with `hidepid=1` or `hidepid=2`, inspecting another process's
`cmdline` requires `PTRACE_MODE_READ_FSCREDS`, which fails with `EACCES` or `EPERM`
when GIDs differ (for example, after `newgrp`). `shutdownServerMatches()` separates
inspection failure (-1) from mismatch (0), so `shutdownOrphans()` and `darling shutdown`
fail closed instead of treating an unreadable server as absent and killing guests
or spawning a duplicate server.

For container integration testing, use the fixed launcher with two disposable
prefixes. Boot both, shut down the first, check the second server PID remains
alive and serves another request, then shut down the second. Do not run the
historical launcher's shutdown path alongside any valuable running container.

Nonroot shellspawn has a separate process tree. The launcher records its host PID
at startup and validates its user and exact prefix socket environment before
obtaining a shutdown target. Existing nonroot containers started by an older
launcher lack this record and are refused rather than falling back to a global
process-name search. Start disposable nonroot tests with the fixed launcher.

## Native socket pathname capacity

```sh
cc -Wall -Wextra -Werror -O2 tests/launcher/prefix-socket-paths.c -o /tmp/darling-prefix-socket-test
/tmp/darling-prefix-socket-test
```

Prefix selection must reserve the full darlingserver and shellspawn socket suffixes
and the terminating NUL within the native `sockaddr_un.sun_path` field. On Linux,
its 108-byte capacity permits socket pathname lengths of 107 bytes and rejects
108. The test exercises explicit prefixes, default HOME-based prefixes, named
profiles and UTF-8 byte lengths at that boundary. The old 255-byte prefix limit
accepted paths which later truncated socket addresses or overflowed the launcher
connection buffer. Because `darlingSelectPrefix` is called before `checkPrefixDir`
and `setupPrefix`, rejection precedes filesystem or server creation.

This regression fails on the parent selector and passes after reserving socket
suffix capacity. Real short unique cold-prefix PTY and `/bin/echo` controls passed;
intentionally overlong diagnostic prefixes timed out and left socket files at
truncated paths. These are path-length results, not a generic cold-start failure.

## Daemon descriptors and reparented PTY sessions

Round-2 native tests are `daemon-stdio.c` and `shutdown-pty.c`. Compile with
`cc -Wall -Wextra -Werror -O2 tests/launcher/<name>.c -lutil -o <output>`.
The former calls the exact launcher redirect helper and verifies pipe EOF while
its daemon stays alive, including debug/zero-valued logging flags, closed stdio,
and rejection of missing, symlinked or non-regular log destinations. The latter
uses only authored processes: a session leader owns a PTY, ignores TERM and is
reparented before shutdown. It must be selected by exact socket/uid/executable,
then stopped by KILL after the grace period; an other-prefix PTY survives and
exits through a test-owned control pipe. Neither test boots a container. Files
are retained. Daemon stderr now belongs to the prefix's launcher-daemons.log;
do not inspect it for guest applications under the clean-room boundary.

## Concurrent boot and exec descriptors

Round 4 adds `prefix-lock.c`: two clients enter the same cold check/boot/PID
publication window. The directory lock permits one boot; `--without-lock`
reproduces two boots (expected exit 1). A forked daemon closes its copy; the
parent lock remains exclusive until explicitly unlocked and never covers the
subsequent shell wait. No container or socket is created by this test.
`daemon-stdio.c` now verifies usable 0–2 after setsid and exec, including
initially closed descriptors. `shutdown-pty.c` checks TERM delivery to a
reparented PTY leader plus login child, grace, KILL, and other-prefix survival.
The runtime shutdown implementation and its one-second grace are retained.
