# IOSurface CPU sharing regression gates

The source-authored `typed-callers.c` is frozen from the clean public specification
(SHA-256 `ee092af5727269fb6a5e4e45d2e64a1bf5d5f534f0645b4fd9067f529eb92862`).
Keep its expectations unchanged unless new permitted evidence changes the contract.
The specification SHA-256 is
`3ec06cdd59f900aaab76f2bbc82ba48bee2ee5315d23f32d6979af0f5639ffc6`.

This is a CPU milestone. Nonzero CoreVideo lock flags, requested CV row alignment,
GPU compatibility/import and Electron workflow acceptance remain unsupported or
unverified. No positive GPU capability follows from successful memfd sharing.

## Lightweight native source tests

Use an already configured Darling build whose source/generated headers come from
permitted vendored source. The driver reads `ninja -t commands`; it never invokes a
build, CMake, installation or a guest:

```sh
python3 tests/iosurface-sharing/run-native.py /path/to/configured/build --output /tmp/iosurface-native
```

Output defaults to a temporary directory if `--output` is omitted. The tests use
actual production source/functions and explicitly labeled boundary adapters:

- Frozen callers and guest runner: strict target-header compilation with assertions
  enabled and fatal implicit declarations/incompatible function/object pointers.
- Layout and private protocol headers: padded BGRA/420v, overflow/extent/overlap,
  malformed descriptor/size/version/payload tables and 32/64-bit wire-size checks.
- Actual `memory.c` and vendored Apple-origin `refcnt.c`: real memfd/MAP_SHARED
  conversion and two-way writes, partial-unmap offset reimport, failed-unmap
  atomicity, half-open adjacency and descriptor fd close. Thread/map hooks are
  local adapters; these results do not establish Mach task security.
- Exact broker lifecycle methods: pre-map allocation failure, bad protection,
  failed-unmap ownership/retry, lease notification cleanup, death followed by
  scalar task-name reuse, balanced suspended-source cleanup and weak-ID pruning.
- Exact RPC helper: partial receive-body cleanup, outbound COPY_SEND preservation
  on send failure, malformed reply destruction and lease-right ownership transfer.
- Exact authorization prefix: failed `task_info`, incorrect count, each audit word
  differing, exact match, and malformed messages rejected before task inspection.

The baseline public headers fail frozen caller compilation at the untyped
GetIOSurface return. The baseline kernel source also fails the adjacency/lifetime
regression. These are distinct from an actual pre-fix guest constructor failure.

## Actual guest gate

Compile `guest-runner.c` with `-UNDEBUG`, corrected public headers and the actual
CoreFoundation/CoreVideo/IOSurface/IOKit/system libraries from a recorded candidate
runtime. Run using a guest executable path that remains executable across `exec`.
Only the coordinating agent schedules runtime builds, staging and guest runs.

The runner executes the unchanged frozen callers and adds:

- Separately exec'd global-ID and real Mach descriptor recipients, parent/child
  write-through, creator release and imported CV-wrapper lifetime.
- Port-only lifetime, independent manual process use counts, IOSurface read-only
  seeds, invalid layouts/types/rights, unsupported GPU/alignment attributes and
  genuine pool allocation without reuse.
- A genuine foreign task send right transferred to a separately exec'd sender;
  its request must fail protection with no mapping/ID/lease output, while its own
  task path succeeds.
- A deterministic lost-reply gate: wait until an acquisition reply is actually
  queued, destroy its receive port unread, drop the original surface, and verify
  that the lease notification reclaims the final mapping/ID.

The inherited registered port is only a rendezvous send right. Surface identity
travels in a Mach port descriptor, never as a task-local scalar port name. Every
created receive/send right and registered stash is released.

A passing native result does not establish the guest results. A passing CPU guest
result does not establish GPU sharing. Required later gates remain real host GPU
allocation/import and coherence, cross-process GPU/CPU writes, and recorded
unmodified Discord, VS Code and Obsidian workflows.

## Permitted contract sources

Rung 2: vendored public headers and current Darling source. Rung 1: vendored
Apple-origin XNU IPC/task source and IOKitUser ownership declarations. Rung 3:
Apple's public [CV constructor](https://developer.apple.com/documentation/corevideo/cvpixelbuffercreatewithiosurface(_:_:_:_:)),
[IOSurface Mach export](https://developer.apple.com/documentation/iosurface/iosurfacecreatemachport(_:)),
[ID lookup](https://developer.apple.com/documentation/iosurface/iosurfacelookup(_:))
and [plane access](https://developer.apple.com/documentation/corevideo/cvpixelbuffergetbaseaddressofplane(_:_:))
contracts, frozen in the separate clean specification ledger. The normalized
layout and descriptor protocol are explicitly Darling-private designs, not Apple
private ABI or inferred proprietary implementation details.
