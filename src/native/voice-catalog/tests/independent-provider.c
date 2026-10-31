#include <espeak-ng/espeak_ng.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

static void hex(const char *s)
{
    putchar('"');
    for (const unsigned char *p=(const unsigned char *)s;*p;p++) printf("%02x",*p);
    putchar('"');
}

int main(void)
{
    void *lib=dlopen("libespeak-ng.so.1",RTLD_NOW|RTLD_LOCAL);
    if (!lib) return 77;
    __typeof__(&espeak_ng_InitializePath) path=dlsym(lib,"espeak_ng_InitializePath");
    __typeof__(&espeak_ng_Initialize) initialize=dlsym(lib,"espeak_ng_Initialize");
    __typeof__(&espeak_ng_ClearErrorContext) clear=dlsym(lib,"espeak_ng_ClearErrorContext");
    __typeof__(&espeak_ng_Terminate) terminate=dlsym(lib,"espeak_ng_Terminate");
    __typeof__(&espeak_ListVoices) list=dlsym(lib,"espeak_ListVoices");
    if (!path||!initialize||!clear||!terminate||!list) return 77;
    espeak_ng_ERROR_CONTEXT error=NULL;
    path(NULL); espeak_ng_STATUS status=initialize(&error); clear(&error);
    if (status!=ENS_OK) return 78;
    const espeak_VOICE **voices=list(NULL);
    if (!voices) return 79;
    putchar('[');
    for (size_t i=0;voices[i];i++) {
        const espeak_VOICE *v=voices[i];
        if (i) putchar(',');
        fputs("{\"identifier\":",stdout);hex(v->identifier);
        fputs(",\"name\":",stdout);hex(v->name);
        printf(",\"gender\":%u,\"languages\":[",v->gender);
        const unsigned char *p=(const unsigned char *)v->languages;
        int first=1;
        while (*p) {
            unsigned int priority=*p++;
            if (!first) putchar(',');
            first=0;
            printf("{\"priority\":%u,\"raw\":",priority);hex((const char *)p);putchar('}');
            while (*p) p++;
            p++;
        }
        fputs("]}",stdout);
    }
    puts("]");
    int ended=terminate(); dlclose(lib);
    return ended==ENS_OK?0:80;
}
