# Native CoreAudio listener controls

Run from the superproject root on Linux with Clang, C++17, Blocks support,
GNU ld's `--wrap`, and pthreads. Set both paths explicitly:

```sh
BLOCKS_RUNTIME_SOURCE=/path/to/darling-libdispatch/src/BlocksRuntime \
AUDIO_NATIVE_BUILD=/path/outside/checkout/coreaudio-native \
sh qvolution/probes/coreaudio-native/run.sh
```

`BLOCKS_RUNTIME_SOURCE` must contain the published open-source `runtime.c`,
`data.c`, `Block.h`, and `Block_private.h` from the project's libdispatch submodule.
The runner compiles those sources directly, alongside the unchanged production
`AudioHardwareImpl.cpp`, `AudioHardwareStream.cpp`, and
`AudioPropertyListeners.cpp`; it does not use an
installed Blocks library or prebuilt private objects. Record the dependency's git
commit and any local changes with the run. This lane used darling-libdispatch
commit `675b8e7b1591a6641ac724c20d39cefca6940123`, with those four files unchanged.
Outputs go exclusively to `AUDIO_NATIVE_BUILD`; it must be outside the checkout.
`CC` and `CXX` may override the default `clang` and `clang++`.

The local minimal CF/MacTypes/dispatch declarations and deterministic queue adapter
are original test scaffolding. CoreAudio declarations come from the production
vendored headers. The adapter owns real BlocksRuntime captures but deliberately
models queue scheduling and ancestry; passing is **not real libdispatch or guest
runtime evidence**. Its global-queue fallback also does not model real main-thread
queue identity. No IOProc, hardware stream or host audio setting is used.

Controls cover actual format mutation/readback, semantic unchanged suppression,
C/direct/queued listeners, copied address payloads, identity/removal, mixed
registrations, reentrant getter/add/remove, capture ownership and teardown,
concurrent removal of already running C and queued callbacks, complete own C++
allocation sweeps for block registration and queued preparation, and explicit
BlocksRuntime copy failure. StreamFormat aliases share one modeled value, so all
registered scopes/elements receive their own registered address after a change;
allocation failure before publication rolls the entire alias change back.
Concurrent ASBD snapshot readers check coherent channel/width/frame fields, and a
compile-time control rejects exposing a reference to mutable format state. A
controlled interleaving pauses preparation after the registry snapshot: a new
listener registers and reads the old format, then must observe publication.
Structural registry generations guard publication; stale preparations retry outside
locks, at most eight times before an explicit unspecified error with no mutation.
The commit section takes registry then format mutex and performs no allocation,
client call or owner destruction.

An original idle stream exercises actual IOProc create/start/stop/destroy APIs
without Pulse. Semantic format changes return unsupported while backend creation
or any cached stream owns the format, including a stopped stream; destroy all
associated IOProcs before changing format. Same-value setters still succeed
without notification. Backend create/start/stop/destructor reentrant setters prove
no proc-mutex acquisition is needed in callbacks, and failure sweeps verify the
reservation releases after null/throwing construction and map allocation failure.
The format lock is released before all backend work. Dynamic stream reconfiguration
is unfinished; this conservative rejection does not establish real stream support.
IO controls cover visible same/descendant queues and global queues, plus
conservative rejection of unknown/reverse ancestry. They do not prove actual
real-time hardware notification delivery.

Project policy: duplicate registration and absent removal succeed idempotently;
removal matches the original function/client or block/queue identity and exact
registered address. Active cancellation is best effort. Removal does not wait for
an already running callback, and a racing callback can pass its active check
before removal returns. Client-owned C context must remain valid across any such
invocation. No strict no-start-after-return guarantee is claimed. Wildcard-address
semantics and physical Pulse hotplug/default/rate notifications are not established
by these controls.

For real runtime validation use the separate original
[`coreaudio-listener-contract.md`](../coreaudio-listener-contract.md) fixtures.
Their documented Stage26 metadata describes the frozen baseline; current source
also reports StreamFormat settable. Keep baseline artifacts and failures intact.
