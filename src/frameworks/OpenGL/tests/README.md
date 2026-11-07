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
