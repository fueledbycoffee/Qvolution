#define GL_GLEXT_PROTOTYPES 1
#include <stdlib.h>
#include <stdio.h>  // for fprintf(stderr, "unimplemented")
#include <string.h>
#include <dlfcn.h>

#include <pthread.h>
#include <stdatomic.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/glext.h>
#include <OpenGL/CGLInternal.h>
#include <CoreFoundation/CFDictionary.h>
#include <pthread.h>

// Try to get the right (generic) type definitions.
// In particular, we really want EGLNativeDisplayType to be void *,
// not int as it is if __APPLE__ is defined.
#undef APPLE
#undef __APPLE__

#define __unix__
#define EGL_NO_X11

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_PLATFORM_X11_KHR
#define EGL_PLATFORM_X11_KHR 0x31D5
#endif

#include "swap_pacing.h"

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

#ifndef EGL_CONTEXT_MAJOR_VERSION
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#endif

#ifndef EGL_CONTEXT_MINOR_VERSION
#define EGL_CONTEXT_MINOR_VERSION 0x30FB
#endif

#ifndef EGL_CONTEXT_OPENGL_PROFILE_MASK
#define EGL_CONTEXT_OPENGL_PROFILE_MASK 0x30FD
#endif

#ifndef EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT 0x00000001
#endif

#define APPLE
#define __APPLE__

static EGLDisplay display;

static EGLConfig config;
static int num_config;
static int default_swap_interval = 1;
// Set while the registered display is Wayland, where the application's swap
// interval is kept in software (swap_pacing.h) and EGL stays at interval 0.
static int default_paced = 0;
static pthread_mutex_t registration_lock = PTHREAD_MUTEX_INITIALIZER;
static EGLDisplay offscreen_display = EGL_NO_DISPLAY;

static EGLBoolean choose_registered_config(EGLDisplay dpy, EGLConfig *out_config, int *out_count) {
    static EGLint const attrs_24[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };
    static EGLint const attrs_16[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };
    static EGLint const attrs_fallback[] = {
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLint count = 0;
    if (eglChooseConfig(dpy, attrs_24, out_config, 1, &count) && count > 0) {
        if (out_count) *out_count = count;
        return EGL_TRUE;
    }
    if (eglChooseConfig(dpy, attrs_16, out_config, 1, &count) && count > 0) {
        if (out_count) *out_count = count;
        return EGL_TRUE;
    }
    if (eglChooseConfig(dpy, attrs_fallback, out_config, 1, &count) && count > 0) {
        if (out_count) *out_count = count;
        return EGL_TRUE;
    }
    return EGL_FALSE;
}

struct _CGLDisplay
{
    EGLDisplay display;
    EGLConfig config;
    int num_config;
};

static CFMutableDictionaryRef g_displays;
static pthread_mutex_t g_displaysMutex = PTHREAD_MUTEX_INITIALIZER;

struct _CGLContextObj {
    GLuint retain_count;
    pthread_mutex_t lock;
    EGLDisplay egl_display;
    EGLConfig egl_config;
    EGLContext egl_context;
    _Atomic EGLSurface egl_surface;
    EGLSurface fallback_pbuffer;
    int native_framebuffer_initialized;
    // EGL has no function for getting the current swap interval,
    // so we need to save the last set value. The default is 1.
    // This is the application's value; a paced context never gives it to EGL.
    int swap_interval;
    int paced;
    struct cgl_swap_pacer pacer;
    CGLPixelFormatObj pixel_format;
    struct _CGLContextObj *prev_active;
    struct _CGLContextObj *next_active;
};

static struct _CGLContextObj *g_activeContexts = NULL;
static pthread_mutex_t g_activeContextsMutex = PTHREAD_MUTEX_INITIALIZER;

struct _CGLPixelFormatObj {
    GLuint retain_count;
    CGLPixelFormatAttribute *attributes;
    EGLDisplay egl_display;
    EGLConfig egl_config;
    int swap_interval;
    int paced;
};

static inline int attribute_has_argument(CGLPixelFormatAttribute attr) {
    switch (attr) {
    case kCGLPFAAuxBuffers:
    case kCGLPFAColorSize:
    case kCGLPFAAlphaSize:
    case kCGLPFADepthSize:
    case kCGLPFAStencilSize:
    case kCGLPFAAccumSize:
    case kCGLPFARendererID:
    case kCGLPFADisplayMask:
    case kCGLPFAOpenGLProfile:
    case kCGLPFASampleBuffers:
    case kCGLPFASamples:
        return 1;
    default:
        return 0;
   }
}

__attribute__((constructor))
static void _CGLInitialize(void)
{
    g_displays = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
}

static int attributes_count(const CGLPixelFormatAttribute *attrs) {
    int result;
    for (result = 0; attrs[result] != 0; result++) {
        if (attribute_has_argument(attrs[result])) {
            result++;
        }
    }
    return result;
}

CGLError CGLRegisterNativeDisplay(void *native_display) {
    if (!native_display)
        return kCGLBadConnection;
    EGLDisplay candidate = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, native_display, NULL);
    EGLConfig candidate_config;
    EGLint count;
    if (candidate == EGL_NO_DISPLAY || !eglInitialize(candidate, NULL, NULL))
        return kCGLBadConnection;
    if (!choose_registered_config(candidate, &candidate_config, &count))
        return kCGLBadPixelFormat;
    if (!eglBindAPI(EGL_OPENGL_API))
        return kCGLBadState;
    pthread_mutex_lock(&registration_lock);
    display = candidate;
    config = candidate_config;
    num_config = count;
    default_swap_interval = 1;
    default_paced = 0;
    pthread_mutex_unlock(&registration_lock);
    return kCGLNoError;
}

