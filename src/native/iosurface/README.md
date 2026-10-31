# Native surface storage foundation

This module is a Darling-private native Linux implementation, not Apple ABI.
It implements one dedicated Vulkan BGRA8 optimal image, an initial clear and
release to the same-device external queue family, and one initial opaque-FD
handoff with two binary semaphores. Imports preserve populated storage and
consume a separate owned native descriptor on each successful Vulkan import.
The initial ready signal can be transferred only once; multi-epoch access and
fan-out require the broker/submission integration described below.
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

Provenance: clean `gpu-surface-direct-primary-next/SPEC.md` sections 4–5 and
`SOURCE-LEDGER.md`; official Khronos Vulkan memory/resources/synchronization
contracts and `EXT_external_objects`/`EXT_external_objects_fd`. Linux descriptor
ownership follows the owned darlingserver SCM_RIGHTS transport source. No
Apple-private constants, proprietary SDK contents or binary inspection were
used. The initial BGRA8 subset, one initial handoff and canonical protocol are
explicit Darling design choices.
