#import <CoreML/CoreML.h>

/* No model execution backend is available. Never report a prediction as successful. */
static void MLUnsupported(NSError **error) {
    if (error) *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
}
@implementation MLFeatureValue
- (instancetype)init { [self release]; return nil; }
+ (instancetype)featureValueWithInt64:(int64_t)value { return nil; }
+ (instancetype)featureValueWithDouble:(double)value { return nil; }
+ (instancetype)featureValueWithString:(NSString *)value { return nil; }
+ (instancetype)featureValueWithMultiArray:(MLMultiArray *)value { return nil; }
+ (instancetype)featureValueWithDictionary:(NSDictionary *)value error:(NSError **)error {
    MLUnsupported(error); return nil;
}
@end
@implementation MLMultiArray
- (instancetype)init { [self release]; return nil; }
- (instancetype)initWithShape:(NSArray *)shape dataType:(MLMultiArrayDataType)dataType error:(NSError **)error {
    MLUnsupported(error); [self release]; return nil;
}
- (instancetype)initWithDataPointer:(void *)data shape:(NSArray *)shape dataType:(MLMultiArrayDataType)dataType strides:(NSArray *)strides deallocator:(void (^)(void *))deallocator error:(NSError **)error {
    /* Failure does not take ownership of the caller's memory. */
    MLUnsupported(error); [self release]; return nil;
}
@end
@implementation MLModelConfiguration
@synthesize computeUnits = _computeUnits, allowLowPrecisionAccumulationOnGPU = _allowLowPrecisionAccumulationOnGPU;
@synthesize preferredMetalDevice = _preferredMetalDevice, parameters = _parameters;
@synthesize modelDisplayName = _modelDisplayName, functionName = _functionName;
/* GUESS: zero/nil defaults until member-level defaults are documented locally. */
- (void)dealloc {
    [_preferredMetalDevice release]; [_parameters release];
    [_modelDisplayName release]; [_functionName release]; [super dealloc];
}
@end
@implementation MLPredictionOptions
@synthesize usesCPUOnly = _usesCPUOnly, outputBackings = _outputBackings;
- (void)dealloc { [_outputBackings release]; [super dealloc]; }
@end
@implementation MLModel
- (instancetype)init { [self release]; return nil; }
+ (instancetype)modelWithContentsOfURL:(NSURL *)url error:(NSError **)error {
    MLUnsupported(error); return nil;
}
+ (instancetype)modelWithContentsOfURL:(NSURL *)url configuration:(MLModelConfiguration *)configuration error:(NSError **)error {
    MLUnsupported(error); return nil;
}
- (id)predictionFromFeatures:(id)features error:(NSError **)error {
    MLUnsupported(error); return nil;
}
- (id)predictionFromFeatures:(id)features options:(MLPredictionOptions *)options error:(NSError **)error {
    MLUnsupported(error); return nil;
}
@end