// Explicit-platform counterpart for backends whose native handles must not be
// interpreted using EGL's process default platform. Keep the X11 entry unchanged.
CGLError CGLRegisterNativeDisplayForPlatform(void *native_display, unsigned int platform) {
    if (!native_display)
        return kCGLBadConnection;
    EGLDisplay candidate = eglGetPlatformDisplay(platform, native_display, NULL);
    if (candidate == EGL_NO_DISPLAY)
        return kCGLBadConnection;
    EGLBoolean initialized = eglInitialize(candidate, NULL, NULL);
    if (!initialized)
        return kCGLBadConnection;
    CGLError error = kCGLBadConnection;
    const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    EGLint count = 0;
    if (!eglChooseConfig(candidate, attributes, NULL, 0, &count) || count <= 0) {
        error = kCGLBadPixelFormat;
        goto reject;
    }
    EGLConfig *configs = calloc((size_t)count, sizeof(*configs));
    if (!configs) {
        error = kCGLBadAlloc;
        goto reject;
    }
    EGLConfig candidate_config = NULL;
    if (eglChooseConfig(candidate, attributes, configs, count, &count)) {
        for (EGLint i = 0; i < count; ++i) {
            EGLint minimum_interval;
            if (eglGetConfigAttrib(candidate, configs[i], EGL_MIN_SWAP_INTERVAL, &minimum_interval) &&
                minimum_interval == 0) {
                candidate_config = configs[i];
                break;
            }
        }
    }
    free(configs);
    if (!candidate_config) {
        error = kCGLBadPixelFormat;
        goto reject;
    }
    if (!eglBindAPI(EGL_OPENGL_API)) {
        error = kCGLBadState;
        goto reject;
    }
    // Publish only a fully initialized display/config pair. A rejected platform
    // must not corrupt an already working backend.
    pthread_mutex_lock(&registration_lock);
    display = candidate;
    config = candidate_config;
    num_config = count;
    // Layer animations may keep drawing while their parent is hidden. Waiting
    // for a Wayland frame callback on an unmapped surface can block forever.
    default_swap_interval = 0;
    default_paced = platform == EGL_PLATFORM_WAYLAND_KHR;
    pthread_mutex_unlock(&registration_lock);
    return kCGLNoError;

reject:
    // EGL may return an existing handle still owned by a live context, even
    // after another backend registration changed the default display.
    // A failed probe must not terminate that shared display.
    return error;
}

static struct _CGLDisplay* getCGLDisplay(CGSConnectionID cid)
{
    struct _CGLDisplay* rv;

    pthread_mutex_lock(&g_displaysMutex);
    rv = (struct _CGLDisplay*) CFDictionaryGetValue(g_displays, (const void*)(unsigned long) cid);
    pthread_mutex_unlock(&g_displaysMutex);

    if (!rv)
    {
        void *native_display = _CGSNativeDisplay(cid);
        if (!native_display)
            return NULL;

        EGLDisplay disp = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, native_display, NULL);
        if (disp == EGL_NO_DISPLAY)
            return NULL;

        struct _CGLDisplay* new_disp = (struct _CGLDisplay*) malloc(sizeof(*new_disp));
        if (!new_disp)
            return NULL;

        new_disp->display = disp;
        if (!eglInitialize(new_disp->display, NULL, NULL))
        {
            free(new_disp);
            return NULL;
        }
        if (!choose_registered_config(new_disp->display, &new_disp->config, &new_disp->num_config))
        {
            eglTerminate(new_disp->display);
            free(new_disp);
            return NULL;
        }

        pthread_mutex_lock(&g_displaysMutex);
        rv = (struct _CGLDisplay*) CFDictionaryGetValue(g_displays, (const void*)(unsigned long) cid);
        if (rv)
        {
            pthread_mutex_unlock(&g_displaysMutex);
            free(new_disp);
            return rv;
        }
        CFDictionaryAddValue(g_displays, (const void*)(unsigned long) cid, new_disp);
        pthread_mutex_unlock(&g_displaysMutex);
        rv = new_disp;
    }

    return rv;
}

