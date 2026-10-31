# Native surface storage foundation

This module is a Darling-private native Linux implementation, not Apple ABI.
It implements one dedicated Vulkan BGRA8 optimal image, an initial clear and
release to the same-device external queue family, and one initial opaque-FD
handoff with two binary semaphores. Imports preserve populated storage and
consume a separate owned native descriptor on each successful Vulkan import.
Allocation permits one initial export. Sending consumes its move-only transfer
at call entry: success, validation failure and send failure all release the
sender's three FDs. Sending performs one syscall without an automatic retry;
the consumed owner cannot deliver again to a second recipient. A failed or
ambiguous delivery requires broker retirement/recovery rather than resending
the binary signal. This ownership contract does not authorize explicit FD
duplication or replace authenticated single-recipient broker coordination.
Multi-epoch access and fan-out require the integration described below.
Construction requires a finite caller-supplied initialization wait. Before
submission it reserves a fence, retirement ticket and one of 32 native registry
slots. Timeout or another non-completion error returns the original failure and
ticket while preserving every submitted resource and its device context.
`RetirementTicket::retry` performs one nonblocking fence-status query; only
completion or actual device loss retires resources. `pendingRetirements` and
`pollRetirements` expose the bounded registry's explicit ownership, including
when a caller drops its ticket. Registry exhaustion rejects before submission.
The service must schedule polling and retain the native library while tickets
exist; a deadline never authorizes freeing pending work. Static destruction
does not tear down the process-lifetime registry's unresolved GPU resources.

The device must match both caller-supplied device and driver UUIDs and be a
discrete or integrated GPU. Callers must obtain those UUIDs from the actual
interoperating context. Device names and extension strings are insufficient.
The exact image and binary semaphore combinations are queried before allocation.
All images are dedicated, including when the driver requires dedication.
Opaque FD metadata preserves the exporter allocation size and memory type;
`vkGetMemoryFdPropertiesKHR` is never used for opaque FDs.

The canonical 224-byte little-endian descriptor is version 1. Correlation tags,
IDs, generations and a matching Unix sender are **not authorization**. The
receiver must independently obtain its expected descriptor through genuine
audited Mach capability rights bound to the correct broker prefix and process
generation. That integration is not provided here. Transport uses native Unix
seqpacket sockets, kernel per-message credentials, SCM_RIGHTS and atomic
CLOEXEC receipt. Neither public Apple properties nor scalar guest FDs enter
this API. Receiver cleanup adopts every delivered FD before validation.

No rendering handles are exposed. Broker CPU/GPU epochs, acquire/release
command submission tracking, GL texture lifecycle,
Metal's IOSurface selector and layer presentation are not integrated. Imported
memory is not cleared or transitioned; a later resource/submission layer must
wait on ready and acquire the external image in the transferred GENERAL
layout. GL format/tiling and UUID compatibility, BGRA channel interpretation,
hardware pixels and actual presentation remain unproved. This module must not
be advertised as complete IOSurface GPU support.

Build native source and controls independently:

```
cmake -S src/native/iosurface -B <private-native-build> -DDARLING_SURFACE_NATIVE_CONTROLS=ON
cmake --build <private-native-build> -j2
ctest --test-dir <private-native-build> --output-on-failure
```

The two controls make no GPU calls. Transport uses self-authored memfd payloads
in independently exec'd processes, including malformed/truncated messages,
wrong peers/bindings and FD cleanup checks. Backend controls link an isolated
fake Vulkan implementation instead of libvulkan and inject failures at every
fallible create/import/export operation. These controls are never linked into
the production library. They establish local validation/ownership properties;
they do not establish device capability, valid GPU pixels or guest behavior.

`DARLING_SURFACE_HARDWARE_PROBE=ON` separately builds
`surface-vulkan-hardware-probe`, which links the production library and real
Vulkan loader. It is never a CTest. Configure a reviewed
`DARLING_SURFACE_PROBE_SOURCE_ID` before root schedules enumeration or GPU
execution. It supports actual production import and a separate payload round
trip, same-process/cross-exec, creator drop and reverse producer roles. Each
endpoint creates a distinct Vulkan instance and matches exact image metadata;
the harness adds no raw-handle access to production objects. Only the payload
case checks shared pixels, nonzero readback sentinels and padded canaries.
Initial ready is waited once, released is signaled/waited once, and directional
return-reader FDs are retained explicitly rather than cloning an initial grant.

The probe uses finite fence/channel/child-exit waits. Failed GPU work remains
in a typed bounded registry; its controller reports its PID/owned child and
holds the process while polling for known completion/device loss. SIGINT and
SIGTERM request stop but do not free pending resources. Do not wrap a root GPU
run in a process-killing timeout. Root must retain and supervise any reported
retirement hold. The probe does not programmatically collect validation-layer
messages from production-private VkInstances; when root enables a validation
layer, its captured diagnostics are a separate acceptance check.

The additional `surface-probe-options-control` is CPU-only and links no Vulkan
loader. It checks parsing bounds and proves untouched/zero buffers cannot pass
the sentinel/pixel/canary detector. It supplies no hardware evidence.

`--fd-audit` records the native process's descriptor number, readlink target,
stat identity and descriptor/status flags at allocation/export/send/import/
creator-drop/retirement/discovery boundaries. Its own temporary directory FD
is excluded from snapshots. The strict gate compares complete stable snapshots
after the first run, including FD numbers and flags, and fails on identity
replacement even when counts match. Counts now exclude the scan directory.
The first-run baseline still cannot rule out a resource lost on the first run.
No device path/inode is exempt: opaque exports can share the driver's inode.
Stat stability
does not prove shared open-file-description identity or authorize closing a
driver-owned FD. The audit is evidence collection, not a driver-cache waiver.
Diagnostic JSON escapes non-ASCII/control bytes as `\u00XX`, preserving distinct
native path bytes instead of replacing them with question marks.
`--enumeration-only --fd-audit --runs 2` isolates instance/physical-device
discovery without logical devices, allocation/export or submitted work. It
retains the strict post-first-run FD gate and never qualifies GPU storage.
`--enumeration-only --enumeration-passes 32 --runs 1 --fd-audit` enumerates
32 times in a single instance, then destroys it. Compare its API-boundary
snapshots with `--enumeration-only --runs 2 --fd-audit` to distinguish retention
per physical-device enumeration from retention per instance lifecycle. This
diagnostic adds no logical device or storage, closes no driver FDs, and cannot
establish that a retained descriptor is expected process-lifetime state.
The new native-only `surface-probe-fd-control` verifies actual memfd duplicate
identity/flags and that the auditor itself neither retains nor changes FDs.
Its `--negative-control` deliberately loses an owned duplicate while closing
an unrelated spare. The count stays unchanged; the identity gate must exit 1.

Provenance: clean `gpu-surface-direct-primary-next/SPEC.md` sections 4–5 and
`SOURCE-LEDGER.md`; official Khronos Vulkan memory/resources/synchronization
contracts and `EXT_external_objects`/`EXT_external_objects_fd`. Linux descriptor
ownership follows the owned darlingserver SCM_RIGHTS transport source. No
Apple-private constants, proprietary SDK contents or binary inspection were
used. The initial BGRA8 subset, one initial handoff and canonical protocol are
explicit Darling design choices.
