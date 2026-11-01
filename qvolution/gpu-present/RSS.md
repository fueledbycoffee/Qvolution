# EGL versus shm resident memory

The runner's 2026-11-01 1000-frame receipts are in
`parallel/gpu-present/runs/codex-soak-1101123742-3ab647/` (outside git).
At frame 100/end, shm used 146436/147168 KiB and EGL 205820/206868 KiB.
The end difference is 59700 KiB (58.3 MiB); growth during frames 100–1000
was 732 KiB for shm and 1048 KiB for EGL. FD counts fell 30→29 and 48→47.
This run shows a mostly fixed EGL cost, not continued surface-sized growth.
It does not rule out smaller leaks or establish the cost on another driver.

Our source accounts for substantial additional storage at 1932×1144:

- `QuartzCore/CAWindowOpenGLContext.m`, `stagingBufferForSize:` allocates three
  persistent, coherent, CPU-mapped upload PBOs. At the minimum BGRA stride each
  is 8840832 bytes; the ring is 26522496 bytes (25.3 MiB).
- `presentSurface:...` keeps one RGBA8 texture: another 8840832 bytes
  (8.43 MiB). Texture storage is reused until the surface changes size.
- `AppKit/Wayland.backend/WaylandWindow.m`, `ensureEGLPresenterWidth:height:`
  also creates a CGL context and EGL window surface. Driver context state,
  drawable buffers, mapped allocations and loaded driver libraries add costs
  that the fixture's whole-process VmRSS includes. Two to three RGBA-sized
  drawable buffers would be another 16.9–25.3 MiB, but that count and their RSS
  attribution are hypotheses, not measurements from these receipts.

The three upload buffers plus texture alone request 33.7 MiB; with drawable
and driver costs this is consistent with the observed 58.3 MiB difference.
GL allocation sizes are not a byte-for-byte accounting of Linux VmRSS.
Verification readback is not the cause here: the EGL soak reports checks=0.
No driver internals were inspected. This explanation is based on our source
and the benchmark API results only (rung 4); exact allocation attribution
remains unverified. The unchanged soak gate still rejects >8192 KiB growth
from frame 100 to 1000. Reducing the ring would change synchronization and
throughput and requires a separate GPU measurement before choosing that tradeoff.
