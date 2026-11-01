#ifndef GAMEKIT_TEST_FOUNDATION_H
#define GAMEKIT_TEST_FOUNDATION_H
#include <stdint.h>
#include <stddef.h>
#include <objc/objc.h>
typedef long NSInteger;
typedef unsigned long NSUInteger;
typedef double NSTimeInterval;
#define NS_ENUM(type, name) enum name : type name; enum name : type
__attribute__((objc_root_class))
@interface NSObject {
    Class isa;
    NSUInteger references;
}
+ (id)alloc;
+ (Class)class;
- (id)init;
- (id)copy;
- (id)retain;
- (oneway void)release;
- (void)dealloc;
@end
@interface NSString : NSObject
@end
@interface NSArray<Element> : NSObject
@end
@interface NSData : NSObject
@end
@interface NSDictionary : NSObject
+ (id)dictionaryWithObjects:(const id *)objects forKeys:(const id *)keys count:(NSUInteger)count;
@end
@interface NSError : NSObject {
    NSString *_domain;
    NSInteger _code;
}
+ (id)errorWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)info;
- (NSString *)domain;
- (NSInteger)code;
@end
extern NSString *const NSLocalizedDescriptionKey;
#endif
