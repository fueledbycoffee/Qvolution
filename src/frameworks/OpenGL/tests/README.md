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

Omit gl_queries.c and add -DCGL_TEST_PUBLIC_GETTERS to the second command
to exercise plain driver forwarding;
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
- smoothing-scope.c compares
  RGB8/RGBA8 texture/renderbuffer targets, default/accelerated context requests,
  three hints, disabled/source-alpha/saturate blending, and a line-smoothing
  control. It also reports enable queries, attribute-stack restoration and
  display-list compile/replay state. It makes no assertion about renderer
  capabilities or expected pixel counts. Counts cover RGB and alpha separately.
  On macOS 12.7.6 versus Darling r7, all 72 polygon and 72 line toggle counts
  differed. On macOS, polygon smoothing changed neither RGB nor alpha in any
  case. Line smoothing changed pixels on both platforms, with different counts.
  Target setup, enable queries, attribute-stack restoration and display-list
  compile/replay state matched. Default/accelerated requests do not establish
  that distinct renderers were selected. The probe records aggregate counts,
  not the per-pixel coverage needed to specify a line antialiasing filter.

Rung 4 observations: fx-render-4/oracle/20261107T084032Z-checks_-587056,
mac/opengl-list-mode.txt, mac/opengl-polygon-smooth.txt, and the corresponding
Darling output and authored PPMs. The scope observation is in
fx-render-4/oracle/20261107T104311Z-checks_-837649,
mac/opengl-smoothing-scope.txt and darling/opengl-smoothing-scope.txt.
These observations do not specify a platform-wide polygon-smoothing no-op or
an exact line rasterization algorithm. These checks neither inspect Apple
images nor invoke tools on framework binaries.

rasterizer-control.c separates the original display-list scene's polygon
smoothing from line filtering. It writes the same white-composited RGB scene
frames as opengl-display-list.m, plus alpha images; its line controls write raw
RGB and alpha separately for all three hints and blend modes. Build with
`clang rasterizer-control.c -framework OpenGL -o rasterizer-control` and set
QV_OUT_DIR to an existing output directory. Renderer strings are diagnostic
metadata (lines prefixed ##), not equality assertions. Allocation/setup/image
failures make the program fail. The existing four checks remain unchanged.

Rung 4: fx-render-4/oracle/20261107T134741Z-controls_-1284108, compared with
20261107T134530Z-checks_-1275760. Both executions exit 0 and control text
matches. All twelve original RGB outputs (six on each platform) are reproduced
byte-for-byte. Disabling polygon smoothing eliminates all six original image
differences. All 36 line-off RGB/alpha controls match; line-on has 30 differing
and six matching images. Scene-off RGB/alpha has twelve matches, scene-on
has twelve differences. The two default/accelerated requests select the same
renderer on each platform.

Rung 3: Apple's OpenGL Programming Guide for Mac, "Techniques for Scene
Antialiasing", delegates point/line antialiasing to the OpenGL specification:
https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_fsaa/opengl_fsaa.html
OpenGL 2.1 sections 3.2 and 5.6 allow approximate coverage, alternate filters,
and implementation-dependent hints; section 3.5.3 describes polygon coverage:
https://registry.khronos.org/OpenGL/specs/gl/glspec21.pdf
The command reference also specifies implementation-dependent hint behavior:
https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/glHint.xml
These contracts do not prescribe one renderer's exact changed-pixel counts,
and ignoring a hint does not mean ignoring the smoothing enable state.
The observed polygon no-effect is a renderer observation, not a public
platform-wide promise. This control supplies no new rendering override.

Review integration: OpenGL.c owns the five public numeric getter exports and
routes non-version requests through the hidden cgl_glGet*v helpers in
gl_queries.c. windowless.c includes both production files: each selected
legacy/core context tests version queries interleaved with idle, compile and
compile-and-execute list-mode requests through all five public getters, including
unchanged output/error behavior for rejected core-profile list queries. The
standalone list-mode suite calls the same helpers and retains thread/context,
vector-forwarding and invalid-query coverage. Both translation units must be
linked into the framework for every architecture.
