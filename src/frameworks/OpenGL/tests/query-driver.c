// CPU fixture reproducing the stale GL_LIST_MODE observed in the Darling run.
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

static _Thread_local struct {
    GLint index, mode;
    int primitive, core;
    GLenum error;
} contexts[2];
static _Thread_local unsigned current;

void query_driver_context(unsigned context, int core) {
    current = context;
    contexts[current].core = core;
}

void glNewList(GLuint list, GLenum mode) {
    contexts[current].index = list;
    contexts[current].mode = mode;
}
void glEndList(void) { contexts[current].index = 0; }
void glBegin(GLenum mode) { contexts[current].primitive = 1; }
void glEnd(void) { contexts[current].primitive = 0; }
GLenum glGetError(void) {
    GLenum error = contexts[current].error;
    contexts[current].error = GL_NO_ERROR;
    return error;
}

#define GET(name, type, convert, wide_only) \
    void name(GLenum pname, type *params) { \
        if (contexts[current].primitive || contexts[current].core == 1 || \
            (wide_only && contexts[current].core == 2)) { \
            if (!contexts[current].error) \
                contexts[current].error = contexts[current].primitive ? GL_INVALID_OPERATION : GL_INVALID_ENUM; \
            return; \
        } \
        double value; \
        if (pname == GL_LIST_MODE) \
            value = contexts[current].mode ? contexts[current].mode : GL_COMPILE; \
        else if (pname == GL_LIST_INDEX) \
            value = contexts[current].index; \
        else if (pname == GL_LINE_WIDTH) \
            value = 2.5; \
        else if (pname == GL_VIEWPORT) { \
            for (int i = 0; i < 4; ++i) { value = i + 10; params[i] = (convert); } \
            return; \
        } else { \
            if (!contexts[current].error) contexts[current].error = GL_INVALID_ENUM; \
            return; \
        } \
        *params = (convert); \
    }
GET(glGetBooleanv, GLboolean, value != 0, 0)
GET(glGetDoublev, GLdouble, value, 0)
GET(glGetFloatv, GLfloat, value, 0)
GET(glGetIntegerv, GLint, (GLint)value, 0)
GET(glGetInteger64v, GLint64, (GLint64)value, 1)
