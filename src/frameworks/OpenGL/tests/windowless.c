// CPU-only EGL boundary regression: no host display or GPU calls.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#ifndef CGL_TEST_SOURCE
#define CGL_TEST_SOURCE "../OpenGL.c"
#endif
#define dlopen mock_dlopen
#define dlsym mock_dlsym
#include CGL_TEST_SOURCE
#undef dlopen
#undef dlsym

static int platform_calls, make_calls, create_calls, destroy_surfaces;
static int fail_init, fail_choose, fail_create, fail_current, require_pbuffer, minimum_interval;
static EGLContext last_share, current_egl;
static EGLint last_major, last_minor, last_profile, last_depth, last_alpha;
static int reject_gl4;
static EGLint create_error = EGL_BAD_MATCH;
static EGLSurface current_surface;
#define HEADLESS ((EGLDisplay)(uintptr_t)1)
#define WINDOWED ((EGLDisplay)(uintptr_t)2)
#define CONFIG ((EGLConfig)(uintptr_t)3)
#define PBUFFER ((EGLSurface)(uintptr_t)4)

static const GLubyte *backend_string(GLenum name) {
    return (const GLubyte *)(name == GL_VERSION ? "4.6 Mock" : "Mock vendor");
}
#define BACKEND_GETTER(name, type) \
    static void backend_##name(GLenum name, type *value) { *value = 77; }
BACKEND_GETTER(glGetIntegerv, GLint)
BACKEND_GETTER(glGetInteger64v, GLint64)
BACKEND_GETTER(glGetFloatv, GLfloat)
BACKEND_GETTER(glGetDoublev, GLdouble)
BACKEND_GETTER(glGetBooleanv, GLboolean)
#undef BACKEND_GETTER
void *mock_dlopen(const char *path, int flags) {
    assert(!strcmp(path, "/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib"));
    return (void *)1;
}
void *mock_dlsym(void *handle, const char *name) {
    assert(handle == (void *)1);
    if (!strcmp(name, "glGetString")) return backend_string;
#define SYMBOL(function) if (!strcmp(name, #function)) return backend_##function;
    SYMBOL(glGetIntegerv)
    SYMBOL(glGetInteger64v)
    SYMBOL(glGetFloatv)
    SYMBOL(glGetDoublev)
    SYMBOL(glGetBooleanv)
#undef SYMBOL
    assert(0); return NULL;
}

static void version(GLint major, GLint minor) {
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "%d.%d ", major, minor);
    const char *reported = (const char *)glGetString(GL_VERSION);
    assert(reported && !strncmp(reported, prefix, strlen(prefix)));
    GLint i = -1; GLint64 i64 = -1; GLfloat f = -1; GLdouble d = -1; GLboolean b = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &i); assert(i == major);
    glGetIntegerv(GL_MINOR_VERSION, &i); assert(i == minor);
    glGetInteger64v(GL_MAJOR_VERSION, &i64); assert(i64 == major);
    glGetFloatv(GL_MINOR_VERSION, &f); assert(f == minor);
    glGetDoublev(GL_MAJOR_VERSION, &d); assert(d == major);
    glGetBooleanv(GL_MAJOR_VERSION, &b); assert(b == GL_TRUE);
    assert(!strcmp((const char *)glGetString(GL_VENDOR), "Mock vendor"));
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &i); assert(i == 77);
    glGetInteger64v(GL_MAX_TEXTURE_SIZE, &i64); assert(i64 == 77);
    glGetFloatv(GL_MAX_TEXTURE_SIZE, &f); assert(f == 77);
    glGetDoublev(GL_MAX_TEXTURE_SIZE, &d); assert(d == 77);
    glGetBooleanv(GL_DEPTH_TEST, &b); assert(b == 77);
}

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
    last_minor = last_profile = 0;
    if (attrs) for (int i = 0; attrs[i] != EGL_NONE; i += 2) {
        if (attrs[i] == EGL_CONTEXT_MAJOR_VERSION) last_major = attrs[i+1];
        if (attrs[i] == EGL_CONTEXT_MINOR_VERSION) last_minor = attrs[i+1];
        if (attrs[i] == EGL_CONTEXT_OPENGL_PROFILE_MASK) last_profile = attrs[i+1];
    }
    return (fail_create || (reject_gl4 && last_major == 4)) ? EGL_NO_CONTEXT : (EGLContext)(uintptr_t)(100 + create_calls);
}
EGLBoolean eglDestroyContext(EGLDisplay d, EGLContext c) { return EGL_TRUE; }
EGLint eglGetError(void) { return create_error; }
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
    version(2, 1);
    assert(last_major == 0 && last_minor == 0 && last_profile == 0);
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
    assert(!strcmp((const char *)glGetString(GL_VERSION), "4.6 Mock"));
    glGetIntegerv(GL_MAJOR_VERSION, &bits); assert(bits == 77);
    CGLReleaseContext(b); assert(destroy_surfaces >= 1);
    CGLReleaseContext(independent);
    attrs[3] = kCGLOGLPVersion_3_2_Core;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLNoError);
    assert(CGLCreateContext(second, NULL, &b) == kCGLNoError);
    assert(last_major == 4 && last_minor == 1 && last_profile == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT);
    assert(CGLSetCurrentContext(b) == kCGLNoError); version(4, 1);
    assert(CGLSetCurrentContext(a) == kCGLNoError); version(2, 1);
    CGLReleaseContext(b);
    fail_create = 1; create_error = EGL_BAD_ALLOC; attempts = create_calls;
    assert(CGLCreateContext(second, NULL, &b) == kCGLBadContext && !b && create_calls == attempts + 1);
    fail_create = 0; create_error = EGL_BAD_MATCH;
    reject_gl4 = 1; attempts = create_calls;
    assert(CGLCreateContext(second, NULL, &b) == kCGLNoError && create_calls == attempts + 2);
    assert(last_major == 3 && last_minor == 2 && last_profile == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT);
    assert(CGLSetCurrentContext(b) == kCGLNoError); version(3, 2);
    CGLContextObj core_share = NULL;
    assert(CGLCreateContext(NULL, b, &core_share) == kCGLNoError && last_share == b->egl_context);
    assert(CGLSetCurrentContext(core_share) == kCGLNoError); version(3, 2);
    CGLReleaseContext(core_share); CGLReleaseContext(b); CGLReleasePixelFormat(second);
    attrs[3] = kCGLOGLPVersion_GL4_Core;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLNoError);
    attempts = create_calls;
    assert(CGLCreateContext(second, NULL, &b) == kCGLBadContext && !b && create_calls == attempts + 1);
    reject_gl4 = 0;
    assert(CGLCreateContext(second, NULL, &b) == kCGLNoError);
    assert(CGLSetCurrentContext(b) == kCGLNoError); version(4, 1);
    CGLReleaseContext(b); CGLReleasePixelFormat(second);
    CGLPixelFormatAttribute defaults[] = { 0 };
    assert(CGLChoosePixelFormat(defaults, &second, NULL) == kCGLNoError);
    assert(CGLCreateContext(second, NULL, &b) == kCGLNoError && last_major == 0 && last_minor == 0);
    assert(CGLSetCurrentContext(b) == kCGLNoError); version(2, 1);
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
