# Real libdispatch IO listener controls

This original internal event source compiles the actual production
`AudioPropertyListeners.cpp` unchanged and links the resulting object into a
Darwin executable against the real staged libSystem/libdispatch and libc++.
It uses the actual vendored CoreAudio and dispatch declarations. It includes no
native deterministic adapter, substitute dispatch API, fake success stub, or
minimal native CF declarations. The source event models an explicit own IO-context
change; it does not claim a physical hardware or Pulse event occurred.

Build in an isolated output directory with the exact configured Darwin C++ flags
recorded by the production compile check. The JSON compile record is an array of
`{source, command, exit}` entries; the builder uses the
`AudioPropertyListeners.cpp` command array, replacing only source/output paths and
adding its own header paths/Blocks support. It compiles the real registry again,
records both source hashes and exact commands, and normally links x86_64:

```sh
python3 qvolution/probes/coreaudio-dispatch-io-build.py \
  --compile-record /path/to/private-compile/results.json \
  --sysroot /path/to/stage/usr/local/libexec/darling \
  --linker /path/to/build/src/external/cctools-port/cctools/ld64/src/ld \
  --output /path/outside/checkout/real-dispatch
```

The builder does not execute the binary. Keep its `results.json`, compile/link
logs, binary hash and actual guest runtime manifest with runtime results. Compile
or link success is not a queue scheduling result.

Run one mode per process using the root's canonical locked guest runner and exact
prefix shutdown. Require explicit `PASS_REAL_DISPATCH_IO`, fixture exit0, no
outer timeout and shutdown0 for each. The emitter's semaphore has a three-second
deadline; failure exits immediately so blocked work cannot outlive stack context.

| Mode | Listener queue / emitter | Required result |
| --- | --- | --- |
| `current` | Same original serial queue | Synchronous callback in that queue's specific context; registry retains the queue after caller release. |
| `descendant` | Parent serial listener / child targeting it | Target-specific registry key propagates; callback completes inline in the child's context without ancestor sync deadlock. |
| `global` | Public default global listener / private serial emitter | Callback completes synchronously and its observed context matches direct dispatch_sync to the same global from the same emitter; no global queue-specific storage is installed. |
| `reverse-reject` | Child listener targeting parent / parent emitter | Unsupported before state mutation, zero callbacks. |
| `unknown-reject` | Distinct unrelated serial queues | Unsupported before state mutation, zero callbacks. |

Accepted modes verify copied address payload. Every mode verifies owned captured
C++ state after caller block/capture release, matching removal/destruction, and no
further delivery after removal. Accepted IO callbacks must finish before the event emitter returns;
rejected events must not publish state. The emitter is drained before stack or
semaphore teardown. Queue-context checks use only public specific keys; the public
global context observation uses `dispatch_get_current_queue` and compares to a
direct same-emitter `dispatch_sync` baseline, because synchronous work may use the
calling thread. Output records whether that baseline reports the supplied global,
the emitter, or another runtime queue; a stronger identity guarantee is not assumed. Registration uses an original
heap block whose identity remains alive through registry ownership until removal.

Provenance: vendored public CoreAudio/dispatch declarations and comments (rung2),
public block-listener synchronous-IO exception documentation (rung3), and this
original API-boundary control (rung4). The conservative unknown/reverse ancestry
rejection is explicit project policy. Passing qualifies these own registry/queue
paths only; physical Pulse events, stream reconfiguration and app audio workflows
remain separate requirements.
