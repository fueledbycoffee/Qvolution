// Differential check (qv-diff): gluUnProject looked up with dlopen/dlsym on the OpenGL
// framework's libGLU and called through the pointer gives a finite point on every call.
//   qv-diff <lane> tests/wrapgen/resolver-float-args/resolver-float-args.c
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>

typedef int (*unproject_fn)(double, double, double, const double *, const double *, const int *, double *, double *, double *);

int main(void) {
    void *h = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGLU.dylib", RTLD_NOW);
    printf("dlopen(libGLU): %d\n", h != NULL);
    unproject_fn f = h ? (unproject_fn)dlsym(h, "gluUnProject") : NULL;
    printf("dlsym(gluUnProject): %d\n", f != NULL);
    unproject_fn g = h ? (unproject_fn)dlsym(h, "gluUnProject") : NULL;
    printf("dlsym(gluUnProject) again gives the same pointer: %d\n", f == g);
    printf("## first=%p second=%p\n", (void *)f, (void *)g);
    double model[16] = {1,0,0,0, 0,0.5,-0.866,0, 0,0.866,0.5,0, 0,0,-300,1};
    double proj[16] = {6.0,0,0,0, 0,6.5,0,0, 0,0,-1.02,-1, 0,0,-20.2,0};
    int view[4] = {0, 0, 741, 680};
    for (int i = 0; f && i < 3; i++) {
        double x = 0, y = 0, z = 0;
        int r = f(406.8, 225.8, 0.974, model, proj, view, &x, &y, &z);
        printf("gluUnProject via dlsym(call %d): ret=%d finite=%d x=%.3f y=%.3f z=%.3f\n", i + 1, r,
               isfinite(x) && isfinite(y) && isfinite(z), x, y, z);
    }
    return 0;
}
