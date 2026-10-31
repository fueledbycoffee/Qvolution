# Hardware GPU primitives

These source-built guest probes qualify individual API paths, not applications.
Build with `ENABLE_TESTS=ON`; when AppKit and the relevant framework targets exist,
`qvolution_gpu_opengl` and `qvolution_gpu_metal` are available under the build's
`qvolution/probes/` directory. Run in a dedicated nonroot staged Darling prefix,
without `DYLD_*` overrides, using the native Wayland backend, then shut down that
exact prefix. Use an outer timeout. No desktop extension or driver is modified.

`qvolution_gpu_opengl` requests a 3.2 core context, reports renderer/vendor/version,
rejects known software renderers, compiles an original GLSL shader, renders a
triangle into an offscreen texture and compares a pixel readback. With
`--negative-control` it expects a deliberately wrong pixel and must fail. This is
not a presentation, frame-time, scaling, power-management or application test.

Metal is intentionally gated by the current runtime. `qvolution_gpu_metal`
reports failure without a device by default. Use `--enable-experimental-metal`
to opt in **only for that test process**. It reports the device, rejects known
software devices, creates a shared buffer and submits a real Metal blit fill;
the existing backend uses Vulkan `vkCmdFillBuffer`. The test waits and checks
all 4096 returned bytes. Add `--negative-control` and require failure. This is
not a Metal shader/render-pipeline, AIR translation, MetalKit or application pass.
The current command-buffer headers do not expose a status query; correctness is
measured by the actual readback plus the implemented error interface.

Observed baseline on the RTX 4070 / NVIDIA 595.99.02: CGL shader output passed
(64,127,191,255 with tolerance for rounding); explicit experimental Metal buffer
fill passed. Both deliberately wrong expectations failed. Native AppKit text/UI
rasterization and shared-memory presentation are still separate CPU paths, so
none of the twenty apps is marked GPU-qualified from these probes.

Provenance: vendored CGL/OpenGL/Metal headers and existing backend source (rung 2),
original probe/shader source and source-built guest API/output observations
(rung 4). No Apple binary inspection, copied shaders or fake device symbols.
