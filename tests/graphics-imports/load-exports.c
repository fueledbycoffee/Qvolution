/* Native dlopen/dlsym check: manifest lines are library-path TAB symbol-name.
 * Run with the runtime native to these own-build libraries. */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3) return 2;
    void *supplement = NULL;
    if (argc == 3) {
#ifdef RTLD_FIRST
        supplement = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL | RTLD_FIRST);
        if (!supplement) { fprintf(stderr, "FAIL dlopen Swift supplement: %s\n", dlerror()); return 1; }
#else
        fprintf(stderr, "Swift supplement provider check requires Darwin RTLD_FIRST\n");
        return 2;
#endif
    }
    FILE *manifest = fopen(argv[1], "r");
    if (!manifest) { perror(argv[1]); return 2; }
    char line[8192], current[4096] = "";
    void *handle = NULL;
    int failures = 0, count = 0, libraries = 0;
    while (fgets(line, sizeof(line), manifest)) {
        char *symbol = strchr(line, '\t');
        if (!symbol) return 2;
        *symbol++ = 0;
        symbol[strcspn(symbol, "\r\n")] = 0;
        if (strcmp(current, line)) {
            /* Retain loaded images until exit: Swift conformances and ObjC
             * classes register process-wide state; this checks loading only. */
            snprintf(current, sizeof(current), "%s", line);
            handle = dlopen(current, RTLD_NOW | RTLD_GLOBAL);
            libraries++;
            if (!handle) { fprintf(stderr, "FAIL dlopen %s: %s\n", current, dlerror()); failures++; }
        }
        if (!handle) continue;
        dlerror();
        void *address = dlsym(handle, symbol);
        const char *error = dlerror();
        if (error || !address) {
            fprintf(stderr, "FAIL dlsym %s: %s\n", symbol, error ? error : "NULL");
            failures++;
        } else {
            if (supplement && !strncmp(symbol, "$s", 2) && strstr(current, "/CoreGraphics.framework/")) {
                void *expected = dlsym(supplement, symbol);
                if (!expected || expected != address) {
                    fprintf(stderr, "FAIL Swift supplement provider %s\n", symbol);
                    failures++;
                } else { printf("PASS Swift supplement provider %s\n", symbol); }
            }
            printf("PASS %s\n", symbol); count++;
        }
    }
    fclose(manifest);
    printf("RESULT load-exports libraries=%d symbols=%d failures=%d\n", libraries, count, failures);
    return failures ? 1 : 0;
}
