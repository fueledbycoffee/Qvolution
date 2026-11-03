// CPU-only EGL boundary regression: no host display or GPU calls.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#ifndef CGL_TEST_SOURCE
#define CGL_TEST_SOURCE "../OpenGL.c"
#endif
#include CGL_TEST_SOURCE

static int platform_calls, make_calls, create_calls, destroy_surfaces;
static int fail_init, fail_choose, fail_create, fail_current, require_pbuffer, minimum_interval;
static EGLContext last_share, current_egl;
static EGLint last_major, last_depth, last_alpha;
static EGLSurface current_surface;
#define HEADLESS ((EGLDisplay)(uintptr_t)1)
#define WINDOWED ((EGLDisplay)(uintptr_t)2)
#define CONFIG ((EGLConfig)(uintptr_t)3)
#define PBUFFER ((EGLSurface)(uintptr_t)4)

CFMutableDictionaryRef CFDictionaryCreateMutable(void *a, long b, const void *c, const void *d) { return NULL; }
const void *CFDictionaryGetValue(CFDictionaryRef a, const void *b) { return NULL; }
void CFDictionaryAddValue(CFMutableDictionaryRef a, const void *b, const void *c) {}
void *_CGSNativeDisplay(CGSConnectionID a) { return NULL; }
void *_CGSNativeWindowForID(CGSConnectionID a, CGSWindowID b) { return NULL; }
void *_CGSNativeWindowForSurfaceID(CGSConnectionID a, CGSWindowID b, CGSSurfaceID c) { return NULL; }
EGLDisplay eglGetPlatformDisplay(EGLenum platform, void *native, const EGLAttrib *attrs) {
    ++platform_calls;
    return platform == EGL_PLATFORM_SURFACELESS_MESA ? HEADLESS : WINDOWED;
}
EGLBoolean eglInitialize(EGLDisplay d, EGLint *a, EGLint *b) { return !fail_init; }
EGLBoolean eglTerminate(EGLDisplay d) { assert(0); return EGL_FALSE; }
EGLBoolean eglBindAPI(EGLenum api) { assert(api == EGL_OPENGL_API); return EGL_TRUE; }
EGLBoolean eglChooseConfig(EGLDisplay d, const EGLint *attrs, EGLConfig *out, EGLint cap, EGLint *count) {
    assert(d == HEADLESS || d == WINDOWED);
    for (int i = 0; attrs[i] != EGL_NONE; i += 2) {
        if (attrs[i] == EGL_DEPTH_SIZE) last_depth = attrs[i+1];
        if (attrs[i] == EGL_ALPHA_SIZE) last_alpha = attrs[i+1];
    }
    *count = fail_choose ? 0 : 1;
    if (out && !fail_choose) *out = CONFIG;
    return EGL_TRUE;
}
EGLBoolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint attr, EGLint *out) {
    *out = attr == EGL_MIN_SWAP_INTERVAL ? minimum_interval :
        attr == EGL_CONFIG_CAVEAT ? EGL_NONE : attr == EGL_DEPTH_SIZE ? 24 : 8;
    return EGL_TRUE;
}
EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext share, const EGLint *attrs) {
    assert(c == CONFIG); ++create_calls; last_share = share; last_major = 0;
    if (attrs) for (int i = 0; attrs[i] != EGL_NONE; i += 2)
        if (attrs[i] == EGL_CONTEXT_MAJOR_VERSION) last_major = attrs[i+1];
    return fail_create ? EGL_NO_CONTEXT : (EGLContext)(uintptr_t)(100 + create_calls);
}
EGLBoolean eglDestroyContext(EGLDisplay d, EGLContext c) { return EGL_TRUE; }
EGLint eglGetError(void) { return EGL_BAD_MATCH; }
EGLBoolean eglMakeCurrent(EGLDisplay d, EGLSurface a, EGLSurface b, EGLContext c) {
    ++make_calls;
    if (fail_current || (require_pbuffer && c && !a)) return EGL_FALSE;
    current_egl = c; current_surface = a; return EGL_TRUE;
}
EGLSurface eglCreatePbufferSurface(EGLDisplay d, EGLConfig c, const EGLint *a) { return PBUFFER; }
EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *a) { return EGL_NO_SURFACE; }
EGLBoolean eglDestroySurface(EGLDisplay d, EGLSurface s) { ++destroy_surfaces; return EGL_TRUE; }
EGLBoolean eglQuerySurface(EGLDisplay d, EGLSurface s, EGLint a, EGLint *v) { return EGL_FALSE; }
EGLBoolean eglSwapInterval(EGLDisplay d, EGLint i) { return EGL_TRUE; }
EGLBoolean eglSwapBuffers(EGLDisplay d, EGLSurface s) { return EGL_TRUE; }

