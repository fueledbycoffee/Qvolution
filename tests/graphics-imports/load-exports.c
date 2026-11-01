/* Native dlopen/dlsym check: manifest lines are library-path TAB symbol-name.
 * Run with the runtime native to these own-build libraries. */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
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
        } else { printf("PASS %s\n", symbol); count++; }
    }
    fclose(manifest);
    printf("RESULT load-exports libraries=%d symbols=%d failures=%d\n", libraries, count, failures);
    return failures ? 1 : 0;
}
