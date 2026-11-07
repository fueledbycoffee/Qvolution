#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdint.h>
#include <stdio.h>

// The legacy SDK header omits the 3.2 query declaration (vendored gl3.h).
extern void glGetInteger64v(GLenum pname, int64_t *data);

static void mode(const char *phase) {
    GLint i = -1, index = -1;
    GLboolean b = 7;
    GLfloat f = -1;
    GLdouble d = -1;
    int64_t wide = -1;
    glGetIntegerv(GL_LIST_MODE, &i);
    glGetBooleanv(GL_LIST_MODE, &b);
    glGetFloatv(GL_LIST_MODE, &f);
    glGetDoublev(GL_LIST_MODE, &d);
    glGetInteger64v(GL_LIST_MODE, &wide);
    glGetIntegerv(GL_LIST_INDEX, &index);
    printf("OpenGL.listMode(%s): int=%d bool=%u float=%.0f double=%.0f int64=%lld compiling=%d error=%u\n",
           phase, i, b, f, d, (long long)wide, index != 0, glGetError());
}

int main(void) {
    CGLPixelFormatAttribute attrs[] = {kCGLPFAColorSize, 24, kCGLPFAAlphaSize, 8, 0};
    CGLPixelFormatObj format = NULL;
    CGLContextObj context = NULL;
    GLint count = 0;
    CGLError error = CGLChoosePixelFormat(attrs, &format, &count);
    printf("CGL.choosePixelFormat(list-mode): error=%d format=%d\n", error, format != NULL);
    if (error || !format) return 1;
    error = CGLCreateContext(format, NULL, &context);
    printf("CGL.createContext(list-mode): error=%d context=%d\n", error, context != NULL);
    if (error || !context) { CGLDestroyPixelFormat(format); return 1; }
    error = CGLSetCurrentContext(context);
    printf("CGL.setCurrentContext(list-mode): error=%d\n", error);
    if (!error) {
        GLuint list = glGenLists(1);
        mode("initial");
        glNewList(list, GL_COMPILE); mode("compile"); glEndList(); mode("ended");
        glNewList(list, GL_COMPILE_AND_EXECUTE); mode("compile-execute"); glEndList(); mode("redefined");
        glCallList(list); mode("replayed");
        glDeleteLists(list, 1); mode("deleted");
        CGLSetCurrentContext(NULL);
    }
    CGLDestroyContext(context);
    CGLDestroyPixelFormat(format);
    return error != kCGLNoError;
}
