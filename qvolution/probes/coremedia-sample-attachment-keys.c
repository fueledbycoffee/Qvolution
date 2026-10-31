// Original data-symbol contract. No samples, codecs or media files are fabricated.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c,m) do{if(!(c)){fprintf(stderr,"FAIL: %s\n",m);return 1;}}while(0)
static const CFStringRef *symbol(void *library,const char *name) {
    return (const CFStringRef *)dlsym(library,name);
}
int main(void) {
    void *library=dlopen("/System/Library/Frameworks/CoreMedia.framework/CoreMedia",RTLD_LAZY|RTLD_LOCAL);
    CHECK(library,"actual CoreMedia framework loads");
    const CFStringRef *display=symbol(library,"kCMSampleAttachmentKey_DisplayImmediately");
    const CFStringRef *sync=symbol(library,"kCMSampleAttachmentKey_NotSync");
    CHECK(display&&*display&&CFGetTypeID(*display)==CFStringGetTypeID(),"existing DisplayImmediately data positive control");
    CHECK(sync&&*sync&&CFGetTypeID(*sync)==CFStringGetTypeID(),"existing NotSync data positive control");
    const CFStringRef *dependency=symbol(library,"kCMSampleAttachmentKey_IsDependedOnByOthers");
    CHECK(dependency&&*dependency,"observed missing attachment data symbol exists");
    CHECK(CFGetTypeID(*dependency)==CFStringGetTypeID(),"data is actual CFString, not function or null stub");
    CHECK(CFStringGetLength(*dependency)>0&&!CFEqual(*dependency,*sync)&&!CFEqual(*dependency,*display),"distinct nonempty public key");
    // This literal is explicit project policy, not a native macOS observation.
    CFStringRef projectKey=CFStringCreateCopy(NULL,CFSTR("IsDependedOnByOthers"));
    CHECK(projectKey&&CFEqual(*dependency,projectKey),"documented project bare-key value policy");
    CFMutableDictionaryRef attachments=CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CHECK(attachments,"original generic attachment dictionary");
    const void *value=NULL;
    CHECK(!CFDictionaryGetValueIfPresent(attachments,*dependency,&value),"absent dependency remains unknown, not implicit false");
    CFDictionarySetValue(attachments,*dependency,kCFBooleanFalse);
    CHECK(CFDictionaryGetValueIfPresent(attachments,projectKey,&value)&&value==kCFBooleanFalse&&CFGetTypeID(value)==CFBooleanGetTypeID(),"explicit false metadata roundtrip");
    CFDictionaryRef snapshot=CFDictionaryCreateCopy(NULL,attachments);CHECK(snapshot,"owned dictionary snapshot");
    CFDictionarySetValue(attachments,*dependency,kCFBooleanTrue);
    CHECK(CFDictionaryGetValue(attachments,projectKey)==kCFBooleanTrue&&CFDictionaryGetValue(snapshot,projectKey)==kCFBooleanFalse,"true replacement does not mutate copied false metadata");
    CFDictionaryRemoveValue(attachments,*dependency);
    CHECK(!CFDictionaryGetValueIfPresent(attachments,*dependency,&value)&&CFDictionaryGetValue(snapshot,projectKey)==kCFBooleanFalse,"unknown/false/true remain separate and snapshot owns key/value");
    CFRelease(attachments);CHECK(CFDictionaryGetValue(snapshot,projectKey)==kCFBooleanFalse,"snapshot survives source dictionary lifetime");
    CFRelease(snapshot);CFRelease(projectKey);dlclose(library);
    puts("PASS: real CoreMedia CFString attachment data, project literal policy and distinct generic unknown/false/true metadata states; no decoding qualification");return 0;
}
