/* Run inside the guest after installation. dlsym deliberately resolves through
 * the Catalyst framework handle rather than linking LaunchServices directly. */
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdio.h>

int main(void) {
    const char *path = "/System/iOSSupport/System/Library/Frameworks/MobileCoreServices.framework/Versions/A/MobileCoreServices";
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    Boolean (*equal)(CFStringRef, CFStringRef) = (Boolean (*)(CFStringRef, CFStringRef))dlsym(handle, "UTTypeEqual");
    CFStringRef (*preferred)(CFStringRef, CFStringRef, CFStringRef) = (CFStringRef (*)(CFStringRef, CFStringRef, CFStringRef))dlsym(handle, "UTTypeCreatePreferredIdentifierForTag");
    const CFStringRef *png = (const CFStringRef *)dlsym(handle, "kUTTypePNG");
    const CFStringRef *extension = (const CFStringRef *)dlsym(handle, "kUTTagClassFilenameExtension");
    if (!equal || !preferred || !png || !extension) { fprintf(stderr, "missing UTI export\n"); return 2; }
    if (!equal(*png, CFSTR("PUBLIC.PNG")) || equal(*png, CFSTR("public.jpeg")) ||
        !CFEqual(*extension, CFSTR("public.filename-extension"))) {
        fprintf(stderr, "incorrect UTI behavior\n"); return 3;
    }
    /* Requires CoreServices' PNG declaration to have been registered. */
    CFStringRef result = preferred(*extension, CFSTR("png"), NULL);
    if (!result || !equal(result, *png)) { fprintf(stderr, "PNG UTI lookup failed (is LaunchServices registered?)\n"); return 4; }
    CFRelease(result);
    dlclose(handle);
    return 0;
}
