#include <assert.h>
#include <dlfcn.h>
#include <objc/runtime.h>
#include <stdio.h>

/* A real two-level link against CoreFoundation, rather than dlsym alone. */
extern char NSMachPortClass __asm__("_OBJC_CLASS_$_NSMachPort");

int main(void)
{
    void *cf = dlopen("/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation", RTLD_NOW);
    void *foundation = dlopen("/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation", RTLD_NOW);
    assert(cf && foundation);
    assert(dlsym(cf, "OBJC_CLASS_$_NSMachPort") == &NSMachPortClass);
    assert(dlsym(foundation, "OBJC_CLASS_$_NSMachPort") == &NSMachPortClass);
    assert((void *)objc_getClass("NSMachPort") == &NSMachPortClass);
    puts("RESULT imp-system-port-binding failures=0");
    return 0;
}
