/* Use only with the matching private build or its installation. */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s manifest.tsv\n", argv[0]); return 2; }
    FILE *manifest = fopen(argv[1], "r");
    if (!manifest) { perror(argv[1]); return 2; }
    char line[4096], current[2048] = "";
    void *handle = NULL;
    unsigned checked = 0, failures = 0;
    while (fgets(line, sizeof(line), manifest)) {
        char *symbol = strchr(line, '\t');
        if (!symbol) { fprintf(stderr, "malformed manifest\n"); return 2; }
        *symbol++ = 0;
        symbol[strcspn(symbol, "\r\n")] = 0;
        if (strcmp(current, line)) {
            /* Keep dependencies resident for the lifetime of the probe. */
            snprintf(current, sizeof(current), "%s", line);
            handle = dlopen(current, RTLD_NOW | RTLD_LOCAL);
            if (!handle) { fprintf(stderr, "FAIL dlopen %s: %s\n", current, dlerror()); ++failures; }
        }
        if (!handle) continue;
        dlerror();
        void *address = dlsym(handle, symbol);
        const char *error = dlerror();
        if (!address || error) { fprintf(stderr, "FAIL %s: %s\n", symbol, error ? error : "null export"); ++failures; }
        else { printf("PASS %s %s\n", current, symbol); ++checked; }
    }
    if (ferror(manifest)) { perror("manifest read"); ++failures; }
    fclose(manifest);
    printf("RESULT imp-system-load checked=%u failures=%u\n", checked, failures);
    return failures ? 1 : 0;
}
