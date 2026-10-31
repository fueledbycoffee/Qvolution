// Original LaunchServices preference contract; no host default or URL-open calls.
#include <CoreFoundation/CoreFoundation.h>
#include <LaunchServices/LaunchServices.h>
#include <CarbonCore/MacErrors.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(c,m) do{if(!(c)){fprintf(stderr,"FAIL: %s\n",m);return 1;}}while(0)
typedef OSStatus (*Setter)(CFStringRef,CFStringRef);
static Setter setDefault;
static CFStringRef arg(const char *s){return CFStringCreateWithCString(NULL,s,kCFStringEncodingUTF8);}
static Boolean equals(CFStringRef value,CFStringRef expected){return value && CFEqual(value,expected);}
struct ThreadResult {unsigned index;int failed;};
static void *writer(void *opaque) {
    struct ThreadResult *result=opaque;
    for(unsigned j=0;j<4;j++) {
        CFStringRef scheme=CFStringCreateWithFormat(NULL,NULL,CFSTR("qvolution-parallel-%ld-%u-%u"),(long)getpid(),result->index,j);
        OSStatus status=setDefault(scheme,CFSTR("org.qvolution.Parallel"));
        CFStringRef value=LSCopyDefaultHandlerForURLScheme(scheme);
        if(status!=noErr || !equals(value,CFSTR("org.qvolution.Parallel")))result->failed=1;
        if(value)CFRelease(value);CFRelease(scheme);
    }
    return NULL;
}
int main(int argc,char **argv) {
    setbuf(stdout,NULL);
    void *library=dlopen("/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/LaunchServices",RTLD_LAZY|RTLD_LOCAL);
    CHECK(library,"actual LaunchServices loads");
    CHECK(dlsym(library,"LSCopyDefaultHandlerForURLScheme")!=NULL,"existing copy-default positive control");
    setDefault=(Setter)dlsym(library,"LSSetDefaultHandlerForURLScheme");
    CHECK(setDefault!=NULL,"documented setter symbol exists");
    if(argc>1) {
        if(argc==2 && !strcmp(argv[1],"--store-path")) {
            printf("/private/var/db/qvolution-url-handlers-%lu/preferences.db\n",(unsigned long)getuid());return 0;
        }
        CHECK(argc>=3,"mode and original test scheme");
        CFStringRef scheme=arg(argv[2]);CHECK(scheme,"scheme argument UTF8");
        if(argc==3 && !strcmp(argv[1],"--expect-none")) {
            CFStringRef value=LSCopyDefaultHandlerForURLScheme(scheme);
            CHECK(value==NULL,"other prefix has no original test preference/capability");
        } else {
            CHECK(argc==4,"handler argument");CFStringRef handler=arg(argv[3]);CHECK(handler,"handler argument UTF8");
            if(!strcmp(argv[1],"--set"))CHECK(setDefault(scheme,handler)==noErr,"committed original preference");
            else if(!strcmp(argv[1],"--expect-write-failure"))CHECK(setDefault(scheme,handler)==kLSDataUnavailableErr,"blocked store reports honest failure");
            else {
                CHECK(!strcmp(argv[1],"--expect"),"known mode");
                CFStringRef value=LSCopyDefaultHandlerForURLScheme(scheme);
                CHECK(equals(value,handler),"separate process observes expected preference/registered fallback");CFRelease(value);
            }
            CFRelease(handler);
        }
        CFRelease(scheme);dlclose(library);puts("PASS: original URL preference mode");return 0;
    }
    CFMutableStringRef scheme=CFStringCreateMutableCopy(NULL,0,CFSTR("Qvolution.Original+Scheme"));
    CFMutableStringRef handler=CFStringCreateMutableCopy(NULL,0,CFSTR("org.qvolution.First"));
    CHECK(setDefault(scheme,handler)==noErr,"first real preference write");
    CFStringReplaceAll(scheme,CFSTR("qvolution-mutated"));CFStringReplaceAll(handler,CFSTR("org.qvolution.Mutated"));
    CFRelease(scheme);CFRelease(handler);
    CFStringRef first=LSCopyDefaultHandlerForURLScheme(CFSTR("qvolution.original+scheme"));
    CHECK(equals(first,CFSTR("org.qvolution.First")),"case-folded scheme and independent input ownership");
    CHECK(setDefault(CFSTR("QVOLUTION.ORIGINAL+SCHEME"),CFSTR("org.qvolution.Second"))==noErr,"replacement committed");
    CFStringRef second=LSCopyDefaultHandlerForURLScheme(CFSTR("qvolution.original+scheme"));
    CHECK(equals(second,CFSTR("org.qvolution.Second")) && equals(first,CFSTR("org.qvolution.First")),"Copy snapshot survives store replacement");
    CFRelease(first);CFRelease(second);
    CHECK(setDefault(NULL,CFSTR("org.qvolution.Fixture"))==paramErr,"nil scheme");
    CHECK(setDefault(CFSTR("qvolution-valid"),NULL)==paramErr,"nil bundle ID");
    const CFStringRef badSchemes[]={CFSTR(""),CFSTR("1bad"),CFSTR("qv:"),CFSTR("qv/a"),CFSTR("qv a"),CFSTR("qv_foo"),CFSTR("é")};
    for(unsigned i=0;i<sizeof(badSchemes)/sizeof(*badSchemes);i++) {
        CHECK(setDefault(badSchemes[i],CFSTR("org.qvolution.Fixture"))==paramErr,"invalid scheme rejected");
        CHECK(LSCopyDefaultHandlerForURLScheme(badSchemes[i])==NULL,"invalid scheme getter rejected");
    }
    const CFStringRef badHandlers[]={CFSTR(""),CFSTR("org.qvolution.Bad ID"),CFSTR("org.qvolution.Bad_ID"),CFSTR("org.qvolution.é")};
    for(unsigned i=0;i<sizeof(badHandlers)/sizeof(*badHandlers);i++)
        CHECK(setDefault(CFSTR("qvolution-valid"),badHandlers[i])==paramErr,"public bundle character set");
    int one=1;CFNumberRef number=CFNumberCreate(NULL,kCFNumberIntType,&one);
    CHECK(setDefault((CFStringRef)number,CFSTR("org.qvolution.Fixture"))==paramErr,"wrong CF scheme type");
    CHECK(setDefault(CFSTR("qvolution-valid"),(CFStringRef)number)==paramErr,"wrong CF handler type");
    CHECK(LSCopyDefaultHandlerForURLScheme((CFStringRef)number)==NULL,"wrong CF getter type");CFRelease(number);
    const UniChar withNUL[]={'q','v',0,'x'};CFStringRef nul=CFStringCreateWithCharacters(NULL,withNUL,4);
    CHECK(setDefault(nul,CFSTR("org.qvolution.Fixture"))==paramErr,"embedded NUL scheme");
    CHECK(setDefault(CFSTR("qvolution-valid"),nul)==paramErr,"embedded NUL bundle ID");CFRelease(nul);
    CFStringRef preserved=LSCopyDefaultHandlerForURLScheme(CFSTR("qvolution.original+scheme"));
    CHECK(equals(preserved,CFSTR("org.qvolution.Second")),"invalid inputs do not change stored preference");CFRelease(preserved);
    struct ThreadResult results[4]={{0,0},{1,0},{2,0},{3,0}};pthread_t threads[4];
    for(unsigned i=0;i<4;i++)CHECK(pthread_create(&threads[i],NULL,writer,&results[i])==0,"writer starts");
    for(unsigned i=0;i<4;i++){CHECK(pthread_join(threads[i],NULL)==0,"writer joins");CHECK(!results[i].failed,"thread-safe independent scheme writes/reads");}
    dlclose(library);puts("PASS: real URL preference storage, input/copy ownership, replacement, validation and concurrent independent keys");return 0;
}