CGLError CGLSetSurface(CGLContextObj gl, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid)
{
    if (!gl)
        return kCGLBadContext;
    struct _CGLDisplay* disp = getCGLDisplay(cid);
    if (!disp)
        return kCGLBadConnection;

    EGLNativeWindowType window;
    if (sid)
        window = (EGLNativeWindowType) _CGSNativeWindowForSurfaceID(cid, wid, sid);
    else
        window = (EGLNativeWindowType) _CGSNativeWindowForID(cid, wid);

    if (!window)
        return kCGLBadWindow;

    EGLSurface surf = eglCreateWindowSurface(gl->egl_display, gl->egl_config, window, NULL);
    atomic_store_explicit(&gl->egl_surface, surf, memory_order_release);
    if (surf == EGL_NO_SURFACE)
        return kCGLBadState;
    return kCGLNoError;
}

// CGLWindowRef is opaque: retain the display that owns its EGL surface.
struct _CGLWindow {
    EGLDisplay display;
    EGLSurface surface;
};

static CGLWindowRef createWindow(EGLDisplay owner, EGLConfig owner_config, void *native_window) {
    if (!native_window)
        return NULL;
    struct _CGLWindow *window = malloc(sizeof(*window));
    if (!window)
        return NULL;
    window->display = owner;
    window->surface = eglCreateWindowSurface(owner, owner_config,
                                             (EGLNativeWindowType) native_window, NULL);
    if (window->surface == EGL_NO_SURFACE) {
        free(window);
        return NULL;
    }
    return window;
}

CGLWindowRef CGLGetWindow(void *native_window) {
    pthread_mutex_lock(&registration_lock);
    EGLDisplay owner = display;
    EGLConfig owner_config = config;
    pthread_mutex_unlock(&registration_lock);
    return createWindow(owner, owner_config, native_window);
}

CGLWindowRef CGLGetWindowForContext(CGLContextObj context, void *native_window) {
    if (!context)
        return NULL;
    return createWindow(context->egl_display, context->egl_config, native_window);
}

CGL_EXPORT void CGLDestroyWindow(CGLWindowRef ref) {
    struct _CGLWindow *window = ref;
    if (!window)
        return;

    CGLContextObj current = CGLGetCurrentContext();
    int current_cleared = 0;

    if (window->surface != EGL_NO_SURFACE) {
        pthread_mutex_lock(&g_activeContextsMutex);
        for (struct _CGLContextObj *ctx = g_activeContexts; ctx != NULL; ctx = ctx->next_active) {
            EGLSurface target = window->surface;
            if (atomic_compare_exchange_strong_explicit(&ctx->egl_surface, &target, (EGLSurface)EGL_NO_SURFACE,
                                                        memory_order_acq_rel, memory_order_acquire)) {
                if (ctx == current) {
                    current_cleared = 1;
                }
            }
        }
        pthread_mutex_unlock(&g_activeContextsMutex);
    }

    if (current_cleared && current && current->egl_display != EGL_NO_DISPLAY) {
        // Disassociate the destroyed window surface from the current EGL thread
        eglMakeCurrent(current->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, current->egl_context);
    }

    eglDestroySurface(window->display, window->surface);
    free(window);
}

static int cgl_has_extension(const char *extensions, const char *name) {
    if (!extensions)
        return 0;
    size_t length = strlen(name);
    const char *match = extensions;
    while ((match = strstr(match, name)) != NULL) {
        if ((match == extensions || match[-1] == ' ') &&
            (match[length] == ' ' || match[length] == '\0'))
            return 1;
        match += length;
    }
    return 0;
}

static void cgl_initialize_native_framebuffer(GLenum buffer) {
    const char *version = (const char *)glGetString(GL_VERSION);
    long major = version ? strtol(version, NULL, 10) : 0;
    const char *extensions = major < 3 ? (const char *)glGetString(GL_EXTENSIONS) : NULL;
    if (major >= 3 || cgl_has_extension(extensions, "GL_ARB_framebuffer_object")) {
        GLint draw, read;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glDrawBuffer(buffer);
        glReadBuffer(buffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
    } else if (cgl_has_extension(extensions, "GL_EXT_framebuffer_object")) {
        GLint framebuffer;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &framebuffer);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        glDrawBuffer(buffer);
        glReadBuffer(buffer);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    } else {
        glDrawBuffer(buffer);
        glReadBuffer(buffer);
    }
}

