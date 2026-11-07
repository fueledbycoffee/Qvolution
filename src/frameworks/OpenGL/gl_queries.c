#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef CGL_NATIVE_GL_LIBRARY
#define CGL_NATIVE_GL_LIBRARY "/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib"
#endif

static void (*host_glGetBooleanv)(GLenum, GLboolean *);
static void (*host_glGetDoublev)(GLenum, GLdouble *);
static void (*host_glGetFloatv)(GLenum, GLfloat *);
static void (*host_glGetIntegerv)(GLenum, GLint *);
static void (*host_glGetInteger64v)(GLenum, GLint64 *);
static pthread_once_t queries_once = PTHREAD_ONCE_INIT;
static void *query_library;

static void *query_symbol(void *library, const char *name) {
    void *symbol = dlsym(library, name);
    if (!symbol) {
        fprintf(stderr, "OpenGL: cannot resolve native %s\n", name);
        abort();
    }
    return symbol;
}

static void initialize_queries(void) {
    // A specific handle avoids resolving back to these framework overrides.
    void *library = dlopen(CGL_NATIVE_GL_LIBRARY, RTLD_LAZY | RTLD_LOCAL);
    if (!library) {
        fputs("OpenGL: cannot load native GL queries\n", stderr);
        abort();
    }
    query_library = library;
#define RESOLVE(name) host_##name = query_symbol(library, #name)
    RESOLVE(glGetBooleanv);
    RESOLVE(glGetDoublev);
    RESOLVE(glGetFloatv);
    RESOLVE(glGetIntegerv);
    RESOLVE(glGetInteger64v);
#undef RESOLVE
}

__attribute__((destructor))
static void release_queries(void) {
    if (query_library)
        dlclose(query_library);
}

static int list_is_idle(void) {
    // Invalid queries (core profile, glBegin/glEnd, no current context) leave
    // the sentinel intact. Keep their output and error state with the driver.
    GLint index = -1;
    host_glGetIntegerv(GL_LIST_INDEX, &index);
    return index == 0;
}

// A rejected scalar mode query must not overwrite the caller's output.
#define QUERY(name, type) \
    void name(GLenum pname, type *params) { \
        pthread_once(&queries_once, initialize_queries); \
        if (pname != GL_LIST_MODE) { \
            host_##name(pname, params); \
            return; \
        } \
        type mode = (type)-1; \
        host_##name(pname, &mode); \
        if (mode != (type)-1) \
            *params = list_is_idle() ? 0 : mode; \
    }

QUERY(glGetBooleanv, GLboolean)
QUERY(glGetDoublev, GLdouble)
QUERY(glGetFloatv, GLfloat)
QUERY(glGetIntegerv, GLint)
QUERY(glGetInteger64v, GLint64)
