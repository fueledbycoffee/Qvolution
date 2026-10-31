# Launcher readiness contract

Run from the repository root on Linux with Python3 and a C compiler:

```sh
python3 tests/startup-ready.py
```

`CC` selects the compiler. The runner compiles the actual `spawnInitProcess`
function and `src/startup/startup-ready.h` with controlled runtime-location and
environment helpers. Authored native-server controls exercise real fork, exec
and pipe behavior. No configured build tree, installed runtime, prefix boot,
namespace mounts, guest program or vendor image is needed. All generated files
and executables are temporary.

Valid readiness continues to the caller. EOF, an invalid byte and an injected
non-EINTR read error must exit before the caller marker. Injected EINTR must
perform another read and receive the byte before continuing. Every server
helper must report completion, rejecting an unrelated helper failure as a pass.

The original function violates these contracts:

```sh
python3 tests/startup-ready.py --source-revision 96f7565
# expected exit 1: EOF, invalid byte, EINTR and EIO controls fail
```

The controlled helpers limit this test to the launcher handshake. Full server
builds, namespace/prefix setup and repeated real guest startups remain separate
integration gates.

Controls run in fresh sessions with bounded waits and temporary-file output. The
test process becomes a child subreaper so that a timed-out controller's authored
server children, including children that call setsid, can be stopped and reaped
without signalling host peers. A dedicated blocked controller/child control checks
that cleanup; timeouts in the readiness cases fail the test.

The blocked child acknowledges its setup before the controller flushes a ready
marker; the short cleanup deadline starts only after that marker is observed.
The supervisor resets SIGCHLD to SIG_DFL before spawning, so an inherited ignored
disposition cannot invalidate child ownership or reaping.

Owned children are finalized after normal controller exits too (after the
launcher's helper-completion acknowledgment). Lingering children fail ordinary
controls and are stopped/reaped before the runner returns. A second supervision
control deliberately exits the controller while its acknowledged child remains
alive; the suite requires detection and cleanup of that condition.