CGL_EXPORT CGLError CGLContextMakeCurrentAndAttachToWindow(CGLContextObj context, CGLWindowRef window) {
    if (!context)
        return kCGLBadContext;
    if (!window)
        return kCGLBadDrawable;
    struct _CGLWindow *drawable = window;
    if (drawable->display != context->egl_display)
        return kCGLBadMatch;
    EGLSurface previous_surface = atomic_load_explicit(&context->egl_surface, memory_order_acquire);
    EGLSurface old_fallback = context->fallback_pbuffer;
    atomic_store_explicit(&context->egl_surface, drawable->surface, memory_order_release);
    CGLError error = CGLSetCurrentContext(context);
    if (error != kCGLNoError) {
        atomic_store_explicit(&context->egl_surface, previous_surface, memory_order_release);
        return error;
    }
    if (old_fallback != EGL_NO_SURFACE) {
        eglDestroySurface(context->egl_display, old_fallback);
        context->fallback_pbuffer = EGL_NO_SURFACE;
    }
    return kCGLNoError;
}

static pthread_key_t current_context_key;

static void make_key() {
    pthread_key_create(&current_context_key, NULL);
}

// What EGL gets: a paced context must never wait for a frame callback.
static int egl_swap_interval(CGLContextObj context) {
    return context->paced ? 0 : context->swap_interval;
}

static pthread_key_t get_current_context_key() {
    static pthread_once_t key_once = PTHREAD_ONCE_INIT;
    pthread_once(&key_once, make_key);
    return current_context_key;
}

CGLContextObj CGLGetCurrentContext(void) {
    pthread_key_t key = get_current_context_key();
    return pthread_getspecific(key);
}

CGLError CGLSetCurrentContext(CGLContextObj context) {
    if (!eglBindAPI(EGL_OPENGL_API))
        return kCGLBadState;
    if (context != NULL) {
        EGLSurface surface = context->egl_surface;
        GLenum native_buffer = GL_NONE;
        if (surface != EGL_NO_SURFACE && surface != context->fallback_pbuffer &&
                !context->native_framebuffer_initialized) {
            EGLint render_buffer;
            if (!eglQuerySurface(context->egl_display, surface, EGL_RENDER_BUFFER, &render_buffer))
                return kCGLBadDrawable;
            if (render_buffer == EGL_BACK_BUFFER)
                native_buffer = GL_BACK;
            else if (render_buffer == EGL_SINGLE_BUFFER)
                native_buffer = GL_FRONT;
            else
                return kCGLBadDrawable;
        }
        if (surface == EGL_NO_SURFACE) {
            if (!eglMakeCurrent(context->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, context->egl_context)) {
                const EGLint pbufferAttribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
                EGLSurface pbuf = eglCreatePbufferSurface(context->egl_display, context->egl_config, pbufferAttribs);
                if (pbuf != EGL_NO_SURFACE && eglMakeCurrent(context->egl_display, pbuf, pbuf, context->egl_context)) {
                    context->fallback_pbuffer = pbuf;
                    atomic_store_explicit(&context->egl_surface, pbuf, memory_order_release);
                    surface = pbuf;
                } else {
                    if (pbuf != EGL_NO_SURFACE)
                        eglDestroySurface(context->egl_display, pbuf);
                    return kCGLBadContext;
                }
            }
        } else {
            if (!eglMakeCurrent(context->egl_display, surface, surface, context->egl_context)) {
                return kCGLBadContext;
            }
        }
        // Surfaceless first use can leave the default draw/read buffers at NONE.
        // Initialize only once; later attachments retain explicit client choices.
        if (native_buffer != GL_NONE) {
            cgl_initialize_native_framebuffer(native_buffer);
            context->native_framebuffer_initialized = 1;
        }
        pthread_setspecific(get_current_context_key(), context);
        if (surface != EGL_NO_SURFACE) {
            eglSwapInterval(context->egl_display, egl_swap_interval(context));
        }
    } else {
        CGLContextObj current = CGLGetCurrentContext();
        if (!current)
            return kCGLNoError;
        if (!eglMakeCurrent(current->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
            return kCGLBadContext;
        pthread_setspecific(get_current_context_key(), NULL);
    }
    return kCGLNoError;
}

CGLError CGLEnable(CGLContextObj ctx, CGLContextEnable pname) {
    return kCGLNoError;
}

CGLError CGLDisable(CGLContextObj ctx, CGLContextEnable pname) {
    return kCGLNoError;
}

CGLError CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname, GLint *enable) {
    if (enable) *enable = 0;
    return kCGLNoError;
}

const char *CGLErrorString(CGLError error) {
    switch (error) {
    case kCGLNoError: return "no error";
    case kCGLBadAttribute: return "invalid pixel format attribute";
    case kCGLBadProperty: return "invalid renderer property";
    case kCGLBadPixelFormat: return "invalid pixel format";
    case kCGLBadRendererInfo: return "invalid renderer info";
    case kCGLBadContext: return "invalid context";
    case kCGLBadDrawable: return "invalid drawable";
    case kCGLBadDisplay: return "invalid display";
    case kCGLBadState: return "invalid context state";
    case kCGLBadValue: return "invalid numerical value";
    case kCGLBadMatch: return "invalid share context";
    case kCGLBadEnumeration: return "invalid enumerant";
    case kCGLBadOffScreen: return "invalid offscreen drawable";
    case kCGLBadFullScreen: return "invalid fullscreen drawable";
    case kCGLBadWindow: return "invalid window";
    case kCGLBadAddress: return "invalid pointer";
    case kCGLBadCodeModule: return "invalid code module";
    case kCGLBadAlloc: return "memory allocation failure";
    case kCGLBadConnection: return "invalid CoreGraphics connection";
    default: return "unknown error";
    }
}

CGLError CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target, GLenum internal_format,
                                GLsizei width, GLsizei height, GLenum format, GLenum type,
                                void *ioSurface, GLuint plane)
{
    return kCGLNoError;
}

