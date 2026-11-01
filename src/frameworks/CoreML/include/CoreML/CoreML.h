#ifndef _DARLING_COREML_H_
#define _DARLING_COREML_H_
#import <Foundation/Foundation.h>

@interface MLFeatureValue : NSObject
@end
@interface MLMultiArray : NSObject
@end
@interface MLModelConfiguration : NSObject
@end
@interface MLPredictionOptions : NSObject
@end
@interface MLModel : NSObject
+ (instancetype)modelWithContentsOfURL:(NSURL *)url error:(NSError **)error;
+ (instancetype)modelWithContentsOfURL:(NSURL *)url configuration:(MLModelConfiguration *)configuration error:(NSError **)error;
- (id)predictionFromFeatures:(id)features error:(NSError **)error;
- (id)predictionFromFeatures:(id)features options:(MLPredictionOptions *)options error:(NSError **)error;
@end
#endif
