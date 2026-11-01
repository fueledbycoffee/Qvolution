/* Own-build export/load regression. Never use this on Apple images. */
#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    void *image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!image) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    const char *symbols[] = { "OBJC_CLASS_$_AXCustomContent",
        "OBJC_METACLASS_$_AXCustomContent" };
    int failed = 0;
    for (unsigned i = 0; i < sizeof(symbols)/sizeof(*symbols); ++i) {
        dlerror();
        void *address = dlsym(image, symbols[i]);
        const char *error = dlerror();
        if (!address || error) { fprintf(stderr, "missing %s: %s\n", symbols[i], error ? error : "null"); failed = 1; }
    }
    // Keep Objective-C image metadata alive until process exit.
    if (!failed) puts("PASS dlopen and every added public export");
    return failed;
}
