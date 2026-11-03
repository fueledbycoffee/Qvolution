CPU-only regression (from the repository root, with host EGL/GL development headers):

    cc -std=gnu11 -Isrc/frameworks/OpenGL/tests/include \
      -Isrc/frameworks/OpenGL/include src/frameworks/OpenGL/tests/windowless.c \
      -pthread -lGL -o /tmp/cgl-windowless-test
    /tmp/cgl-windowless-test

The test compiles the actual CGL source with a fake EGL driver. It does not create
host contexts or execute GPU operations. It checks lazy initialization, config
minimums, failed initialization/selection/creation, explicit sharing versus
isolation, profile requests, current-context state preservation on failure, the
pbuffer fallback, pixel-format lifetime across later native registration, and
swap-interval queries. The original source fails the first initialization-error
assertion. Build this test under the host's build slot convention.

cgl-offscreen.m is an authored public-API differential check for macOS and Darling.
Build with -framework OpenGL (the batch runner adds Foundation for .m). It checks
64x64 texture-backed FBO rendering and readback, replacement of a display list,
shared texture contents, independent groups, and legacy/core current contexts.
It prints only the GL version major; a different supported GL version is an
expected platform-capability difference, not a reason to spoof the version.
