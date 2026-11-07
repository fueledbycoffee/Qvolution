// Authored public-API diagnostic: is polygon smoothing effective on an RGBA8 FBO?
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static uint8_t pixels[2][64 * 64 * 4];

static void geometry(int triangle) {
    glColor4f(.85f, .1f, .15f, 1);
    glBegin(triangle ? GL_TRIANGLES : GL_QUADS);
    glVertex2f(-.73f, -.71f); glVertex2f(.79f, -.61f);
    glVertex2f(.21f, .83f);
    if (!triangle) glVertex2f(-.67f, .69f);
    glEnd();
}

static int image(const char *label, const uint8_t *rgba) {
    const char *dir = getenv("QV_OUT_DIR");
    if (!dir) return 0;
    char path[4096];
    int n = snprintf(path, sizeof path, "%s/opengl-polygon-smooth-%s.ppm", dir, label);
    if (n < 0 || n >= sizeof path) return 0;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    int ok = fprintf(file, "P6\n64 64\n255\n") > 0;
    for (int y = 63; ok && y >= 0; --y)
        for (int x = 0; ok && x < 64; ++x)
            ok = fwrite(rgba + (y * 64 + x) * 4, 1, 3, file) == 3;
    return fclose(file) == 0 && ok;
}

int main(void) {
    CGLPixelFormatAttribute attrs[] = {kCGLPFAColorSize, 24, kCGLPFAAlphaSize, 8, 0};
    CGLPixelFormatObj format = NULL;
    CGLContextObj context = NULL;
    GLint count = 0;
    CGLError error = CGLChoosePixelFormat(attrs, &format, &count);
    printf("CGL.choosePixelFormat(smooth): error=%d format=%d\n", error, format != NULL);
    if (error || !format) return 1;
    error = CGLCreateContext(format, NULL, &context);
    printf("CGL.createContext(smooth): error=%d context=%d\n", error, context != NULL);
    if (error || !context) { CGLDestroyPixelFormat(format); return 1; }
    error = CGLSetCurrentContext(context);
    printf("CGL.setCurrentContext(smooth): error=%d\n", error);
    if (error) goto done;
    GLuint framebuffer, renderbuffer;
    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glGenRenderbuffersEXT(1, &renderbuffer);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, 64, 64);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, renderbuffer);
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    printf("OpenGL.framebufferStatus(smooth): %u\n", status);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) { error = kCGLBadDrawable; goto release; }
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glViewport(0, 0, 64, 64);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DITHER); glDisable(GL_DEPTH_TEST); glDisable(GL_MULTISAMPLE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    GLint samples = -1; glGetIntegerv(GL_SAMPLES, &samples);
    printf("OpenGL.samples(smooth): %d\n", samples);
    for (int triangle = 0; triangle < 2; ++triangle) for (int list = 0; list < 2; ++list) {
        for (int smooth = 0; smooth < 2; ++smooth) {
            if (smooth) glEnable(GL_POLYGON_SMOOTH); else glDisable(GL_POLYGON_SMOOTH);
            glClearColor(.95f, .95f, .95f, 1); glClear(GL_COLOR_BUFFER_BIT);
            if (list) {
                GLuint id = glGenLists(1);
                glNewList(id, GL_COMPILE); geometry(triangle); glEndList();
                glCallList(id); glDeleteLists(id, 1);
            } else geometry(triangle);
            glFinish(); glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels[smooth]);
            char label[80];
            snprintf(label, sizeof label, "%s-%s-%s", triangle ? "triangle" : "quad", list ? "list" : "direct", smooth ? "on" : "off");
            printf("OpenGL.polygonSmooth(%s): enabled=%d error=%u image=%d\n", label,
                   glIsEnabled(GL_POLYGON_SMOOTH), glGetError(), image(label, pixels[smooth]));
        }
        unsigned changed = 0;
        for (int i = 0; i < 64 * 64; ++i) {
            int different = 0;
            for (int c = 0; c < 3; ++c)
                if (abs((int)pixels[0][i * 4 + c] - pixels[1][i * 4 + c]) > 8) different = 1;
            changed += different;
        }
        printf("OpenGL.smoothingChangedPixels(%s-%s): %u\n", triangle ? "triangle" : "quad", list ? "list" : "direct", changed);
    }
release:
    glDeleteRenderbuffersEXT(1, &renderbuffer); glDeleteFramebuffersEXT(1, &framebuffer);
    CGLSetCurrentContext(NULL);
done:
    CGLDestroyContext(context); CGLDestroyPixelFormat(format);
    return error != kCGLNoError;
}
