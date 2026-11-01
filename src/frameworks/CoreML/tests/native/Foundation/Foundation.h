/* Controlled native test doubles; not a Foundation compatibility test. */
#pragma once
#include <objc/objc.h>
#include <objc/runtime.h>
#include <stdint.h>
#include <stddef.h>
typedef unsigned long NSUInteger;
typedef long NSInteger;
typedef double NSTimeInterval;
typedef void NSZone;
#define FOUNDATION_EXPORT extern
#define NS_ENUM(t,n) t n; enum
@class NSString, NSArray, NSDictionary, NSURL;
@protocol NSCopying
- (id)copyWithZone:(NSZone *)zone;
@end
__attribute__((objc_root_class))
@interface NSObject { Class isa; }
+ (id)alloc; + (id)allocWithZone:(NSZone *)zone; + (id)new; + (Class)class;
- (id)init; - (id)retain; - (void)release; - (void)dealloc; - (id)autorelease;
- (id)copy; - (Class)class; - (BOOL)isKindOfClass:(Class)cls;
- (BOOL)respondsToSelector:(SEL)selector;
@end
@interface NSString : NSObject
- (BOOL)isEqualToString:(NSString *)other;
- (const char *)UTF8String;
@end
@interface NativeConstantString : NSString { const char *bytes; unsigned int length; }
@end
typedef struct { unsigned long state; id *itemsPtr; unsigned long *mutationsPtr; unsigned long extra[5]; } NSFastEnumerationState;
@interface NSArray : NSObject { id _objects[64]; NSUInteger _count; }
+ (id)arrayWithObjects:(const id *)objects count:(NSUInteger)count;
- (NSUInteger)count;
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(id *)objects count:(NSUInteger)count;
@end
@interface NSDictionary : NSObject
@end
@interface NSError : NSObject { NSString *_domain; NSInteger _code; }
+ (id)errorWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)info;
- (NSString *)domain; - (NSInteger)code;
@end
@interface NSAutoreleasePool : NSObject
- (void)drain;
@end
@interface NSMethodSignature : NSObject
+ (id)signatureWithObjCTypes:(const char *)types;
@end
@interface NSInvocation : NSObject
- (SEL)selector;
@end
extern NSString *NSCocoaErrorDomain;
enum { NSKeyValueValidationError = 1024, NSFeatureUnsupportedError = 3328 };
NSString *NSStringFromSelector(SEL selector);
void NSLog(NSString *format, ...);
