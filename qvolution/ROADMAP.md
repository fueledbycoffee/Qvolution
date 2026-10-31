# Qvolution delivery roadmap

The target is all 20 pinned Mac applications completing their real workflows on GNOME with polished UI/UX, verified hardware acceleration and stability. A visible window is only a launch milestone. Source fixtures and official vendor bundles keep separate results. Dates are not promises: dependencies and measured gates determine promotion.

## Acceptance gates for every application

| Gate | Evidence required before marking it passed |
| --- | --- |
| Artifact and architecture | Legitimately acquired, unmodified official bundle with version/hash/architecture and pinned runtime; source fixtures are labelled separately. Dia requires a working ARM64 execution path. |
| Functional workflow | The app-specific playbook completes, including persistence and failure handling. Login alone is insufficient; authenticated actions use the user's chosen test accounts/destinations. |
| UI/UX | Complete functional controls, native Wayland focus/window behavior, keyboard/clipboard/accessibility, correct scale/layout, coherent light/dark styling, and top-bar menus tied to the focused app with fallback on shell restart. Missing controls are functional defects, not cosmetic debt. |
| GPU | Record device/driver/backend, rendered-output correctness and workload timing. Distinguish CPU rasterization, GPU presentation, GPU composition and GPU rendering. A GPU-driven GNOME compositor or successful context creation alone does not establish app acceleration. Reject or explicitly label software fallbacks. |
| Stability | First qualification: 10 clean launch/exit cycles, a one-hour repeated workflow, bounded memory growth and no corruption. Before calling a daily-use/service app stable, run an eight-hour soak and relevant suspend/resume/device-loss cases. These are planned gates, not results already achieved. |

Measure startup, input latency, frame-time percentiles, memory and CPU/GPU usage on this host. Establish an app-specific performance budget before qualification; interactive 60 Hz rendering has a 16.7 ms frame budget, but unsupported workloads must be reported rather than hidden behind a pass.

## Parallel execution now

| Lane | Owner | Current deliverable | Exit gate / dependencies |
| --- | --- | --- | --- |
| Rich-text controls | Inspector subagent | Font/underline prerequisite fixes, a real rich-text inspector and show/hide/layout tests | Failing-before fixtures, guest checks, then visual TextEdit rich/plain and formatting validation |
| GNOME global menus | Menu subagent | Orbit Global Menu bridge, real menu export/focus association and exporter acknowledgement/fallback | Isolated D-Bus/model tests, real guest PID association, staged integration and shell restart behavior; no hiding local menus before acknowledged presentation |
| Terminal/runtime PTY | Terminal subagent | Real PTY input/Unicode/resize/control-terminal/exit harness and vendor-artifact gate | Guest PTY correctness before a Terminal.app claim; no Terminal GUI source fixture exists in the pinned checkout |
| GPU and integration | Main agent | Host/guest renderer baseline, prioritized GPU gaps, roadmap, review and runtime integration | Verify hardware use and rendered output; coordinate shared heavy builds, staging and prefix launches |

Subagents work in isolated checkouts and can compile lightweight isolated outputs. The main agent reviews and integrates focused commits. Shared CMake configuration, heavy builds and staged-runtime replacement have one coordinator; independent source work, model tests and audits run in parallel. Every handoff states what ran, what failed and what still needs verification.

## Dependency waves

1. **Desktop substrate:** finish TextEdit rich editing, undo/clipboard, GNOME menu consumer/registration, PTYs and measured OpenGL/Metal/2D presentation baselines concurrently. Native AppKit UI currently uses CPU rasterization and shared-memory presentation; acceleration is not yet qualified.
2. **Basic desktop apps:** qualify TextEdit, Terminal, Stickies, Calculator and Preview using the same shared primitives. UI and GPU work continues alongside app testing, rather than waiting for all apps to launch.
3. **Graphics/media and utility breadth:** Chess, CotEditor, Transmission, Keka, Cyberduck, VLC, IINA and KeePassXC. Add targeted framework/codec/network fixtures and output correctness tests; fix shared blockers instead of app-specific fake-success stubs.
4. **Multiprocess editors/browsers:** qualify an unmodified Mac Electron fixture and process/sandbox/network/GPU behavior, then VS Code, Obsidian and Firefox. Matching x86 Swift overlays are an independent dependency for apps that require them.
5. **Authenticated services and ARM64:** Spotify, WhatsApp, Dia and Discord. Deliver real media/capture/portal behavior and permissions, validate ARM64 execution for Dia, then complete each service's authorized real workflow. Discord voice, camera and screen share are separate required gates.
6. **Release qualification:** apply the same UI/UX, GPU, persistence and stability gates to every pinned app/version; publish reproducible results, regressions and honest remaining failures. No app is promoted merely because another app from its family works.

## Current evidence and limits