int main(void) {
    CGLPixelFormatAttribute attrs[] = { kCGLPFAAccelerated, kCGLPFAAllowOfflineRenderers,
        kCGLPFAOpenGLProfile, kCGLOGLPVersion_Legacy, kCGLPFAColorSize, 24,
        kCGLPFADepthSize, 24, kCGLPFAAlphaSize, 8, 0 };
    CGLPixelFormatObj pf = NULL; GLint screens = -1;
    fail_init = 1;
    assert(CGLChoosePixelFormat(attrs, &pf, &screens) == kCGLBadConnection);
    assert(!pf && screens == 0);
    fail_init = 0;
    assert(CGLChoosePixelFormat(attrs, &pf, &screens) == kCGLNoError && pf && screens == 1);
    assert(pf->egl_display == HEADLESS && last_depth == 24 && last_alpha == 8);
    GLint bits = 0;
    assert(CGLDescribePixelFormat(pf, 0, kCGLPFADepthSize, &bits) == kCGLNoError && bits >= 24);
    assert(CGLDescribePixelFormat(pf, 0, kCGLPFAColorSize, &bits) == kCGLNoError && bits >= 24);
    assert(CGLDescribePixelFormat(pf, 0, kCGLPFAAccelerated, &bits) == kCGLNoError && bits == 1);
    int probes = platform_calls;
    CGLPixelFormatObj second = NULL;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLNoError);
    assert(platform_calls == probes);
    CGLReleasePixelFormat(second);
    attrs[5] = 128;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLBadPixelFormat && !second);
    attrs[5] = 24;
    attrs[3] = 0x7fff;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLBadAttribute && !second);
    attrs[3] = kCGLOGLPVersion_Legacy;
    CGLContextObj a = NULL, b = NULL, independent = NULL;
    assert(CGLCreateContext(pf, NULL, &a) == kCGLNoError && last_share == EGL_NO_CONTEXT);
    assert(CGLSetCurrentContext(a) == kCGLNoError && current_egl == a->egl_context);
    assert(CGLGetCurrentContext() == a);
    GLint interval = -1;
    assert(CGLGetParameter(a, kCGLCPSwapInterval, &interval) == kCGLNoError && interval == 1);
    interval = 0;
    assert(CGLSetParameter(a, kCGLCPSwapInterval, &interval) == kCGLNoError);
    assert(CGLGetParameter(a, kCGLCPSwapInterval, &interval) == kCGLNoError && interval == 0);
    assert(CGLGetParameter(NULL, kCGLCPSwapInterval, &interval) == kCGLBadContext);
    assert(CGLGetParameter(a, (CGLContextParameter)-1, &interval) == kCGLBadEnumeration);
    assert(CGLCreateContext(NULL, a, &b) == kCGLNoError && last_share == a->egl_context);
    assert(CGLCreateContext(pf, NULL, &independent) == kCGLNoError && !last_share);
    fail_create = 1; int attempts = create_calls; CGLContextObj failed = a;
    assert(CGLCreateContext(pf, a, &failed) == kCGLBadMatch && !failed);
    assert(create_calls == attempts + 1);
    fail_create = 0;
    fail_current = 1;
    assert(CGLSetCurrentContext(b) != kCGLNoError && CGLGetCurrentContext() == a);
    fail_current = 0; require_pbuffer = 1;
    assert(CGLSetCurrentContext(b) == kCGLNoError && current_surface == PBUFFER);
    assert(CGLSetCurrentContext(NULL) == kCGLNoError && !CGLGetCurrentContext());
    CGLReleaseContext(b); assert(destroy_surfaces >= 1);
    CGLReleaseContext(independent);
    attrs[3] = kCGLOGLPVersion_3_2_Core;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLNoError);
    assert(CGLCreateContext(second, NULL, &b) == kCGLNoError && last_major == 3);
    CGLReleaseContext(b); CGLReleasePixelFormat(second);
    fail_create = 1; attempts = create_calls;
    assert(CGLCreateContext(pf, NULL, &failed) != kCGLNoError && !failed);
    assert(create_calls == attempts + 1); fail_create = 0;
    assert(CGLRegisterNativeDisplayForPlatform((void *)1, EGL_PLATFORM_WAYLAND_KHR) == kCGLNoError);
    minimum_interval = 1;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLBadPixelFormat && !second);
    minimum_interval = 0;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLNoError && second->egl_display == WINDOWED);
    assert(pf->egl_display == HEADLESS);
    assert(CGLCreateContext(second, a, &failed) == kCGLBadMatch && !failed);
    CGLReleasePixelFormat(second);
    fail_choose = 1; second = pf;
    assert(CGLChoosePixelFormat(attrs, &second, &screens) == kCGLBadPixelFormat && !second && !screens);
    CGLReleaseContext(a); CGLReleasePixelFormat(pf);
    puts("windowless CPU regression: PASS");
}