CGLError CGLSetFullScreen(CGLContextObj ctx) {
    printf("STUB: CGLSetFullScreen\n");

    return kCGLNoError;
}

CGLError CGLChoosePixelFormat(
    const CGLPixelFormatAttribute *attrs,
    CGLPixelFormatObj *result,
    GLint *number_of_screens
) {
    if (!result)
        return kCGLBadAddress;
    *result = NULL;
    if (number_of_screens)
        *number_of_screens = 0;
    if (!attrs)
        return kCGLBadAttribute;
    int count = attributes_count(attrs);
    EGLint egl_attrs[32] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT };
    int n = 2;
    GLint color_size = 0;
    for (int i = 0; i < count; ++i) {
        CGLPixelFormatAttribute attr = attrs[i];
        GLint value = attribute_has_argument(attr) ? (GLint)attrs[++i] : 1;
        EGLint egl_attr = EGL_NONE;
        switch (attr) {
        case kCGLPFAColorSize: color_size = value; break;
        case kCGLPFAAlphaSize: egl_attr = EGL_ALPHA_SIZE; break;
        case kCGLPFADepthSize: egl_attr = EGL_DEPTH_SIZE; break;
        case kCGLPFAStencilSize: egl_attr = EGL_STENCIL_SIZE; break;
        case kCGLPFASampleBuffers: egl_attr = EGL_SAMPLE_BUFFERS; break;
        case kCGLPFASamples: egl_attr = EGL_SAMPLES; break;
        case kCGLPFAAccelerated: egl_attr = EGL_CONFIG_CAVEAT; value = EGL_NONE; break;
        case kCGLPFAOpenGLProfile:
            if (value != kCGLOGLPVersion_Legacy && value != kCGLOGLPVersion_3_2_Core &&
                    value != kCGLOGLPVersion_GL4_Core)
                return kCGLBadAttribute;
            break;
        default: break;
        }
        if (value < 0)
            return kCGLBadAttribute;
        if (egl_attr != EGL_NONE) {
            if (n + 2 >= (int)(sizeof(egl_attrs) / sizeof(egl_attrs[0])) - 2)
                return kCGLBadAttribute;
            egl_attrs[n++] = egl_attr;
            egl_attrs[n++] = value;
        }
    }

    pthread_mutex_lock(&registration_lock);
    EGLDisplay owner = display;
    int interval = default_swap_interval;
    int paced = default_paced;
    if (owner == EGL_NO_DISPLAY) {
        if (offscreen_display == EGL_NO_DISPLAY) {
            EGLDisplay candidate = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                                         EGL_DEFAULT_DISPLAY, NULL);
            if (candidate != EGL_NO_DISPLAY && eglInitialize(candidate, NULL, NULL))
                offscreen_display = candidate;
        }
        owner = offscreen_display;
    }
    pthread_mutex_unlock(&registration_lock);
    if (owner == EGL_NO_DISPLAY)
        return kCGLBadConnection;

    // Window-capable configs remain attachable; a headless display needs only
    // pbuffer support for drivers that cannot make a surfaceless context current.
    egl_attrs[n++] = EGL_SURFACE_TYPE;
    egl_attrs[n++] = owner == offscreen_display ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT;
    egl_attrs[n] = EGL_NONE;
    EGLint available = 0;
    if (!eglChooseConfig(owner, egl_attrs, NULL, 0, &available) || available == 0)
        return kCGLBadPixelFormat;
    EGLConfig *configs = calloc((size_t)available, sizeof(*configs));
    if (!configs)
        return kCGLBadAlloc;
    EGLConfig selected = NULL;
    if (eglChooseConfig(owner, egl_attrs, configs, available, &available)) {
        for (EGLint i = 0; i < available; ++i) {
            EGLint red, green, blue, minimum_interval;
            if (paced && (!eglGetConfigAttrib(owner, configs[i], EGL_MIN_SWAP_INTERVAL, &minimum_interval) ||
                    minimum_interval != 0))
                continue;
            if (eglGetConfigAttrib(owner, configs[i], EGL_RED_SIZE, &red) &&
                    eglGetConfigAttrib(owner, configs[i], EGL_GREEN_SIZE, &green) &&
                    eglGetConfigAttrib(owner, configs[i], EGL_BLUE_SIZE, &blue) &&
                    red + green + blue >= color_size) {
                selected = configs[i];
                break;
            }
        }
    }
    free(configs);
    if (!selected)
        return kCGLBadPixelFormat;

    CGLPixelFormatObj format = malloc(sizeof(struct _CGLPixelFormatObj));
    if (!format)
        return kCGLBadAlloc;
    format->egl_display = owner;
    format->egl_config = selected;
    format->swap_interval = interval;
    format->paced = paced;
    format->retain_count = 1;
    format->attributes = malloc(sizeof(CGLPixelFormatAttribute) * (count + 1));
    if (!format->attributes) {
        free(format);
        return kCGLBadAlloc;
    }
    for (int i = 0; i <= count; i++) {
        format->attributes[i] = attrs[i];
    }

    *result = format;
    if (number_of_screens) {
        *number_of_screens = 1;
    }

    return kCGLNoError;
}

