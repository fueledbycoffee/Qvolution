# Qvolution work queue

- [x] QV-001: Fork VibeDarling, preserve provenance and create an independent local checkout.
- [x] QV-002: Define exactly 20 application targets, workflow gates and reproducible run records.
- [x] QV-003: Establish a clean pinned build, staged runtime and repeated CLI bootstrap.
- [x] QV-004: Fix the reproduced Wayland absolute-socket autodetection bug with a failing-before/passing-after regression.
- [ ] QV-005: Make TextEdit edit/save/reopen reliable under GNOME/Mutter.
- [ ] QV-006: Make Terminal PTY input, resize, multiple windows and clean exit reliable.
- [ ] QV-007: Repair menu mutation, validation, removed-item IDs, toggle state and panel-disappearance fallback.
- [ ] QV-008: Separate D-Bus menu export from X11 registration; implement native Wayland focus/application association with a specific GNOME extension.
- [ ] QV-009: Publish Mutter and Sway integration evidence for focus, input, clipboard and fractional scaling.
- [ ] QV-010: Exercise waves 1 and 2; link shared blockers rather than adding app-specific fake-success stubs.
- [ ] QV-011: Run a minimal unmodified Mac Electron fixture: renderer/helpers, network, input, clipboard and sustained rendering.
- [ ] QV-012: Test VS Code, Obsidian and Firefox with versioned artifacts.
- [ ] QV-013: Implement and test real capture via Linux desktop portals/PipeWire and explicit permission handling.
- [ ] QV-014: Establish the guest sandbox behavior and an outer Linux isolation strategy for browser workloads.
- [ ] QV-015: Obtain an ARM64 test path for Dia; do not count an architecture mismatch as an API regression.
- [ ] QV-016: Test Spotify, WhatsApp, Dia and Discord one workflow at a time with user-driven authentication.

Each runtime change needs a focused regression, its source/provenance and an observed workflow result. A missing-symbol fix is a load milestone, not a usable application. Preserve upstream clean-room boundaries and component licenses. Shared human review should be independent; do not invent reviewers or approvals. No upstream contribution or public issue is implied by local work.
