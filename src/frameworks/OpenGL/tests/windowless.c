// CPU-only EGL boundary regression: no host display or GPU calls.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#ifndef CGL_TEST_SOURCE
#define CGL_TEST_SOURCE "../OpenGL.c"
#endif
#define dlopen mock_dlopen
#define dlsym mock_dlsym
#define dlclose mock_dlclose
#include CGL_TEST_SOURCE
#ifndef CGL_TEST_QUERIES
#define CGL_TEST_QUERIES "../gl_queries.c"
#endif
#include CGL_TEST_QUERIES
#undef dlopen
#undef dlsym
#undef dlclose

static int platform_calls, make_calls, create_calls, destroy_surfaces;
static int fail_init, fail_choose, fail_create, fail_current, require_pbuffer, minimum_interval;
static EGLContext last_share, current_egl;
static EGLint last_major, last_minor, last_profile, last_depth, last_alpha;
static int reject_gl4, multi_configs;
static GLenum backend_error;
static int backend_begin;
static GLint backend_index, backend_mode = GL_COMPILE;
#define CONFIG_LARGE ((EGLConfig)(uintptr_t)5)
static EGLint create_error = EGL_BAD_MATCH;
static EGLSurface current_surface;
#define HEADLESS ((EGLDisplay)(uintptr_t)1)
#define WINDOWED ((EGLDisplay)(uintptr_t)2)
#define CONFIG ((EGLConfig)(uintptr_t)3)
#define PBUFFER ((EGLSurface)(uintptr_t)4)

static const GLubyte *backend_string(GLenum name) {
    if (backend_begin) {
        backend_error = GL_INVALID_OPERATION;
        return NULL;
    }
    if (name == GL_VERSION) return (const GLubyte *)"4.6 Mock";
    if (name == GL_VENDOR) return (const GLubyte *)"Mock vendor";
    backend_error = GL_INVALID_ENUM;
    return NULL;
}
#define BACKEND_GETTER(name, type) \
    static void backend_##name(GLenum name, type *value) { \
        if (name == GL_LIST_INDEX || name == GL_LIST_MODE) { \
            CGLContextObj context = CGLGetCurrentContext(); \
            if (context && context->version_major >= 3) { \
                backend_error = GL_INVALID_ENUM; \
                return; \
            } \
            GLint result = name == GL_LIST_INDEX ? backend_index : backend_mode; \
            *value = sizeof(type) == sizeof(GLboolean) ? result != 0 : result; \
        } else *value = 77; \
    }
BACKEND_GETTER(glGetIntegerv, GLint)
BACKEND_GETTER(glGetInteger64v, GLint64)
BACKEND_GETTER(glGetFloatv, GLfloat)
BACKEND_GETTER(glGetDoublev, GLdouble)
BACKEND_GETTER(glGetBooleanv, GLboolean)
#undef BACKEND_GETTER
int mock_dlclose(void *handle) { assert(handle == (void *)1); return 0; }
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

GLenum glGetError(void) {
    GLenum error = backend_error;
    backend_error = GL_NO_ERROR;
    return error;
}

static void legacy_errors(void) {
    const GLenum names[] = { GL_MAJOR_VERSION, GL_MINOR_VERSION };
    for (unsigned q = 0; q < 2; ++q) {
#define CHECK(function, type, initial) \
        do { \
            type value = initial; \
            function(names[q], &value); \
            assert(value == initial); \
            assert(glGetError() == GL_INVALID_ENUM); \
            assert(glGetError() == GL_NO_ERROR); \
        } while (0)
        CHECK(glGetIntegerv, GLint, -7);
        CHECK(glGetInteger64v, GLint64, -7);
        CHECK(glGetFloatv, GLfloat, -7);
        CHECK(glGetDoublev, GLdouble, -7);
        CHECK(glGetBooleanv, GLboolean, 17);
#undef CHECK
    }
}