Source TextEdit starts without loader overrides. ASCII saving/reopening, single title bar and removal of menu/scroll flicker are user-confirmed; normal-startup typing and menus are confirmed. The rich-text inspector is implemented: the user confirmed B/I/U and left/center/right alignment. Focus-driven font/size reset regressions pass and the user screenshot shows the requested family/size applied. Synthetic popup tests pass, but interactive acceptance failed: the menu still overflows on the real multi-output setup, scrolling is slow, and the user reports pixelated text/font-change lag. These remain open defects. Fifteen host regression steps and three CLI cycles pass. This does not establish Unicode TextEdit editing, undo, clipboard, complete inspector behavior, GNOME top-bar integration, GPU acceleration, vendor TextEdit compatibility or long-term stability. See [parallel desktop evidence](verification/parallel-desktop.json), including the unexplained timeouts and passing repeat controls.

Orbit Global Menu is the user-selected integration target and is installed as `global-menu@unmade.space` (1.1.3), with its companion Python daemon. The versioned native-Wayland bridge has been deployed with backups and an installation receipt. It associates authenticated D-Bus senders with host process IDs and exports the existing canonical dbusmenu protocol. Private-bus registration, restart and local fallback checks pass; real focused-window PID association and top-bar rendering need verification after the user logs out/in. Local menus remain visible; presentation acknowledgement and duplicate-menu hiding are not implemented. We will not install a competing consumer or change extension settings. See [Orbit source](https://github.com/Unmade760/orbit-global-menu). Guest CGL/OpenGL has rendered a shader triangle with correct hardware pixel readback; guest Metal has completed a GPU buffer-fill/readback with its existing explicit experimental opt-in. These primitive probes do not qualify any of the 20 apps as GPU-accelerated, and native AppKit drawing remains a separate CPU-path acceleration task.

Eight official unmodified vendor inputs have bounded launches through the stage 19 evidence checkpoint: KeePassXC, VLC, Transmission, Firefox, VS Code, CotEditor, Keka and Discord. Versions and actual per-app launch stages are pinned in [the app matrix](APP-MATRIX.md); universal inputs remain unthinned with x86_64 execution. Seven inputs have publisher SHA256 checks; Discord 0.0.420 has official HTTPS/CDN MD5 transport verification and local SHA256 pinning only, with no publisher SHA256, signature or notarization claim. Discord and VS Code stage 17 clear attachment type-hint data and stop at NSScrubberImageItemView; Firefox stage 17 clears popover role data and stops at NSAccessibilityCustomAction. KeePassXC repeats its unexplained startup exit at stage 19; CotEditor retains its stage 11 gate, VLC stage 6 and Transmission stage 4. Keka stage 16 clears the distributed-notification exception and stage 19 repeats a diagnostic CPU settings client image with generic symbol boxes and a duplicate title; compositor visibility and archive workflows remain unverified. URL-handler registry gates pass independently without qualifying URL delivery or app UI. All recorded exact-prefix shutdowns succeed without establishing clean app exit. Every vendor workflow, UI/UX, GPU and stability gate remains open despite that limited client rendering evidence; a timeout establishes neither hang nor success.

Stage6 own-source PDF regressions pass cross-reference/object streams, PNG/TIFF predictors, actual page pixels and malformed-input NULL handling. Metal stage-input descriptors pass metadata/reset/deep-copy/bounds and actual Indium optional conversion without a GPU device. These are shared prerequisite results, not Preview, VLC or KeePassXC qualification; compute stage-in GPU execution remains explicitly unsupported. An opt-in CPU client-raster PNG diagnostic passes an original synthetic roundtrip and independent host pixel checks. It defaults off, limits captures per window, records no titles/content in logs and excludes GPU children/compositor output. Stage7 real-backend capture and ordinary AppKit EGL-presentation checks expose an unresolved progression failure; details below. Images and raw vendor logs stay private.

## Tracked work

- [x] QV-001: Fork VibeDarling, preserve provenance and create an independent local checkout.
- [x] QV-002: Define exactly 20 application targets, workflow gates and reproducible run records.
- [x] QV-003: Establish a clean pinned build, staged runtime and repeated CLI bootstrap.
- [x] QV-004: Fix the reproduced Wayland absolute-socket autodetection bug with a failing-before/passing-after regression.
- [ ] QV-005: Make TextEdit edit/save/reopen reliable under GNOME/Mutter. ASCII save/quit/reopen and flicker correction are user-confirmed; normal Objective-C startup and typing/menus are confirmed. Unicode editing, undo, clipboard and extended stability remain pending.
- [ ] QV-006: Make Terminal PTY input, resize, multiple windows and clean exit reliable.
- [ ] QV-007: Repair menu mutation, validation, removed-item IDs, toggle state and panel-disappearance fallback.
- [ ] QV-008: Separate D-Bus menu export from X11 registration; bridge native Wayland focus/application association into Orbit Global Menu and verify its real menus after session activation.
- [ ] QV-009: Publish Mutter and Sway integration evidence for focus, input, clipboard and fractional scaling.
- [ ] QV-010: Exercise waves 1 and 2; link shared blockers rather than adding app-specific fake-success stubs.
- [ ] QV-011: Run a minimal unmodified Mac Electron fixture: renderer/helpers, network, input, clipboard and sustained rendering.
- [ ] QV-012: Test VS Code, Obsidian and Firefox with versioned artifacts.
- [ ] QV-013: Implement and test real capture via Linux desktop portals/PipeWire and explicit permission handling.
- [ ] QV-014: Establish the guest sandbox behavior and an outer Linux isolation strategy for browser workloads.
- [ ] QV-015: Obtain an ARM64 test path for Dia; do not count an architecture mismatch as an API regression.
- [ ] QV-016: Test Spotify, WhatsApp, Dia and Discord one workflow at a time with user-driven authentication.

Each runtime change needs a focused regression, its source/provenance and an observed workflow result. A missing-symbol fix is a load milestone, not a usable application. Preserve upstream clean-room boundaries and component licenses. Shared human review should be independent; do not invent reviewers or approvals. No upstream contribution or public issue is implied by local work.

- [ ] QV-017: Build matching x86 Swift overlays/runtime and validate Swift ABI behavior. Normal Objective-C TextEdit startup no longer requires diagnostic overrides; Swift-based apps remain unsupported on this legacy-overlay path.
- [x] QV-018: Validate single title-bar ownership on Wayland, preserve X11 layout, and confirm the corrected window with the user.

- [ ] QV-019: Implement NSTextView rich-text inspector controls. Implemented with rich/plain visibility, nested-page layout and formatting fixtures. User confirmed B/I/U and alignment; font/size changes appear in the user screenshot. Synthetic popup checks pass but real multi-output placement, slow scrolling, font quality and change latency still fail acceptance. This remains a functional gate, separate from theme polish.

- [ ] QV-020: Establish guest OpenGL/EGL and Metal/Vulkan device/output correctness baselines, including explicit software-fallback detection.
- [ ] QV-021: Accelerate native AppKit/Onyx2D composition and presentation; measure copies, frame pacing, glyph/image composition and multi-window scaling.
- [ ] QV-022: Define and implement a coherent light/dark GNOME-oriented visual system, accessibility and interaction states after functional controls exist.
- [ ] QV-023: Add per-app repeat-launch, workflow-loop and soak qualification with memory/performance records and restart/device-loss behavior.
- [ ] QV-024: Validate media hardware decoding, texture/surface sharing, audio and portal-backed capture for media/browser/service targets.

- [x] QV-025: Reject overlong prefix socket paths before starting services. Isolated controls reproduced silent Unix-socket truncation for long paths; short unique cold PTY and echo controls pass. The integrated launcher boundary tests pass. Preserve the one unexplained initial timeout separately and do not infer a general cold-start defect from invalid path controls.

## Follow-up candidate: popup bounds and font metrics

The [follow-up evidence](verification/popup-font-followup.json) records a popup-only compositor resize path, configured viewport shrink/grow, live row hit-testing and batched FreeType advances. All seven targeted candidate guest checks pass; real GNOME popup placement/input acceptance remains pending after the prior failed visual check. Rectangle lookup work falls from 400,000 to 8,000 for 1,000 last-row hits, with live item enumeration retained. Guest CJK advance-cache fill measured about 0.9 ms; native old-outline versus bulk-metric timings are microbenchmarks, not established UI latency improvements. Fifteen host check steps complete with socket permission for the selection test; three CLI boot/shutdown cycles pass. The no-output timeout remains recorded.

- [ ] QV-026: Honor explicitly enabled fractional glyph positioning with bounded phase-aware caching and verified flag/bitmap behavior. The original source diagnostic loses requested subpixel movement at 1x/2x while integer controls and an independent FreeType reference pass. Current defaults leave the flags off; this does not establish the cause of the user's pixelated TextEdit text.

The first QV-021 GPU prototype should compose explicit CPU-rasterized layers through existing CALayer/CARenderer and native EGL-child hooks, compare actual pixels with an independent alpha/stride/orientation oracle, fail deliberate upload/UV corruption, and verify compositor-visible presentation. Only measured correctness and performance should justify an opt-in ordinary-window EGL presenter. A root texture upload still retains CPU glyph/path rasterization; globally enabling wantsLayer does not capture ordinary AppKit controls in the current implementation.

The stage7 candidate passes original screen color-space, appearance inheritance/lifetime/drawing and PNG alpha contracts. Ordinary-window EGL positive progression and sustained capture presentation checks fail after a few frames in both shm/EGL; the deliberate one-pixel negative control is detected. Keep GPU presentation experimental and investigate submission versus callback progress before promotion. These checks do not establish app workflow, physical ICC, compositor-visible rendering or GPU rasterization.

A controlled headless GNOME retake now passes the unchanged ordinary-window EGL positive sequence and all capture configuration modes, with genuine corruption rejection. Native GTK callback/presentation feedback progresses there too. This establishes isolated hardware GPU presentation and environment-dependent earlier stalling; it does not prove physical-session visibility/cause, GPU rasterization, app workflows or stability. The reproducible scene is documented in `testing/README.md`.
