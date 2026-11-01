#import <CoreML/CoreML.h>
#include <objc/runtime.h>
#include <assert.h>
#include <stdio.h>
/* Also compile against the pre-fix header for the before/after native probe. */
@interface MLMultiArray (RequiredConstructors)
- (instancetype)initWithShape:(NSArray *)shape dataType:(NSInteger)type error:(NSError **)error;
- (instancetype)initWithDataPointer:(void *)data shape:(NSArray *)shape dataType:(NSInteger)type strides:(NSArray *)strides deallocator:(void (^)(void *))deallocator error:(NSError **)error;
@end
@interface MLFeatureValue (RequiredConstructors)
+ (instancetype)featureValueWithInt64:(int64_t)value;
+ (instancetype)featureValueWithDouble:(double)value;
+ (instancetype)featureValueWithString:(NSString *)value;
+ (instancetype)featureValueWithMultiArray:(MLMultiArray *)value;
+ (instancetype)featureValueWithDictionary:(NSDictionary *)value error:(NSError **)error;
@end
@interface MLModelConfiguration (RequiredProperties)
@property NSInteger computeUnits;
@property BOOL allowLowPrecisionAccumulationOnGPU;
@property(retain) id preferredMetalDevice;
@property(copy) NSDictionary *parameters;
@property(copy) NSString *modelDisplayName;
@property(copy) NSString *functionName;
@end
@interface MLPredictionOptions (RequiredProperties)
@property BOOL usesCPUOnly;
@property(copy) NSDictionary *outputBackings;
@end
static void unsupported(NSError *error)
{
    assert(error && [error.domain isEqualToString:NSCocoaErrorDomain] && error.code == NSFeatureUnsupportedError);
}
int main(void)
{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    if (!class_getInstanceMethod([MLMultiArray class], @selector(initWithShape:dataType:error:))) {
        fputs("missing typed multiarray constructor\n", stderr); return 1;
    }
    NSError *error = nil;
    assert(![[MLMultiArray alloc] initWithShape:@[] dataType:0 error:&error]); unsupported(error);
    assert(![[MLMultiArray alloc] initWithShape:@[] dataType:0 error:NULL]);
    int bytes = 7; __block BOOL freed = NO;
    error = nil;
    assert(![[MLMultiArray alloc] initWithDataPointer:&bytes shape:@[] dataType:0 strides:@[] deallocator:^(void *p) { freed = YES; } error:&error]);
    unsupported(error); assert(!freed && bytes == 7);
    assert(![[MLMultiArray alloc] initWithDataPointer:&bytes shape:@[] dataType:0 strides:@[] deallocator:nil error:NULL]);
    assert(![MLFeatureValue featureValueWithInt64:42]);
    assert(![MLFeatureValue featureValueWithDouble:1.5]);
    assert(![MLFeatureValue featureValueWithString:@"value"]);
    assert(![MLFeatureValue featureValueWithMultiArray:nil]);
    error = nil;
    assert(![MLFeatureValue featureValueWithDictionary:nil error:&error]); unsupported(error);
    assert(![MLFeatureValue featureValueWithDictionary:nil error:NULL]);
    MLModelConfiguration *configuration = [MLModelConfiguration new];
    configuration.computeUnits = 2; assert(configuration.computeUnits == 2);
    configuration.computeUnits = 0; assert(configuration.computeUnits == 0);
    configuration.allowLowPrecisionAccumulationOnGPU = YES; assert(configuration.allowLowPrecisionAccumulationOnGPU);
    configuration.allowLowPrecisionAccumulationOnGPU = NO; assert(!configuration.allowLowPrecisionAccumulationOnGPU);
    id device = [NSObject new]; configuration.preferredMetalDevice = device; assert(configuration.preferredMetalDevice == device);
    NSDictionary *dictionary = [NSDictionary new];
    configuration.parameters = dictionary; assert(configuration.parameters);
    configuration.modelDisplayName = @"model"; assert([configuration.modelDisplayName isEqualToString:@"model"]);
    configuration.functionName = @"main"; assert([configuration.functionName isEqualToString:@"main"]);
    MLPredictionOptions *options = [MLPredictionOptions new];
    options.usesCPUOnly = YES; assert(options.usesCPUOnly);
    options.usesCPUOnly = NO; assert(!options.usesCPUOnly);
    options.outputBackings = dictionary; assert(options.outputBackings);
    configuration.parameters = nil; configuration.preferredMetalDevice = nil;
    configuration.modelDisplayName = nil; configuration.functionName = nil; options.outputBackings = nil;
    assert(!configuration.parameters && !configuration.preferredMetalDevice && !configuration.modelDisplayName && !configuration.functionName && !options.outputBackings);
    error = nil; assert(![MLModel modelWithContentsOfURL:nil error:&error]); unsupported(error);
    assert(![MLModel modelWithContentsOfURL:nil error:NULL]);
    error = nil; assert(![MLModel modelWithContentsOfURL:nil configuration:configuration error:&error]); unsupported(error);
    assert(![MLModel modelWithContentsOfURL:nil configuration:configuration error:NULL]);
    /* alloc intentionally bypasses unavailable init to exercise the error paths. */
    MLModel *model = [MLModel alloc];
    error = nil; assert(![model predictionFromFeatures:nil error:&error]); unsupported(error);
    assert(![model predictionFromFeatures:nil error:NULL]);
    error = nil; assert(![model predictionFromFeatures:nil options:options error:&error]); unsupported(error);
    assert(![model predictionFromFeatures:nil options:options error:NULL]);
    [model release]; [configuration release]; [options release]; [dictionary release]; [device release]; [pool drain];
    puts("PASS: typed CoreML failures, configuration and options round-trip");
}
