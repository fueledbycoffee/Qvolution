#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <stdio.h>

int main(void) {
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
