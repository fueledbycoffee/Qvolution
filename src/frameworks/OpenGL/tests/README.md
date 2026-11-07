CPU-only regression (from the repository root, with host EGL/GL development headers):

    cc -std=gnu11 -Isrc/frameworks/OpenGL/tests/include \
      -Isrc/frameworks/OpenGL/include src/frameworks/OpenGL/tests/windowless.c \
      -pthread -lGL -o /tmp/cgl-windowless-test
    /tmp/cgl-windowless-test

The test compiles the actual CGL source with a fake EGL driver. It does not create
host contexts or execute GPU operations. It checks lazy initialization, config
closest/minimum/maximum selection, failed initialization/selection/creation, explicit sharing versus
isolation, profile requests, current-context state preservation on failure, the
pbuffer fallback, pixel-format lifetime across later native registration, and
swap-interval queries. The original source fails the first initialization-error
assertion. Build this test under the host's build slot convention.

Run the same executable with `--display-list-context` to exercise the two
pixel-format requests from oracle-render's opengl-display-list.m: RGB 24,
alpha 8, with and without kCGLPFAAccelerated, before any native display is
registered. Both formats must create a context that can be made current and
released. This mode checks only context setup; the differential check still
needs real GL rendering and six PPM image comparisons.

The list-mode CPU regression uses an authored shared-library driver that keeps
GL_COMPILE after glEndList, reproducing the differential failure. It verifies
idle mode through all five query types, active compile and compile-and-execute,
context/thread isolation, unrelated scalar/vector queries, and unchanged output
and errors for invalid queries. No GPU or host GL context is used:

    lane_native=/tmp/cgl-query-test
    mkdir -p "$lane_native"
    inc="$PWD/src/frameworks/OpenGL/include"
    tests="$PWD/src/frameworks/OpenGL/tests"
    cc -std=gnu11 -fPIC -shared -I"$inc" "$tests/query-driver.c" \
      -o "$lane_native/libquery-driver.so"
    cc -std=gnu11 -I"$inc" \
      -DCGL_NATIVE_GL_LIBRARY="\"$lane_native/libquery-driver.so\"" \
      "$tests/list-mode.c" src/frameworks/OpenGL/gl_queries.c \
      -L"$lane_native" -lquery-driver -Wl,-rpath,"$lane_native" \
      -pthread -ldl -o "$lane_native/list-mode"
    "$lane_native/list-mode"

Omit gl_queries.c from the second command to exercise plain driver forwarding;
the first completed-list assertion must fail. Build under the host build slot.

cgl-offscreen.m is an authored public-API differential check for macOS and Darling.
Build with -framework OpenGL (the batch runner adds Foundation for .m). It checks
64x64 texture-backed FBO rendering and readback, replacement of a display list,
shared texture contents, independent groups, and legacy/core current contexts.
The advertised version follows the selected CGL profile: legacy exposes 2.1,
while core contexts expose the version requested successfully from EGL (4.1,
or the 3.2 fallback). Native backend capabilities are still used internally
when initializing framebuffer state.

profile.c checks the string version and all five intercepted numeric getter
types. Legacy GL_MAJOR_VERSION and GL_MINOR_VERSION queries must preserve the
output and raise GL_INVALID_ENUM; core queries must succeed. selection.c checks
closest/default, minimum and maximum size selection, alpha/stencil constraints,
and rejection of a NULL pixel format even when a share context is supplied.
Build both with -framework OpenGL through qv-diff-batch. The selection check
compares success/error contracts instead of hardware-dependent buffer sizes.
The CPU test also offers individual legacy, selection and null-format arguments
for reproducing each review failure independently.

Public query/rasterization probes (each builds with `clang <file> -framework
OpenGL -o <executable>` on macOS; polygon-smooth.c writes PPMs to QV_OUT_DIR):

- list-mode-oracle.c exercises all five scalar query types while idle,
  compiling, redefining, replaying and deleting lists. The same authored binary
  matched on macOS 12.7.6 and Darling r7 with the list-mode fix.
- polygon-smooth.c compares direct and display-list triangle/quad rendering,
  toggling GL_POLYGON_SMOOTH on a single-sample RGBA8 renderbuffer. The r7 run
  retained requested enable state on both platforms. All four smoothing-off
  images matched within tolerance; smoothing changed 0 pixels on macOS and
  140/206 pixels on Darling. This remains an unresolved rasterization
  difference, not evidence that queries should report smoothing disabled.
- smoothing-scope.c is a follow-up probe, not yet observed on macOS. It compares
  RGB8/RGBA8 texture/renderbuffer targets, default/accelerated context requests,
  three hints, disabled/source-alpha/saturate blending, and a line-smoothing
  control. It also reports enable queries, attribute-stack restoration and
  display-list compile/replay state. It makes no assertion about renderer
  capabilities or expected pixel counts. Counts cover RGB and alpha separately.

Rung 4 observations: fx-render-4/oracle/20261107T084032Z-checks_-587056,
mac/opengl-list-mode.txt, mac/opengl-polygon-smooth.txt, and the corresponding
Darling output and authored PPMs. The scope probe needs the same differential
run before a rasterization policy can be inferred. These checks neither inspect
Apple images nor invoke tools on framework binaries.
