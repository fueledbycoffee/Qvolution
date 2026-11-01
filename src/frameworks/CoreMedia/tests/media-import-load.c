/* Load only this lane's Darling-built payloads, never Apple framework binaries.
 * Usage: media-import-load PAYLOAD SYMBOL_MANIFEST (Mach-O nm spellings). */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!handle) { fprintf(stderr, "dlopen %s: %s\n", argv[1], dlerror()); return 1; }
    FILE *file = fopen(argv[2], "r");
    if (!file) { perror(argv[2]); return 2; }
    char symbol[512];
    int count = 0, failed = 0;
    while (fgets(symbol, sizeof(symbol), file)) {
        symbol[strcspn(symbol, "\r\n")] = 0;
        if (!symbol[0]) continue;
        /* dlsym accepts C names, without Mach-O's added leading underscore. */
        const char *name = symbol[0] == '_' ? symbol + 1 : symbol;
        dlerror();
        void *address = dlsym(handle, name);
        const char *error = dlerror();
        if (error || !address) { fprintf(stderr, "missing %s: %s\n", symbol, error ? error : "NULL"); failed = 1; }
        count++;
    }
    if (ferror(file)) failed = 1;
    fclose(file);
    printf("%s: %d exports, %s\n", argv[1], count, failed ? "FAIL" : "PASS");
    /* Keep ObjC-containing images mapped until process exit. */
    return failed;
}
