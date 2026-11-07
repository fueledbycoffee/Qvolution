#ifndef CGL_QUERIES_H
#define CGL_QUERIES_H
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#define CGL_QUERY(function, type) \
    __attribute__((visibility("hidden"))) void cgl_##function(GLenum, type *)
CGL_QUERY(glGetBooleanv, GLboolean);
CGL_QUERY(glGetDoublev, GLdouble);
CGL_QUERY(glGetFloatv, GLfloat);
CGL_QUERY(glGetIntegerv, GLint);
CGL_QUERY(glGetInteger64v, GLint64);
#undef CGL_QUERY
#endif
