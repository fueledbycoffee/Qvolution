# Graphics import regression packet

Configure normally with `ENABLE_TESTS=ON` and the three required Swift inputs in
Cocotron's `CoreGraphics/SWIFT-BUILD.md`. No external CMake injection is needed.
Build these targets (on the fleet host, hold `dashboard/qv-slot build`):

```
ninja -C <build> -j6 CoreGraphics CoreText QuartzCore ImageIO CoreDisplay \
  QuickLookThumbnailing imp_graphics_load_test imp_graphics_behavior_test \
  imp_graphics_layer_test imp_graphics_swift_test
ninja -C <build> -n <same targets>
python3 tests/graphics-imports/audit-exports.py <build> <evidence-directory>
```

The static audit reads only these Darling build products, checks the framework's
Swift reexport priority and every demanded non-weak export, and emits `guest-load.tsv`.
It does not establish runtime loadability. Under Darling, with matching runtime
and supplement installed, run all four clients and require exit zero:

- `tests/graphics-imports/imp_graphics_load_test <guest-load.tsv>`: real dlopen
  and dlsym through the parent frameworks, including the Swift reexport.
  Add `/usr/lib/swift/libswiftCoreGraphicsImports.dylib` as a second argument
  to require every supplement Swift entry to resolve to that provider.
- `tests/graphics-imports/imp_graphics_behavior_test`: typed fallback and object
  behavior across CoreGraphics, CoreText, QuartzCore, ImageIO and thumbnails.
- `src/external/cocotron/CoreGraphics/imp_graphics_layer_test`: PDF clip state,
  successful nested bitmap compositing and parent drawing outside layer bounds.
- `src/external/cocotron/CoreGraphics/imp_graphics_swift_test`: a separate Swift
  client of the generated CoreGraphics module. Calls both fill-rule enum cases
  and checks pixels, transformed path wrappers, CGRect equality and Set hashing.

The last two source fixtures live in Cocotron's `tests/graphics-imports/` and
travel with that component. Its `layer-state-native.py` also exercises the
production lifecycle methods with C++ API doubles on the host (not rendering).
Its `configure-swift.py` verifies that a missing toolchain cannot be skipped.

For the layer negative control, link the unchanged `layer-clip.m` against the
reviewed baseline framework/runtime. Require a failing PDF parent-clip check,
then repeat with the candidate; never claim the native doubles are a PDF run.
The baseline had no Swift test target or installed compiler module: building
`imp_graphics_swift_test` there must fail with unknown target. Candidate coverage
requires a successful build and execution, not merely symbol resolution.

The fleet's prepared lane runner delegates to the source-controlled
`run-review.sh` with `IMP_GRAPHICS_LANE` set. It owns sequential build/guest
slots, initializes fresh prefixes before copying fixtures, snapshots/verifies
host folders, and uses `dashboard/qv-shutdown` at every shutdown. A failed
shutdown or host-folder verification fails the run. Exact fleet rerun:

```
bash parallel/imp-graphics/run-fix-review.sh
```
