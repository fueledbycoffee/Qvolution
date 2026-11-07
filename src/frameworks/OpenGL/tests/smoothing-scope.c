// Authored public-API probe; no expected rasterization policy is assumed.
#define GL_GLEXT_PROTOTYPES 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t pixels[2][64 * 64 * 4];

static void geometry(int line) {
    glColor4f(.85f, .1f, .15f, 1);
    glBegin(line ? GL_LINE_LOOP : GL_TRIANGLES);
    glVertex2f(-.73f, -.71f); glVertex2f(.79f, -.61f); glVertex2f(.21f, .83f);
    glEnd();
}

static void state(int accelerated) {
    GLint query = -1;
    glDisable(GL_POLYGON_SMOOTH);
    glPushAttrib(GL_ENABLE_BIT);
    glEnable(GL_POLYGON_SMOOTH);
    glGetIntegerv(GL_POLYGON_SMOOTH, &query);
    printf("OpenGL.smoothState(accelerated-%d-pushed): enabled=%d query=%d error=%u\n",
           accelerated, glIsEnabled(GL_POLYGON_SMOOTH), query, glGetError());
    glPopAttrib();
    printf("OpenGL.smoothState(accelerated-%d-popped): enabled=%d error=%u\n",
           accelerated, glIsEnabled(GL_POLYGON_SMOOTH), glGetError());
    GLuint list = glGenLists(1);
    if (!list) {
        printf("OpenGL.smoothState(accelerated-%d-list): allocation-failed error=%u\n", accelerated, glGetError());
        return;
    }
    glNewList(list, GL_COMPILE); glEnable(GL_POLYGON_SMOOTH); glEndList();
    printf("OpenGL.smoothState(accelerated-%d-compiled): enabled=%d error=%u\n",
           accelerated, glIsEnabled(GL_POLYGON_SMOOTH), glGetError());
    glCallList(list);
    printf("OpenGL.smoothState(accelerated-%d-replayed): enabled=%d error=%u\n",
           accelerated, glIsEnabled(GL_POLYGON_SMOOTH), glGetError());
    glDeleteLists(list, 1);
    glDisable(GL_POLYGON_SMOOTH);
}

static int target(int accelerated, int texture, int alpha) {
    GLuint framebuffer = 0, color = 0;
    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    if (texture) {
        glGenTextures(1, &color); glBindTexture(GL_TEXTURE_2D, color);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, alpha ? GL_RGBA8 : GL_RGB8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, color, 0);
    } else {
        glGenRenderbuffersEXT(1, &color); glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, color);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, alpha ? GL_RGBA8 : GL_RGB8, 64, 64);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, color);
    }
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    GLint samples = -1;
    glGetIntegerv(GL_SAMPLES, &samples);
    GLenum error = glGetError();
    printf("OpenGL.smoothTarget(accelerated-%d-%s-%s): status=%u samples=%d error=%u\n",
           accelerated, texture ? "texture" : "renderbuffer", alpha ? "rgba" : "rgb", status, samples, error);
    int ok = status == GL_FRAMEBUFFER_COMPLETE_EXT && error == GL_NO_ERROR;
    if (ok) {
        const GLenum hints[] = {GL_DONT_CARE, GL_FASTEST, GL_NICEST};
        const char *hint_names[] = {"default", "fastest", "nicest"};
        const char *blend_names[] = {"disabled", "src-alpha", "saturate"};
        glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
        glViewport(0, 0, 64, 64);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glDisable(GL_DITHER); glDisable(GL_DEPTH_TEST); glDisable(GL_MULTISAMPLE);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        for (int line = 0; line < 2; ++line) for (int hint = 0; hint < 3; ++hint) for (int blend = 0; blend < 3; ++blend) {
            GLenum cap = line ? GL_LINE_SMOOTH : GL_POLYGON_SMOOTH;
            glDisable(GL_LINE_SMOOTH); glDisable(GL_POLYGON_SMOOTH);
            glHint(line ? GL_LINE_SMOOTH_HINT : GL_POLYGON_SMOOTH_HINT, hints[hint]);
            if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            glBlendFunc(blend == 2 ? GL_SRC_ALPHA_SATURATE : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            GLboolean enabled[2];
            GLenum errors[2];
            for (int smooth = 0; smooth < 2; ++smooth) {
                if (smooth) glEnable(cap); else glDisable(cap);
                glClearColor(.95f, .95f, .95f, 0); glClear(GL_COLOR_BUFFER_BIT);
                geometry(line); glFinish();
                glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels[smooth]);
                enabled[smooth] = glIsEnabled(cap);
                errors[smooth] = glGetError();
            }
            unsigned rgb = 0, a = 0;
            for (int i = 0; i < 64 * 64; ++i) {
                int changed = 0;
                for (int c = 0; c < 3; ++c)
                    changed |= abs((int)pixels[0][i * 4 + c] - pixels[1][i * 4 + c]) > 8;
                rgb += changed;
                a += abs((int)pixels[0][i * 4 + 3] - pixels[1][i * 4 + 3]) > 8;
            }
            printf("OpenGL.smoothScope(accelerated-%d-%s-%s-%s-%s-%s): enabled=%d,%d errors=%u,%u rgb=%u alpha=%u\n",
                   accelerated, texture ? "texture" : "renderbuffer", alpha ? "rgba" : "rgb",
                   line ? "line" : "polygon", hint_names[hint], blend_names[blend],
                   enabled[0], enabled[1], errors[0], errors[1], rgb, a);
            ok &= errors[0] == GL_NO_ERROR && errors[1] == GL_NO_ERROR;
        }
    }
    if (texture) glDeleteTextures(1, &color); else glDeleteRenderbuffersEXT(1, &color);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); glDeleteFramebuffersEXT(1, &framebuffer);
    return ok;
}

static int context(int accelerated) {
    CGLPixelFormatAttribute attrs[8];
    int i = 0;
    if (accelerated) attrs[i++] = kCGLPFAAccelerated;
    attrs[i++] = kCGLPFAColorSize; attrs[i++] = 24;
    attrs[i++] = kCGLPFAAlphaSize; attrs[i++] = 8; attrs[i++] = 0;
    CGLPixelFormatObj format = NULL;
    CGLContextObj context = NULL;
    GLint count = 0;
    CGLError error = CGLChoosePixelFormat(attrs, &format, &count);
    printf("CGL.choosePixelFormat(smooth-scope-%d): error=%d format=%d\n", accelerated, error, format != NULL);
    int ok = !error && format;
    if (!ok) goto done;
    error = CGLCreateContext(format, NULL, &context);
    printf("CGL.createContext(smooth-scope-%d): error=%d context=%d\n", accelerated, error, context != NULL);
    ok = !error && context;
    if (!ok) goto done;
    error = CGLSetCurrentContext(context);
    printf("CGL.setCurrentContext(smooth-scope-%d): error=%d\n", accelerated, error);
    ok = error == kCGLNoError;
    if (!ok) goto done;
    state(accelerated);
    for (int texture = 0; texture < 2; ++texture) for (int alpha = 0; alpha < 2; ++alpha)
        ok &= target(accelerated, texture, alpha);
    CGLSetCurrentContext(NULL);
done:
    if (context) CGLDestroyContext(context);
    if (format) CGLDestroyPixelFormat(format);
    return ok;
}

int main(void) {
    int ok = context(0);
    ok &= context(1);
    return !ok;
}
