# Fixture frame settling and text field partial redraw

Base: Stage41c3, superproject 8aff919e94, cocotron a360e4d95.
The component changes add a shared fixture event pump and conservative
NSTextField opacity. Its current cell drawing leaves translucent or uncovered
edges, so child invalidation must redraw an opaque ancestor first.

The TextEdit-like latency and ruler fixtures wait for stable frame/content size,
backing scale, configure and key state before post-launch measurements. Theme
capture uses the same helper for mapped windows; offscreen bitmaps need no
compositor. The fixture policy observes at least 1 second and requires 0.5
seconds of quiet, failing after 5 seconds. This is a heuristic, not a compositor
completion guarantee.

`DARLING_APPKIT_FRAME_TRACE=1` enables fixed source labels and old/new rectangles
for menu hide/show, NSWindow frame forwarding, Wayland setFrame/configure and
context resize. It defaults off; no caller addresses or backtraces are recorded.

The new field fixture compares 18 partial/full backing-pixel pairs across square,
rounded, line, plain and translucent/absent backgrounds. It captures just the
field and its margin using the ruler fixture's tested device conversion.
`FIXTURE_FORCE_OPAQUE=1` is a fixture-only mutation required to fail actual pixels.

Native verification in `$W/parallel/menu-settle-textfield/evidence`:
- `run-native.sh red-committed .../red-source`: settle compile-red; opacity fails
  on the production baseline method (source snapshot from test commit b1e6dcf5).
- `run-native.sh green-committed`: both pass; frame state uses ASan/UBSan.
- `check-trace.sh`: default/zero silent; enabled trace records the 26px shrink.
- `check-capture.sh`: all 12 geometry checks pass with ASan/UBSan.

`candidate-build/build-report.json` records successful AppKit and Wayland builds;
control relinks reproduce both Stage41c3 images byte for byte. All six guest
fixtures compile with `-Werror=objc-method-access`. Both Ninja graphs are idle.
`overlay/` shares immutable Stage41c3 files and contains private replacement images.

Container behavior is NOT VERIFIED: the worker sandbox cannot boot Darling.
Run `$W/parallel/menu-settle-textfield/run-container.sh` on the GNOME/Wayland host.
It uses a guest slot, fresh initialized prefixes, qv-shutdown and qv-host-guard;
requires baseline failure, candidate equality, opaque-mutant failure, trace
identity/default-off behavior, observed menu hide and existing fixture success.
The text-edit timing output is a functional smoke, not performance evidence.
