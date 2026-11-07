#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include "../gl_queries.h"
#ifndef CGL_TEST_PUBLIC_GETTERS
#define glGetBooleanv cgl_glGetBooleanv
#define glGetDoublev cgl_glGetDoublev
#define glGetFloatv cgl_glGetFloatv
#define glGetIntegerv cgl_glGetIntegerv
#define glGetInteger64v cgl_glGetInteger64v
#endif
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

void query_driver_context(unsigned context, int core);

static void mode(GLint expected) {
    struct { GLint value, guard; } i = {-5, 91};
    GLboolean b = 9;
    GLdouble d = -5;
    GLfloat f = -5;
    GLint64 wide = -5;
    glGetIntegerv(GL_LIST_MODE, &i.value);
    glGetBooleanv(GL_LIST_MODE, &b);
    glGetDoublev(GL_LIST_MODE, &d);
    glGetFloatv(GL_LIST_MODE, &f);
    glGetInteger64v(GL_LIST_MODE, &wide);
    assert(i.value == expected && i.guard == 91);
    assert(b == (expected != 0) && d == expected && f == expected && wide == expected);
    assert(glGetError() == GL_NO_ERROR);
}

static void *thread(void *unused) {
    mode(0);
    glNewList(83, GL_COMPILE);
    mode(GL_COMPILE);
    glEndList();
    mode(0);
    return NULL;
}

int main(void) {
    glNewList(42, GL_COMPILE);
    glEndList();
    mode(0);
    for (int pass = 0; pass < 2; ++pass) {
        GLenum compile = pass ? GL_COMPILE_AND_EXECUTE : GL_COMPILE;
        glNewList(42, compile);
        mode(compile);
        GLint index = 0;
        glGetIntegerv(GL_LIST_INDEX, &index);
        assert(index == 42);
        query_driver_context(1, 0);
        mode(0);
        query_driver_context(0, 0);
        mode(compile);
        pthread_t other;
        assert(pthread_create(&other, NULL, thread, NULL) == 0);
        assert(pthread_join(other, NULL) == 0);
        mode(compile);
        glEndList();
        mode(0);
    }
    GLint viewport[4] = {0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    for (int i = 0; i < 4; ++i) assert(viewport[i] == i + 10);
    GLfloat width = 0;
    glGetFloatv(GL_LINE_WIDTH, &width);
    assert(width == 2.5f);
    GLint value = 123;
    glGetIntegerv((GLenum)-1, &value);
    assert(value == 123);
    glGetIntegerv(GL_LIST_MODE, &value);
    assert(value == 0 && glGetError() == GL_INVALID_ENUM);
    for (int core = 0; core <= 1; ++core) {
        query_driver_context(0, core);
        if (!core) glBegin(GL_TRIANGLES);
        GLint i = 123;
        GLboolean b = 7;
        GLdouble d = 123;
        GLfloat f = 123;
        GLint64 wide = 123;
        glGetIntegerv(GL_LIST_MODE, &i);
        glGetBooleanv(GL_LIST_MODE, &b);
        glGetDoublev(GL_LIST_MODE, &d);
        glGetFloatv(GL_LIST_MODE, &f);
        glGetInteger64v(GL_LIST_MODE, &wide);
        if (!core) glEnd();
        assert(i == 123 && b == 7 && d == 123 && f == 123 && wide == 123);
        assert(glGetError() == (core ? GL_INVALID_ENUM : GL_INVALID_OPERATION));
    }
    query_driver_context(0, 2);
    GLint64 unsupported = 123;
    glGetInteger64v(GL_LIST_MODE, &unsupported);
    assert(unsupported == 123 && glGetError() == GL_INVALID_ENUM);
    query_driver_context(0, 0);
    mode(0);
    puts("list-mode CPU regression: PASS");
    return 0;
}