CGLError CGLClearDrawable(CGLContextObj ctx)
{
    if (!ctx)
        return kCGLBadContext;

    pthread_mutex_lock(&ctx->lock);
    EGLSurface old_fallback = ctx->fallback_pbuffer;
    ctx->fallback_pbuffer = EGL_NO_SURFACE;
    atomic_store_explicit(&ctx->egl_surface, (EGLSurface)EGL_NO_SURFACE, memory_order_release);
    int is_current = (CGLGetCurrentContext() == ctx);
    pthread_mutex_unlock(&ctx->lock);

    /* egl_display and egl_context are immutable after CGLCreateContext */
    if (ctx->egl_display != EGL_NO_DISPLAY) {
        if (old_fallback != EGL_NO_SURFACE) {
            eglDestroySurface(ctx->egl_display, old_fallback);
        }
        if (is_current) {
            // Disassociate any surface from the current thread's EGL context
            eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx->egl_context);
        }
    }

    return kCGLNoError;
}

CGLError CGLUpdateContext(CGLContextObj ctx)
{
    return kCGLNoError;
}

CGLError CGLDescribePixelFormat(
    CGLPixelFormatObj format,
    GLint sreen_num,
    CGLPixelFormatAttribute attr,
    GLint *value
) {
    if (!format || !format->attributes) {
        return kCGLBadPixelFormat;
    }
    if (!value) {
        return kCGLBadAddress;
    }
    if (sreen_num != 0)
        return kCGLBadValue;
    EGLint egl_attr = EGL_NONE;
    switch (attr) {
    case kCGLPFAColorSize: {
        EGLint red, green, blue;
        if (!eglGetConfigAttrib(format->egl_display, format->egl_config, EGL_RED_SIZE, &red) ||
                !eglGetConfigAttrib(format->egl_display, format->egl_config, EGL_GREEN_SIZE, &green) ||
                !eglGetConfigAttrib(format->egl_display, format->egl_config, EGL_BLUE_SIZE, &blue))
            return kCGLBadPixelFormat;
        *value = red + green + blue;
        return kCGLNoError;
    }
    case kCGLPFAAccelerated: {
        EGLint caveat;
        if (!eglGetConfigAttrib(format->egl_display, format->egl_config, EGL_CONFIG_CAVEAT, &caveat))
            return kCGLBadPixelFormat;
        *value = caveat == EGL_NONE;
        return kCGLNoError;
    }
    case kCGLPFAAlphaSize: egl_attr = EGL_ALPHA_SIZE; break;
    case kCGLPFADepthSize: egl_attr = EGL_DEPTH_SIZE; break;
    case kCGLPFAStencilSize: egl_attr = EGL_STENCIL_SIZE; break;
    case kCGLPFASampleBuffers: egl_attr = EGL_SAMPLE_BUFFERS; break;
    case kCGLPFASamples: egl_attr = EGL_SAMPLES; break;
    case kCGLPFAVirtualScreenCount: *value = 1; return kCGLNoError;
    default: break;
    }
    if (egl_attr != EGL_NONE)
        return eglGetConfigAttrib(format->egl_display, format->egl_config, egl_attr, value)
            ? kCGLNoError : kCGLBadPixelFormat;

    for (int i = 0; format->attributes[i] != 0; i++) {
        int has_arg = attribute_has_argument(format->attributes[i]);

        if (format->attributes[i] == attr) {
            if (has_arg) {
                *value = format->attributes[i + 1];
            } else {
                *value = 1;
            }
            return kCGLNoError;
        }

        if (has_arg) {
            i++;
        }
    }

    *value = 0;
    return kCGLNoError;
}

CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj format) {
    if (format == NULL) {
        return NULL;
    }

    __sync_fetch_and_add(&format->retain_count, 1);
    return format;
}

void CGLReleasePixelFormat(CGLPixelFormatObj format) {
    if (format == NULL) {
        return;
    }

    if (__sync_sub_and_fetch(&format->retain_count, 1) == 0) {
        free(format->attributes);
        free(format);
    }
}

CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pixelFormat) {
    CGLReleasePixelFormat(pixelFormat);
    return kCGLNoError;
}

GLuint CGLGetPixelFormatRetainCount(CGLPixelFormatObj pixelFormat) {
    return pixelFormat->retain_count;
}

CGLError CGLCreateContext(CGLPixelFormatObj pixelFormat, CGLContextObj share, CGLContextObj *resultp) {

    if (resultp == NULL)
        return kCGLBadAddress;
    *resultp = NULL;

    if (!pixelFormat && share)
        pixelFormat = share->pixel_format;
    if (!pixelFormat)
        return kCGLBadPixelFormat;
    EGLDisplay owner = pixelFormat->egl_display;
    EGLConfig owner_config = pixelFormat->egl_config;
    int interval = pixelFormat->swap_interval;
    int paced = pixelFormat->paced;
    if (share && share->egl_display != owner)
        return kCGLBadMatch;
    if (!eglBindAPI(EGL_OPENGL_API))
        return kCGLBadState;
    EGLContext egl_share = share ? share->egl_context : EGL_NO_CONTEXT;
    EGLint context_attribs[16] = { EGL_NONE };
    int attr_idx = 0;
    GLint profile = 0;
    if (pixelFormat && CGLDescribePixelFormat(pixelFormat, 0, kCGLPFAOpenGLProfile, &profile) == kCGLNoError) {
        if (profile == kCGLOGLPVersion_3_2_Core) {
            context_attribs[attr_idx++] = EGL_CONTEXT_MAJOR_VERSION;
            context_attribs[attr_idx++] = 3;
            context_attribs[attr_idx++] = EGL_CONTEXT_MINOR_VERSION;
            context_attribs[attr_idx++] = 2;
            context_attribs[attr_idx++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
            context_attribs[attr_idx++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
        } else if (profile == kCGLOGLPVersion_GL4_Core) {
            context_attribs[attr_idx++] = EGL_CONTEXT_MAJOR_VERSION;
            context_attribs[attr_idx++] = 4;
            context_attribs[attr_idx++] = EGL_CONTEXT_MINOR_VERSION;
            context_attribs[attr_idx++] = 1;
            context_attribs[attr_idx++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
            context_attribs[attr_idx++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
        }
        /* Note: kCGLOGLPVersion_Legacy (0x1000) passes NULL attributes to request a compatibility context */
    }
    context_attribs[attr_idx] = EGL_NONE;
    EGLContext egl_context = eglCreateContext(owner, owner_config, egl_share, attr_idx > 0 ? context_attribs : NULL);
    if (egl_context == EGL_NO_CONTEXT)
        return share && eglGetError() == EGL_BAD_MATCH ? kCGLBadMatch : kCGLBadContext;

    CGLContextObj context = malloc(sizeof(struct _CGLContextObj));

    if (context == NULL) {
        eglDestroyContext(owner, egl_context);
        return kCGLBadAlloc;
    }

    context->retain_count = 1;
    pthread_mutexattr_t lock_attr;
    pthread_mutexattr_init(&lock_attr);
    pthread_mutexattr_settype(&lock_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&(context->lock), &lock_attr);
    pthread_mutexattr_destroy(&lock_attr);
    context->egl_display = owner;
    context->egl_config = owner_config;
    context->egl_context = egl_context;
    atomic_init(&context->egl_surface, (EGLSurface)EGL_NO_SURFACE);
    context->fallback_pbuffer = EGL_NO_SURFACE;
    context->native_framebuffer_initialized = 0;
    context->swap_interval = interval;
    context->paced = paced;
    context->pacer.period_ns = cgl_period_ns_from_millihertz(0);
    context->pacer.last_ns = 0;
    context->pixel_format = pixelFormat ? CGLRetainPixelFormat(pixelFormat) : NULL;

    pthread_mutex_lock(&g_activeContextsMutex);
    context->prev_active = NULL;
    context->next_active = g_activeContexts;
    if (g_activeContexts != NULL) {
        g_activeContexts->prev_active = context;
    }
    g_activeContexts = context;
    pthread_mutex_unlock(&g_activeContextsMutex);

    *resultp = context;

    return kCGLNoError;
}

CGLContextObj CGLRetainContext(CGLContextObj context) {
    if (context == NULL) {
        return NULL;
    }

    __sync_fetch_and_add(&context->retain_count, 1);
    return context;
}

void CGLReleaseContext(CGLContextObj context) {
    if (context == NULL) {
        return;
    }

    if (__sync_sub_and_fetch(&context->retain_count, 1) != 0) {
        return;
    }

    pthread_mutex_lock(&g_activeContextsMutex);
    if (context->prev_active != NULL) {
        context->prev_active->next_active = context->next_active;
    } else if (g_activeContexts == context) {
        g_activeContexts = context->next_active;
    }
    if (context->next_active != NULL) {
        context->next_active->prev_active = context->prev_active;
    }
    context->prev_active = NULL;
    context->next_active = NULL;
    pthread_mutex_unlock(&g_activeContextsMutex);

    if (CGLGetCurrentContext() == context) {
        CGLSetCurrentContext(NULL);
    }

    if (context->fallback_pbuffer != EGL_NO_SURFACE) {
        eglDestroySurface(context->egl_display, context->fallback_pbuffer);
        context->fallback_pbuffer = EGL_NO_SURFACE;
    }

    pthread_mutex_destroy(&(context->lock));

    if (context->pixel_format != NULL)
        CGLReleasePixelFormat(context->pixel_format);

    eglDestroyContext(context->egl_display, context->egl_context);

    free(context);
}

CGL_EXPORT CGLPixelFormatObj CGLGetPixelFormat(CGLContextObj context) {
    if (context == NULL)
        return NULL;
    return context->pixel_format;
}

GLuint CGLGetContextRetainCount(CGLContextObj context) {
    if (context == NULL) {
        return 0;
    }

    return context->retain_count;
}

CGLError CGLDestroyContext(CGLContextObj context) {
    CGLReleaseContext(context);

    return kCGLNoError;
}

CGLError CGLLockContext(CGLContextObj context) {
    if (context == NULL) {
        return kCGLBadContext;
    }
    pthread_mutex_lock(&(context->lock));
    return kCGLNoError;
}

CGLError CGLUnlockContext(CGLContextObj context) {
    if (context == NULL) {
        return kCGLBadContext;
    }
    pthread_mutex_unlock(&(context->lock));
    return kCGLNoError;
}

static CGLError cgl_flush_drawable(CGLContextObj context, int finish) {
    if (context == NULL)
        return kCGLBadContext;
    if (context->egl_surface == EGL_NO_SURFACE)
        return kCGLBadDrawable;
    if (finish)
        glFinish();
    if (context->paced)
        cgl_swap_pace(&context->pacer, context->swap_interval);
    return eglSwapBuffers(context->egl_display, context->egl_surface) ? kCGLNoError : kCGLBadDrawable;
}

CGLError CGLFlushDrawable(CGLContextObj context) {
    return cgl_flush_drawable(context, 1);
}

// eglSwapBuffers already flushes the context; only callers that need the GPU
// to have finished before the swap returns should pay for glFinish.
CGLError CGLFlushDrawableWithoutFinish(CGLContextObj context) {
    return cgl_flush_drawable(context, 0);
}

CGLError CGLSetDrawableRefreshRate(CGLContextObj context, uint32_t millihertz) {
    if (context == NULL)
        return kCGLBadContext;
    context->pacer.period_ns = cgl_period_ns_from_millihertz(millihertz);
    return kCGLNoError;
}

CGLError CGLSetParameter(CGLContextObj context, CGLContextParameter parameter, const GLint *value) {
    if (!value)
        return kCGLBadAddress;
    if (!context)
        return kCGLBadContext;

    if (parameter == kCGLCPSwapInterval)
    {
        GLint v = *value;
        if (!context)
            return kCGLBadContext;
        EGLBoolean success = EGL_TRUE;
        if (CGLGetCurrentContext() == context && atomic_load_explicit(&context->egl_surface, memory_order_acquire) != EGL_NO_SURFACE)
            success = eglSwapInterval(context->egl_display, context->paced ? 0 : v);
        if (success)
            context->swap_interval = v;
        return success ? kCGLNoError : kCGLBadValue;
    }
    return kCGLBadEnumeration;
}

CGLError CGLGetParameter(CGLContextObj context, CGLContextParameter parameter, GLint *value) {
    if (!value)
        return kCGLBadAddress;
    if (!context)
        return kCGLBadContext;

    if (parameter == kCGLCPSwapInterval)
    {
        *value = context->swap_interval;
        return kCGLNoError;
    }
    return kCGLBadEnumeration;
}

CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, long rend_num, CGLRendererProperty prop, long *value) {
    return kCGLNoError;
}

CGLError CGLQueryRendererInfo(unsigned long display_mask, CGLRendererInfoObj *rend, long *nrend) {
    return kCGLNoError;
}

CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend) {
    return kCGLNoError;
}

void CGLGetVersion(GLint *majorvers, GLint *minorvers) {
    if (majorvers)
        *majorvers = 1;
    if (minorvers)
        *minorvers = 0;
}
