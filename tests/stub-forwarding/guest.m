#import <Foundation/Foundation.h>
#include <dlfcn.h>
#import <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "payload-image.h"

struct LargeResult { unsigned long long words[8]; };
/* Deliberately absent selectors with distinct caller ABIs. No Apple behavior is inferred. */
@interface NSObject (StubAuditCalls)
- (id)qvAuditObject;
- (long long)qvAuditInteger;
- (double)qvAuditDouble;
- (struct LargeResult)qvAuditStructure;
- (id)qvAuditObject:(id)value withInteger:(long long)number;
- (id)qvAuditObjectCommand;
- (long long)qvAuditIntegerCommand;
- (struct LargeResult)qvAuditStructureCommand;
- (long long)setqvAuditInteger:(long long)value;
@end
/* A class that really forwards a struct return larger than 16 bytes, which the x86-64 ABI returns
 * through a caller-provided buffer (objc_msgSend_stret), with two arguments after self and _cmd. */
@interface StretForwarder : NSObject
- (struct LargeResult)qvRealWith:(id)value integer:(long long)number;
- (struct LargeResult)qvForwardedWith:(id)value integer:(long long)number;
@end
@implementation StretForwarder
- (struct LargeResult)qvRealWith:(id)value integer:(long long)number { struct LargeResult r = {{0}}; return r; }
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    if (selector == @selector(qvForwardedWith:integer:)) return [super methodSignatureForSelector:@selector(qvRealWith:integer:)];
    return [super methodSignatureForSelector:selector];
}
- (void)forwardInvocation:(NSInvocation *)invocation {
    id value = nil; long long number = 0;
    [invocation getArgument:&value atIndex:2];
    [invocation getArgument:&number atIndex:3];
    struct LargeResult r;
    for (unsigned i = 0; i < 8; i++) r.words[i] = number * 100 + i + (value == (id)self ? 1000 : 0);
    [invocation setReturnValue:&r];
}
@end
static int auditStructForwarding(void) {
    printf("CHECK forwarded struct return\n"); fflush(stdout);
    StretForwarder *forwarder = [[StretForwarder alloc] init];
    struct LargeResult r = [forwarder qvForwardedWith:forwarder integer:7];
    int failures = 0;
    for (unsigned i = 0; i < 8; i++) if (r.words[i] != 1700 + i) failures++;
    if (failures) fprintf(stderr, "FAIL forwarded struct return carried %d wrong words\n", failures);
    [forwarder release];
    return failures ? 1 : 0;
}
static int requirePayloadImage(Class cls, const char *className, const char *expected) {
    const char *mapped = class_getImageName(cls);
    printf("PAYLOAD class=%s expected=%s mapped=%s\n", className, expected, mapped ? mapped : "(none)");
    fflush(stdout);
    if (!qvSamePayloadImage(expected, mapped)) {
        fprintf(stderr, "FAIL payload image mismatch for %s; no selector calls sent\n", className);
        return 0;
    }
    return 1;
}
static int auditUnknownSelectors(id object) {
    int failures = 0;
#define REJECT(expression) do { \
    printf("CHECK %s %s\n", class_getName([object class]), #expression); fflush(stdout); \
    BOOL caught = NO; \
    @try { (void)(expression); } \
    @catch (NSException *exception) { \
        caught = [[exception name] isEqualToString:NSInvalidArgumentException]; \
    } \
    if (!caught) { fprintf(stderr, "FAIL %s: %s\n", class_getName([object class]), #expression); failures++; } \
} while (0)
    REJECT([object qvAuditObject]);
    REJECT([object qvAuditInteger]);
    REJECT([object qvAuditDouble]);
    REJECT([object qvAuditStructure]);
    REJECT([object qvAuditObject:object withInteger:123]);
    REJECT([object qvAuditObjectCommand]);
    REJECT([object qvAuditIntegerCommand]);
    REJECT([object qvAuditStructureCommand]);
    REJECT([object setqvAuditInteger:123]);
#undef REJECT
    return failures;
}
int main(int argc, const char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: stub-forwarding-guest <required-payload-root>\n"); return 2; }
    @autoreleasepool {
        struct { const char *framework; const char *className; } cases[] = {
            { "CoreTelephony", "CTTelephonyNetworkInfo" },
            { "CoreTelephony", "CTCarrier" },
            { "AVFAudio", "AVAudioPlayerNode" },
            { "Automator", "AMAction" },
            { "Automator", "AMWorkflow" },
            { "MediaPlayer", "MPRemoteCommand" },
            { "MediaPlayer", "MPRemoteCommandCenter" },
        };
        const char *payloadRoot = argv[1];
        int failures = 0;
        failures += auditStructForwarding();
        /* Control: an object whose class has no forwarding override at all. */
        printf("CHECK control NSObject\n"); fflush(stdout);
        id control = [[NSObject alloc] init];
        failures += auditUnknownSelectors(control);
        [control release];
        for (unsigned i = 0; i < sizeof(cases)/sizeof(*cases); i++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/System/Library/Frameworks/%s.framework/Versions/A/%s", payloadRoot, cases[i].framework, cases[i].framework);
            if (!dlopen(path, RTLD_NOW | RTLD_GLOBAL)) { fprintf(stderr, "FAIL load %s: %s\n", path, dlerror()); return 1; }
            Class cls = objc_getClass(cases[i].className);
            if (!cls) { fprintf(stderr, "FAIL missing class %s\n", cases[i].className); return 1; }
            if (!requirePayloadImage(cls, cases[i].className, path)) return 3;
            printf("CHECK init %s\n", cases[i].className); fflush(stdout);
            id object = [[cls alloc] init];
            if (!object) { fprintf(stderr, "FAIL init %s\n", cases[i].className); return 1; }
            printf("CHECK instance selectors %s\n", cases[i].className); fflush(stdout);
            failures += auditUnknownSelectors(object);
            printf("CHECK class selectors %s\n", cases[i].className); fflush(stdout);
            failures += auditUnknownSelectors(cls);
            printf("CHECK release %s\n", cases[i].className); fflush(stdout);
            [object release];
        }
        if (failures) return 1;
        puts("PASS guest typed unknown-selector exception paths");
    }
    return 0;
}
