#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdio.h>
#include <stdlib.h>

static void triangle(float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.75f, -0.75f);
    glVertex2f(0.75f, -0.75f);
    glVertex2f(0.0f, 0.75f);
    glEnd();
}
static void pixel(const char *name, int x, int y) {
    unsigned char p[4] = { 0, 0, 0, 0 };
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    printf("GL.pixel(%s): %u,%u,%u,%u\n", name, p[0], p[1], p[2], p[3]);
}
static int major(void) {
    const char *s = (const char *)glGetString(GL_VERSION);
    return s ? (int)strtol(s, NULL, 10) : 0;
}
int main(void) {
    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAAccelerated, kCGLPFAAllowOfflineRenderers,
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_Legacy,
        kCGLPFAColorSize, 24, kCGLPFAAlphaSize, 8, kCGLPFADepthSize, 24, 0
    };
    CGLPixelFormatObj pf = NULL; CGLContextObj ctx = NULL, shared = NULL, isolated = NULL;
    GLint screens = 0;
    CGLError e = CGLChoosePixelFormat(attrs, &pf, &screens);
    printf("CGL.choose(legacy): %d\n", e == kCGLNoError && pf != NULL && screens > 0);
    if (e || !pf) return 1;
    GLint color = 0, depth = 0;
    int described = CGLDescribePixelFormat(pf, 0, kCGLPFAColorSize, &color) == kCGLNoError;
    described &= CGLDescribePixelFormat(pf, 0, kCGLPFADepthSize, &depth) == kCGLNoError;
    printf("CGL.format.minimums: %d\n", described && color >= 24 && depth >= 24);
    e = CGLCreateContext(pf, NULL, &ctx);
    printf("CGL.create(legacy): %d\n", !e && ctx != NULL);
    if (e || !ctx) { CGLDestroyPixelFormat(pf); return 1; }
    e = CGLSetCurrentContext(ctx);
    printf("CGL.current(legacy): %d\n", !e && CGLGetCurrentContext() == ctx);
    if (e) { CGLDestroyContext(ctx); CGLDestroyPixelFormat(pf); return 1; }
    printf("GL.version.major(legacy): %d\n", major());
    GLint interval = 0, read_interval = -1;
    int parameter = CGLSetParameter(ctx, kCGLCPSwapInterval, &interval) == kCGLNoError;
    parameter &= CGLGetParameter(ctx, kCGLCPSwapInterval, &read_interval) == kCGLNoError;
    printf("CGL.swapInterval.roundTrip: %d\n", parameter && read_interval == 0);

    GLuint tex = 0, fbo = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, tex, 0);
    int complete = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT;
    printf("GL.fbo.complete: %d\n", complete);
    if (!complete) return 1;
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glViewport(0, 0, 64, 64);
    glDisable(GL_DITHER); glDisable(GL_BLEND); glDisable(GL_MULTISAMPLE);
    glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
    GLuint list = glGenLists(1);
    printf("GL.displayList.allocated: %d\n", list != 0);
    glNewList(list, GL_COMPILE); triangle(1, 0, 0); glEndList(); glCallList(list);
    pixel("first-triangle", 32, 24);
    glNewList(list, GL_COMPILE); triangle(0, 1, 0); glEndList();
    glClear(GL_COLOR_BUFFER_BIT); glCallList(list);
    pixel("background", 4, 4); pixel("replacement-triangle", 32, 24); pixel("outside", 8, 56);
    printf("GL.render.errorFree: %d\n", glGetError() == GL_NO_ERROR);

    e = CGLCreateContext(pf, ctx, &shared);
    printf("CGL.create(shared): %d\n", !e && shared != NULL);
    int share_current = !e && CGLSetCurrentContext(shared) == kCGLNoError;
    printf("CGL.current(shared): %d\n", share_current);
    printf("GL.shared.texture: %d\n", share_current && glIsTexture(tex) == GL_TRUE);
    if (share_current) {
        unsigned char p[64 * 64 * 4] = {0};
        glBindTexture(GL_TEXTURE_2D, tex);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
        size_t i = (24 * 64 + 32) * 4;
        printf("GL.shared.pixel: %u,%u,%u,%u\n", p[i], p[i+1], p[i+2], p[i+3]);
    }
    e = CGLCreateContext(pf, NULL, &isolated);
    int independent = !e && CGLSetCurrentContext(isolated) == kCGLNoError;
    printf("GL.independent.unshared: %d\n", independent && glIsTexture(tex) == GL_FALSE);
    CGLSetCurrentContext(ctx);
    glDeleteLists(list, 1); glDeleteFramebuffersEXT(1, &fbo); glDeleteTextures(1, &tex);
    CGLSetCurrentContext(NULL);
    printf("CGL.current(clear): %d\n", CGLGetCurrentContext() == NULL);
    if (isolated) CGLDestroyContext(isolated);
    if (shared) CGLDestroyContext(shared);
    CGLDestroyContext(ctx); CGLDestroyPixelFormat(pf);

    attrs[3] = (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core;
    pf = NULL; ctx = NULL;
    e = CGLChoosePixelFormat(attrs, &pf, &screens);
    printf("CGL.choose(core): %d\n", !e && pf != NULL);
    if (e || !pf) return 1;
    e = CGLCreateContext(pf, NULL, &ctx);
    printf("CGL.create(core): %d\n", !e && ctx != NULL);
    if (e || !ctx) { CGLDestroyPixelFormat(pf); return 1; }
    e = CGLSetCurrentContext(ctx);
    printf("CGL.current(core): %d\n", !e && CGLGetCurrentContext() == ctx);
    if (!e) {
        printf("GL.version.major(core): %d\n", major());
        (void)glGetString(GL_EXTENSIONS);
        printf("GL.core.rejectsLegacyExtensionsQuery: %d\n", glGetError() == GL_INVALID_ENUM);
    }
    CGLSetCurrentContext(NULL); CGLDestroyContext(ctx); CGLDestroyPixelFormat(pf);
    return e ? 1 : 0;
}
