// Authored public-API observation; renderer-dependent images have no golden policy.
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t pixels[160 * 120 * 4];

static int image(const char *label, int w, int h, int mode) {
    const char *dir = getenv("QV_OUT_DIR");
    char path[4096];
    if (!dir) return 0;
    int n = snprintf(path, sizeof path, "%s/%s-%s.ppm", dir, label, mode == 1 ? "alpha" : "rgb");
    if (n < 0 || (size_t)n >= sizeof path) return 0;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    int ok = fprintf(file, "P6\n%d %d\n255\n", w, h) > 0;
    for (int y = h - 1; ok && y >= 0; --y) for (int x = 0; ok && x < w; ++x) {
        uint8_t *p = pixels + (y * w + x) * 4;
        uint8_t rgb[3];
        for (int c = 0; c < 3; ++c) {
            unsigned v = mode == 1 ? p[3] : mode == 2 ? (p[c] * p[3] + 127) / 255 + 255 - p[3] : p[c];
            rgb[c] = v > 255 ? 255 : v;
        }
        ok = fwrite(rgb, 1, 3, file) == 3;
    }
    return fclose(file) == 0 && ok;
}

static void scene(int variant) {
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        float v = (x + y) % 2 ? .82f : .22f;
        glColor4f(v, v * .85f, v * .65f, 1);
        glBegin(GL_QUADS);
        glVertex2f(x, y); glVertex2f(x + 1, y);
        glVertex2f(x + 1, y + 1); glVertex2f(x, y + 1);
        glEnd();
    }
    if (variant) glColor4f(.1f, .7f, .2f, 1);
    else glColor4f(.85f, .1f, .15f, 1);
    float x = variant ? 2.5f : .5f;
    glBegin(GL_TRIANGLES);
    glVertex2f(x, .3f); glVertex2f(x + .7f, .3f); glVertex2f(x + .35f, 2.6f);
    glEnd();
}

static int render(int accelerated) {
    CGLPixelFormatAttribute attrs[8];
    int i = 0;
    if (accelerated) attrs[i++] = kCGLPFAAccelerated;
    attrs[i++] = kCGLPFAColorSize; attrs[i++] = 24;
    attrs[i++] = kCGLPFAAlphaSize; attrs[i++] = 8; attrs[i++] = 0;
    CGLPixelFormatObj format = NULL;
    CGLContextObj context = NULL;
    GLint count = 0;
    CGLError error = CGLChoosePixelFormat(attrs, &format, &count);
    if (!error) error = CGLCreateContext(format, NULL, &context);
    if (!error) error = CGLSetCurrentContext(context);
    printf("CGL.rasterControl(%d): error=%d format=%d context=%d\n", accelerated, error, format != NULL, context != NULL);
    int ok = !error && context;
    if (!ok) goto done;
    // Capability strings are diagnostic metadata, not portable equality facts.
    printf("## renderer(%d): %s; vendor=%s; version=%s\n", accelerated,
           glGetString(GL_RENDERER), glGetString(GL_VENDOR), glGetString(GL_VERSION));
    GLuint fbo = 0, color = 0, list = 0;
    glGenFramebuffersEXT(1, &fbo); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    glGenRenderbuffersEXT(1, &color); glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, color);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, 160, 120);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, color);
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    printf("OpenGL.rasterControlTarget(%d): status=%u\n", accelerated, status);
    ok = status == GL_FRAMEBUFFER_COMPLETE_EXT;
    if (!ok) goto release;
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glViewport(0, 0, 160, 120);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-.3, 4.3, -.3, 4.3, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DITHER); glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH); glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    list = glGenLists(1);
    if (!list) { ok = 0; goto release; }
    for (int smooth = 0; smooth < 2; ++smooth) {
        if (smooth) glEnable(GL_POLYGON_SMOOTH); else glDisable(GL_POLYGON_SMOOTH);
        for (int pass = 0; pass < 3; ++pass) {
            if (pass < 2) { glNewList(list, GL_COMPILE); scene(pass); glEndList(); }
            glClearColor(.95f, .95f, .95f, 1); glClear(GL_COLOR_BUFFER_BIT);
            glCallList(list); glFinish();
            glReadPixels(0, 0, 160, 120, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            char label[80];
            snprintf(label, sizeof label, "scene-a%d-smooth%d-pass%d", accelerated, smooth, pass);
            GLenum e = glGetError();
            int written = image(label, 160, 120, 2) && image(label, 160, 120, 1);
            printf("OpenGL.rasterControl(%s): enabled=%d error=%u image=%d\n", label,
                   glIsEnabled(GL_POLYGON_SMOOTH), e, written);
            ok &= e == GL_NO_ERROR && written;
        }
    }
    glDeleteLists(list, 1); list = 0;
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, 64, 64);
    glViewport(0, 0, 64, 64);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glDisable(GL_POLYGON_SMOOTH); glDisable(GL_MULTISAMPLE);
    const GLenum hints[] = {GL_DONT_CARE, GL_FASTEST, GL_NICEST};
    for (int hint = 0; hint < 3; ++hint) for (int blend = 0; blend < 3; ++blend) {
        glHint(GL_LINE_SMOOTH_HINT, hints[hint]);
        if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        glBlendFunc(blend == 2 ? GL_SRC_ALPHA_SATURATE : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int smooth = 0; smooth < 2; ++smooth) {
            if (smooth) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH);
            glClearColor(.95f, .95f, .95f, 0); glClear(GL_COLOR_BUFFER_BIT);
            glColor4f(.85f, .1f, .15f, 1);
            glBegin(GL_LINE_LOOP);
            glVertex2f(-.73f, -.71f); glVertex2f(.79f, -.61f); glVertex2f(.21f, .83f);
            glEnd(); glFinish();
            glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            char label[80];
            snprintf(label, sizeof label, "line-a%d-hint%d-blend%d-smooth%d", accelerated, hint, blend, smooth);
            GLenum e = glGetError();
            int written = image(label, 64, 64, 0) && image(label, 64, 64, 1);
            printf("OpenGL.rasterControl(%s): enabled=%d error=%u image=%d\n", label,
                   glIsEnabled(GL_LINE_SMOOTH), e, written);
            ok &= e == GL_NO_ERROR && written;
        }
    }
release:
    if (list) glDeleteLists(list, 1);
    glDeleteRenderbuffersEXT(1, &color); glDeleteFramebuffersEXT(1, &fbo);
    CGLSetCurrentContext(NULL);
done:
    if (context) CGLDestroyContext(context);
    if (format) CGLDestroyPixelFormat(format);
    return ok;
}

int main(void) {
    int ok = render(0);
    ok &= render(1);
    return !ok;
}
