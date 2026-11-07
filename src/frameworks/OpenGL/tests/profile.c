#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <stdio.h>

int main(void) {
    const CGLPixelFormatAttribute profiles[] = {
        kCGLOGLPVersion_Legacy, kCGLOGLPVersion_3_2_Core, kCGLOGLPVersion_GL4_Core
    };
    const char *names[] = { "legacy", "core32", "core41" };
    for (unsigned i = 0; i < sizeof(profiles) / sizeof(profiles[0]); ++i) {
        CGLPixelFormatAttribute attrs[] = {
            kCGLPFAAccelerated, kCGLPFAAllowOfflineRenderers,
            kCGLPFAOpenGLProfile, profiles[i], 0
        };
        CGLPixelFormatObj pf = NULL;
        CGLContextObj ctx = NULL;
        GLint count = 0;
        CGLError error = CGLChoosePixelFormat(attrs, &pf, &count);
        if (!error) error = CGLCreateContext(pf, NULL, &ctx);
        if (!error) error = CGLSetCurrentContext(ctx);
        printf("CGL.profile(%s).error: %d\n", names[i], error);
        if (!error) {
            const char *version = (const char *)glGetString(GL_VERSION);
            int major = -1, minor = -1;
            if (version) sscanf(version, "%d.%d", &major, &minor);
            printf("GL.version(%s).parsed: %d.%d\n", names[i], major, minor);
            (void)glGetError();
            const GLenum queries[] = { GL_MAJOR_VERSION, GL_MINOR_VERSION };
            const char *qnames[] = { "major", "minor" };
            for (unsigned q = 0; q < 2; ++q) {
                printf("GL.%s(%s).string.null: %d\n", qnames[q], names[i], glGetString(queries[q]) == NULL);
                printf("GL.%s(%s).string.error: %u\n", qnames[q], names[i], glGetError());
#define QUERY(function, type, initial, label, spec, cast) \
                do { \
                    type value = initial; \
                    function(queries[q], &value); \
                    printf("GL.%s(%s).%s.value: " spec "\n", qnames[q], names[i], label, (cast)value); \
                    printf("GL.%s(%s).%s.error: %u\n", qnames[q], names[i], label, glGetError()); \
                    printf("GL.%s(%s).%s.cleared: %u\n", qnames[q], names[i], label, glGetError()); \
                } while (0)
                QUERY(glGetIntegerv, GLint, -7, "int", "%d", int);
                QUERY(glGetInteger64v, GLint64, -7, "int64", "%lld", long long);
                QUERY(glGetFloatv, GLfloat, -7, "float", "%.0f", double);
                QUERY(glGetDoublev, GLdouble, -7, "double", "%.0f", double);
                QUERY(glGetBooleanv, GLboolean, 17, "bool", "%u", unsigned);
#undef QUERY
            }
        }
        CGLSetCurrentContext(NULL);
        if (ctx) CGLDestroyContext(ctx);
        if (pf) CGLDestroyPixelFormat(pf);
    }
    return 0;
}