static void list_queries(GLint expected, int invalid) {
#define CHECK(function, type, initial, convert) \
    do { \
        type value = initial; \
        function(GL_LIST_MODE, &value); \
        assert(value == (invalid ? initial : (convert))); \
        assert(glGetError() == (invalid ? GL_INVALID_ENUM : GL_NO_ERROR)); \
    } while (0)
    CHECK(glGetIntegerv, GLint, -7, expected);
    CHECK(glGetInteger64v, GLint64, -7, expected);
    CHECK(glGetFloatv, GLfloat, -7, expected);
    CHECK(glGetDoublev, GLdouble, -7, expected);
    CHECK(glGetBooleanv, GLboolean, 17, expected != 0);
#undef CHECK
}

static void version(GLint major, GLint minor) {
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "%d.%d ", major, minor);
    const char *reported = (const char *)glGetString(GL_VERSION);
    assert(reported && !strncmp(reported, prefix, strlen(prefix)));
    GLint i = -1; GLint64 i64 = -1; GLfloat f = -1; GLdouble d = -1; GLboolean b = 0;
    if (major == 2) {
        backend_begin = 1;
        assert(glGetString(GL_VERSION) == NULL);
        backend_begin = 0;
        assert(glGetError() == GL_INVALID_OPERATION);
        assert(glGetError() == GL_NO_ERROR);
        assert(glGetString(GL_VERSION) == reported);
        backend_error = GL_INVALID_ENUM;
        assert(glGetString(GL_VERSION) == reported);
        assert(glGetError() == GL_INVALID_ENUM);
        legacy_errors();
    } else {
    glGetIntegerv(GL_MAJOR_VERSION, &i); assert(i == major);
    glGetIntegerv(GL_MINOR_VERSION, &i); assert(i == minor);
    glGetInteger64v(GL_MAJOR_VERSION, &i64); assert(i64 == major);
    glGetFloatv(GL_MINOR_VERSION, &f); assert(f == minor);
    glGetDoublev(GL_MAJOR_VERSION, &d); assert(d == major);
    glGetBooleanv(GL_MAJOR_VERSION, &b); assert(b == GL_TRUE);
    assert(glGetError() == GL_NO_ERROR);
    }
    if (major == 2) {
        backend_index = 0;
        list_queries(0, 0);
        backend_index = 42;
        backend_mode = GL_COMPILE;
        list_queries(GL_COMPILE, 0);
        backend_mode = GL_COMPILE_AND_EXECUTE;
        list_queries(GL_COMPILE_AND_EXECUTE, 0);
        backend_index = 0;
        list_queries(0, 0);
        legacy_errors();
    } else {
        list_queries(0, 1);
        glGetIntegerv(GL_MAJOR_VERSION, &i); assert(i == major);
    }
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
    *count = 0;
    for (int c = 0; c < (multi_configs ? 2 : 1) && !fail_choose; ++c) {
        EGLConfig candidate = c == 0 ? CONFIG : CONFIG_LARGE;
        int matches = 1;
        for (int i = 0; attrs[i] != EGL_NONE; i += 2) {
            if (attrs[i] == EGL_DEPTH_SIZE) last_depth = attrs[i+1];
            if (attrs[i] == EGL_ALPHA_SIZE) last_alpha = attrs[i+1];
            if (attrs[i] == EGL_DEPTH_SIZE || attrs[i] == EGL_ALPHA_SIZE || attrs[i] == EGL_STENCIL_SIZE) {
                EGLint bits;
                eglGetConfigAttrib(d, candidate, attrs[i], &bits);
                if (bits < attrs[i+1]) matches = 0;
            }
        }
        if (matches) {
            if (out && *count < cap) out[*count] = candidate;
            ++*count;
        }
    }
    return EGL_TRUE;
}
EGLBoolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint attr, EGLint *out) {
    *out = attr == EGL_MIN_SWAP_INTERVAL ? minimum_interval :
        attr == EGL_CONFIG_CAVEAT ? EGL_NONE :
        attr == EGL_BUFFER_SIZE ? (c == CONFIG_LARGE ? 64 : 32) :
        attr == EGL_DEPTH_SIZE ? (c == CONFIG_LARGE ? 32 : 24) : (c == CONFIG_LARGE ? 16 : 8);
    return EGL_TRUE;
}
EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext share, const EGLint *attrs) {
    assert(c == CONFIG || c == CONFIG_LARGE); ++create_calls; last_share = share; last_major = 0;
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

static void selection_policy(void) {
    multi_configs = 1;
    const CGLPixelFormatAttribute sizes[] = { kCGLPFAColorSize, kCGLPFADepthSize, kCGLPFAAlphaSize };
    for (unsigned i = 0; i < 3; ++i) {
        CGLPixelFormatAttribute attrs[] = { sizes[i], i == 2 ? 32 : 128, 0, 0 };
        CGLPixelFormatObj pf = NULL;
        assert(CGLChoosePixelFormat(attrs, &pf, NULL) == kCGLNoError && pf);
        assert(pf->egl_config == CONFIG_LARGE);
        CGLReleasePixelFormat(pf);
        attrs[2] = kCGLPFAMinimumPolicy;
        assert(CGLChoosePixelFormat(attrs, &pf, NULL) == kCGLBadPixelFormat && !pf);
        attrs[1] = 1; attrs[2] = kCGLPFAMaximumPolicy;
        assert(CGLChoosePixelFormat(attrs, &pf, NULL) == kCGLNoError && pf);
        assert(pf->egl_config == CONFIG_LARGE);
        CGLReleasePixelFormat(pf);
        attrs[1] = 0;
        assert(CGLChoosePixelFormat(attrs, &pf, NULL) == kCGLNoError && pf->egl_config == CONFIG);
        CGLReleasePixelFormat(pf);
    }
    CGLPixelFormatAttribute attrs[] = { kCGLPFAColorSize, 24, kCGLPFADepthSize, 24, 0, 0 };
    CGLPixelFormatObj pf = NULL;
    assert(CGLChoosePixelFormat(attrs, &pf, NULL) == kCGLNoError && pf->egl_config == CONFIG);
    CGLReleasePixelFormat(pf);
    attrs[3] = 25; attrs[4] = kCGLPFAMinimumPolicy;
    assert(CGLChoosePixelFormat(attrs, &pf, NULL) == kCGLNoError && pf->egl_config == CONFIG_LARGE);
    CGLReleasePixelFormat(pf);
    CGLPixelFormatAttribute alpha[] = { kCGLPFAAlphaSize, 33, 0 };
    assert(CGLChoosePixelFormat(alpha, &pf, NULL) == kCGLBadPixelFormat && !pf);
    alpha[0] = kCGLPFAColorSize; alpha[1] = 129;
    assert(CGLChoosePixelFormat(alpha, &pf, NULL) == kCGLBadPixelFormat && !pf);
    CGLPixelFormatAttribute color32[] = { kCGLPFAColorSize, 32, kCGLPFAMinimumPolicy, 0 };
    assert(CGLChoosePixelFormat(color32, &pf, NULL) == kCGLNoError && pf->egl_config == CONFIG);
    GLint bits = 0;
    assert(CGLDescribePixelFormat(pf, 0, kCGLPFAColorSize, &bits) == kCGLNoError && bits == 32);
    CGLReleasePixelFormat(pf);
    multi_configs = 0;
}

static void display_list_contexts(void) {
    for (int accelerated = 0; accelerated <= 1; ++accelerated) {
        CGLPixelFormatAttribute attrs[6];
        int i = 0;
        if (accelerated) attrs[i++] = kCGLPFAAccelerated;
        attrs[i++] = kCGLPFAColorSize; attrs[i++] = 24;
        attrs[i++] = kCGLPFAAlphaSize; attrs[i++] = 8;
        attrs[i] = 0;
        CGLPixelFormatObj format = NULL;
        CGLContextObj context = NULL;
        GLint count = 0;
        assert(CGLChoosePixelFormat(attrs, &format, &count) == kCGLNoError);
        assert(format && count > 0);
        assert(CGLCreateContext(format, NULL, &context) == kCGLNoError);
        assert(context);
        assert(CGLSetCurrentContext(context) == kCGLNoError);
        assert(CGLGetCurrentContext() == context);
        assert(CGLSetCurrentContext(NULL) == kCGLNoError);
        assert(CGLDestroyContext(context) == kCGLNoError);
        assert(CGLDestroyPixelFormat(format) == kCGLNoError);
    }
    puts("display-list context regression: PASS");
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--display-list-context")) {
        display_list_contexts();
        return 0;
    }
    if (argc == 2) {
        if (!strcmp(argv[1], "selection")) {
            selection_policy();
        } else {
            CGLPixelFormatAttribute attrs[] = { 0 };
            CGLPixelFormatObj pf = NULL;
            CGLContextObj ctx = NULL, shared = NULL;
            assert(CGLChoosePixelFormat(attrs, &pf, NULL) == kCGLNoError);
            assert(CGLCreateContext(pf, NULL, &ctx) == kCGLNoError);
            if (!strcmp(argv[1], "legacy")) {
                assert(CGLSetCurrentContext(ctx) == kCGLNoError);
                legacy_errors();
            } else {
                assert(!strcmp(argv[1], "null-format"));
                int attempts = create_calls;
                assert(CGLCreateContext(NULL, ctx, &shared) == kCGLBadPixelFormat && !shared);
                assert(create_calls == attempts);
            }
            CGLReleaseContext(ctx); CGLReleasePixelFormat(pf);
        }
        puts("review regression: PASS");
        return 0;
    }
    CGLPixelFormatAttribute attrs[] = { kCGLPFAAccelerated, kCGLPFAAllowOfflineRenderers,
        kCGLPFAOpenGLProfile, kCGLOGLPVersion_Legacy, kCGLPFAColorSize, 24,
        kCGLPFADepthSize, 24, kCGLPFAAlphaSize, 8, 0 };
    CGLPixelFormatObj pf = NULL; GLint screens = -1;
    fail_init = 1;
    assert(CGLChoosePixelFormat(attrs, &pf, &screens) == kCGLBadConnection);
    assert(!pf && screens == 0);
    fail_init = 0;
    assert(CGLChoosePixelFormat(attrs, &pf, &screens) == kCGLNoError && pf && screens == 1);
    assert(pf->egl_display == HEADLESS);
    GLint bits = 0;
    assert(CGLDescribePixelFormat(pf, 0, kCGLPFADepthSize, &bits) == kCGLNoError && bits >= 24);
    assert(CGLDescribePixelFormat(pf, 0, kCGLPFAColorSize, &bits) == kCGLNoError && bits >= 24);
    assert(CGLDescribePixelFormat(pf, 0, kCGLPFAAccelerated, &bits) == kCGLNoError && bits == 1);
    selection_policy();
    int probes = platform_calls;
    CGLPixelFormatObj second = NULL;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLNoError);
    assert(platform_calls == probes);
    CGLReleasePixelFormat(second);
    attrs[5] = 128;
    assert(CGLChoosePixelFormat(attrs, &second, NULL) == kCGLNoError && second);
    CGLReleasePixelFormat(second);
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
    int before_share = create_calls;
    b = a;
    assert(CGLCreateContext(NULL, a, &b) == kCGLBadPixelFormat && !b);
    assert(create_calls == before_share);
    assert(CGLCreateContext(pf, a, &b) == kCGLNoError && last_share == a->egl_context);
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
    assert(CGLCreateContext(second, b, &core_share) == kCGLNoError && last_share == b->egl_context);
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
