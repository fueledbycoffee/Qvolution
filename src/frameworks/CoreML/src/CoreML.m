#import <CoreML/CoreML.h>

/* No model execution backend is available. Never report a prediction as successful. */
static void MLUnsupported(NSError **error) {
    if (error) *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
}
@implementation MLFeatureValue
- (instancetype)init { [self release]; return nil; }
@end
@implementation MLMultiArray
- (instancetype)init { [self release]; return nil; }
@end
@implementation MLModelConfiguration
@end
@implementation MLPredictionOptions
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
