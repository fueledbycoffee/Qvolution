#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <stdio.h>

static void choose(const char *name, CGLPixelFormatAttribute *attrs) {
    CGLPixelFormatObj pf = NULL;
    GLint count = -1;
    CGLError e = CGLChoosePixelFormat(attrs, &pf, &count);
    printf("CGL.choose(%s).error: %d\n", name, e);
    printf("CGL.choose(%s).format: %d\n", name, pf != NULL);
    if (pf) CGLDestroyPixelFormat(pf);
}
int main(void) {
    const CGLPixelFormatAttribute sizes[] = {
        kCGLPFAColorSize, kCGLPFAAlphaSize, kCGLPFADepthSize, kCGLPFAStencilSize,
        kCGLPFAAccumSize
    };
    const char *names[] = { "color", "alpha", "depth", "stencil", "accum" };
    const CGLPixelFormatAttribute policies[] = { 0, kCGLPFAMinimumPolicy, kCGLPFAMaximumPolicy };
    const char *pnames[] = { "closest", "minimum", "maximum" };
    for (unsigned i = 0; i < 5; ++i) {
        for (unsigned p = 0; p < 3; ++p) {
            CGLPixelFormatAttribute attrs[] = { kCGLPFAAccelerated, sizes[i], 128, policies[p], 0 };
            char name[80];
            snprintf(name, sizeof(name), "%s128.%s", names[i], pnames[p]);
            choose(name, attrs);
        }
    }
    CGLPixelFormatAttribute alpha32[] = { kCGLPFAAccelerated, kCGLPFAAlphaSize, 32, 0, 0 };
    choose("alpha32.closest", alpha32);
    alpha32[3] = kCGLPFAMinimumPolicy;
    choose("alpha32.minimum", alpha32);
    alpha32[3] = kCGLPFAMaximumPolicy;
    choose("alpha32.maximum", alpha32);
    CGLPixelFormatAttribute color32[] = { kCGLPFAAccelerated, kCGLPFAColorSize, 32, kCGLPFAMinimumPolicy, 0 };
    choose("color32.minimum", color32);
    CGLPixelFormatAttribute boundary[] = { kCGLPFAAccelerated, kCGLPFAColorSize, 129, 0 };
    choose("color129.closest", boundary);
    boundary[1] = kCGLPFAAlphaSize; boundary[2] = 33;
    choose("alpha33.closest", boundary);
    CGLPixelFormatAttribute defaults[] = { kCGLPFAAccelerated, 0 };
    CGLPixelFormatObj pf = NULL;
    CGLContextObj ctx = NULL, shared = NULL;
    GLint count = 0;
    CGLError e = CGLChoosePixelFormat(defaults, &pf, &count);
    if (!e) e = CGLCreateContext(pf, NULL, &ctx);
    printf("CGL.setup.error: %d\n", e);
    if (ctx) {
        GLint value = -7;
        printf("CGL.interval.error: %d\n", CGLGetParameter(ctx, kCGLCPSwapInterval, &value));
        printf("CGL.interval.value: %d\n", value);
        printf("CGL.nilFormatShare.error: %d\n", CGLCreateContext(NULL, ctx, &shared));
        printf("CGL.nilFormatShare.context: %d\n", shared != NULL);
        if (shared) CGLDestroyContext(shared);
        CGLDestroyContext(ctx);
    }
    if (pf) CGLDestroyPixelFormat(pf);
    return 0;
}
