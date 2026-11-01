#import <Foundation/Foundation.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
@implementation NSObject
+ (id)alloc { return class_createInstance(self, 0); }
+ (id)allocWithZone:(NSZone *)zone { return [self alloc]; }
+ (id)new { return [[self alloc] init]; }
+ (Class)class { return self; }
- (id)init { return self; }
- (id)retain { return self; }
- (void)release {} /* Fixture arena lives until process exit. */
- (void)dealloc {}
- (id)autorelease { return self; }
- (id)copy { return [self respondsToSelector:@selector(copyWithZone:)] ? [(id<NSCopying>)self copyWithZone:NULL] : self; }
- (id)copyWithZone:(NSZone *)zone { return self; }
- (Class)class { return object_getClass(self); }
- (BOOL)isKindOfClass:(Class)cls { for (Class c = [self class]; c; c = class_getSuperclass(c)) if (c == cls) return YES; return NO; }
- (BOOL)respondsToSelector:(SEL)s { return class_getInstanceMethod([self class], s) != NULL; }
@end
@implementation NSString
- (const char *)UTF8String { return ""; }
- (BOOL)isEqualToString:(NSString *)s { return s && !strcmp([self UTF8String], [s UTF8String]); }
@end
@implementation NativeConstantString
- (const char *)UTF8String { return bytes; }
@end
@implementation NSArray
+ (id)arrayWithObjects:(const id *)objects count:(NSUInteger)count { NSArray *a = [self new]; assert(count <= 64); a->_count = count; memcpy(a->_objects, objects, count * sizeof(id)); return a; }
- (NSUInteger)count { return _count; }
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)s objects:(id *)objects count:(NSUInteger)n { if (s->state) return 0; s->state = 1; s->itemsPtr = _objects; s->mutationsPtr = &s->extra[0]; return _count; }
@end
@implementation NSDictionary @end
NSString *NSCocoaErrorDomain = @"NSCocoaErrorDomain";
@implementation NSError
+ (id)errorWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)info { NSError *e = [self new]; e->_domain = domain; e->_code = code; return e; }
- (NSString *)domain { return _domain; }
- (NSInteger)code { return _code; }
@end
@implementation NSAutoreleasePool
- (void)drain {}
@end
@implementation NSMethodSignature
+ (id)signatureWithObjCTypes:(const char *)types { return [self new]; }
@end
@implementation NSInvocation
- (SEL)selector { return NULL; }
@end
NSString *NSStringFromSelector(SEL selector) { return @"selector"; }
void NSLog(NSString *format, ...) {}
