#import <GameKit/GameKit.h>
#include <dispatch/dispatch.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Block_private.h>

// Use a real BlocksRuntime with the GNU Objective-C runtime. GNU libobjc has
// no block classes, so route only block ownership messages to BlocksRuntime.
extern IMP __real_objc_msg_lookup(id, SEL);
static id copyBlock(id block, SEL selector) { return _Block_copy(block); }
static void releaseBlock(id block, SEL selector) { _Block_release(block); }
IMP __wrap_objc_msg_lookup(id object, SEL selector)
{
    void *kind = object ? *(void **)object : NULL;
    if (kind == _NSConcreteStackBlock || kind == _NSConcreteMallocBlock || kind == _NSConcreteGlobalBlock) {
        const char *name = sel_getName(selector);
        if (!strcmp(name, "copy") || !strcmp(name, "retain")) return (IMP)copyBlock;
        if (!strcmp(name, "release")) return (IMP)releaseBlock;
        abort();
    }
    return __real_objc_msg_lookup(object, selector);
}
static void retainObject(const void *object) { [(id)object retain]; }
static void releaseObject(const void *object) { [(id)object release]; }
static void destructBlock(const void *object) { }

@implementation NSObject
+ (id)alloc { return class_createInstance(self, 0); }
+ (Class)class { return self; }
- (id)init { references = 1; return self; }
- (id)copy { return [self retain]; }
- (id)retain { ++references; return self; }
- (oneway void)release { if (--references == 0) [self dealloc]; }
- (void)dealloc { object_dispose(self); }
@end
@implementation NSString
@end
@interface NSConstantString : NSString
@end
@implementation NSConstantString : NSString
@end
@implementation NSArray
@end
@implementation NSData
@end
@implementation NSDictionary
+ (id)dictionaryWithObjects:(const id *)objects forKeys:(const id *)keys count:(NSUInteger)count { return nil; }
@end
NSString *const NSLocalizedDescriptionKey = @"NSLocalizedDescription";
@implementation NSError
+ (id)errorWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)info
{
    NSError *error = [[self alloc] init];
    error->_domain = domain;
    error->_code = code;
    return error;
}
- (NSString *)domain { return _domain; }
- (NSInteger)code { return _code; }
@end
@implementation NSViewController
@end
@implementation NSWindow
@end

static pthread_t mainThread;
static void (^pending[16])(void);
static unsigned queued;
static int mainQueue, globalQueue;
dispatch_queue_t dispatch_get_main_queue(void) { return &mainQueue; }
dispatch_queue_t dispatch_get_global_queue(long priority, unsigned long flags) { return &globalQueue; }
void dispatch_once(dispatch_once_t *once, void (^block)(void)) { if (!*once) { *once = 1; block(); } }
void dispatch_async(dispatch_queue_t queue, void (^block)(void))
{
    assert(queue == &mainQueue);
    assert(queued < 16);
    pending[queued++] = _Block_copy(block);
}
static void drain(void)
{
    assert(pthread_equal(pthread_self(), mainThread));
    while (queued) {
        void (^block)(void) = pending[0];
        memmove(pending, pending + 1, --queued * sizeof(*pending));
        block();
        _Block_release(block);
    }
}

static unsigned destroyed, legacyCalls, propertyCalls;
@interface Token : NSObject
- (void)check;
@end
@implementation Token
- (void)check { assert(references > 0); }
- (void)dealloc { ++destroyed; [super dealloc]; }
@end
static void checkError(NSError *error)
{
    assert(pthread_equal(pthread_self(), mainThread));
    assert(error != nil);
    assert([error domain] == GKErrorDomain);
    assert([error code] == GKErrorNotSupported);
}
// These stack blocks must survive both the helper's return and caller release
// of captured objects. The property is also cleared before its callback runs.
static void requestLegacy(GKLocalPlayer *player)
{
    Token *token = [[Token alloc] init];
    [player authenticateWithCompletionHandler:^(NSError *error) {
        [token check]; checkError(error); ++legacyCalls;
    }];
    [token release];
}
static void requestProperty(GKLocalPlayer *player)
{
    Token *token = [[Token alloc] init];
    [player setAuthenticateHandler:^(NSViewController *view, NSError *error) {
        assert(view == nil);
        [token check]; checkError(error); ++propertyCalls;
    }];
    [token release];
    [player setAuthenticateHandler:nil];
}
int main(void)
{
    mainThread = pthread_self();
    const Block_callbacks_RR callbacks = { sizeof(callbacks), retainObject, releaseObject, destructBlock };
    _Block_use_RR2(&callbacks);
    GKLocalPlayer *player = [GKLocalPlayer localPlayer];
    assert(class_getInstanceMethod([GKLocalPlayer class], @selector(authenticateWithCompletionHandler:)));
    assert(player == [GKLocalPlayer localPlayer]);
    assert(![player isAuthenticated]);
    [player authenticateWithCompletionHandler:nil];
    [player setAuthenticateHandler:nil];
    assert(queued == 0);
    requestLegacy(player);
    requestProperty(player);
    assert(legacyCalls == 0 && propertyCalls == 0 && destroyed == 0);
    assert(queued == 2);
    drain();
    assert(legacyCalls == 1 && propertyCalls == 1 && destroyed == 2);
    assert([player authenticateHandler] == nil && ![player isAuthenticated]);
    drain();
    assert(legacyCalls == 1 && propertyCalls == 1);
    puts("PASS: authentication callbacks deferred once on main queue; nil and MRC lifetime");
    return 0;
}
