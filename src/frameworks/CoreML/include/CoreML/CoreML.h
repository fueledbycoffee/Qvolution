#ifndef _DARLING_COREML_H_
#define _DARLING_COREML_H_
#import <Foundation/Foundation.h>
#include <stdint.h>

/* Numeric enumerators are not specified by the available local public pages. */
typedef NSInteger MLMultiArrayDataType;
typedef NSInteger MLComputeUnits;
@class MLMultiArray;

@interface MLFeatureValue : NSObject
+ (instancetype)featureValueWithInt64:(int64_t)value;
+ (instancetype)featureValueWithDouble:(double)value;
+ (instancetype)featureValueWithString:(NSString *)value;
+ (instancetype)featureValueWithMultiArray:(MLMultiArray *)value;
+ (instancetype)featureValueWithDictionary:(NSDictionary *)value error:(NSError **)error;
@end
@interface MLMultiArray : NSObject
- (instancetype)initWithShape:(NSArray *)shape dataType:(MLMultiArrayDataType)dataType error:(NSError **)error;
- (instancetype)initWithDataPointer:(void *)data shape:(NSArray *)shape dataType:(MLMultiArrayDataType)dataType strides:(NSArray *)strides deallocator:(void (^)(void *))deallocator error:(NSError **)error;
@end
@interface MLModelConfiguration : NSObject {
    MLComputeUnits _computeUnits;
    BOOL _allowLowPrecisionAccumulationOnGPU;
    id _preferredMetalDevice;
    NSDictionary *_parameters;
    NSString *_modelDisplayName, *_functionName;
}
@property MLComputeUnits computeUnits;
@property BOOL allowLowPrecisionAccumulationOnGPU;
@property(retain) id preferredMetalDevice;
@property(copy) NSDictionary *parameters;
@property(copy) NSString *modelDisplayName;
@property(copy) NSString *functionName;
@end
@interface MLPredictionOptions : NSObject {
    BOOL _usesCPUOnly;
    NSDictionary *_outputBackings;
}
@property BOOL usesCPUOnly;
@property(copy) NSDictionary *outputBackings;
@end
@interface MLModel : NSObject
+ (instancetype)modelWithContentsOfURL:(NSURL *)url error:(NSError **)error;
+ (instancetype)modelWithContentsOfURL:(NSURL *)url configuration:(MLModelConfiguration *)configuration error:(NSError **)error;
- (id)predictionFromFeatures:(id)features error:(NSError **)error;
- (id)predictionFromFeatures:(id)features options:(MLPredictionOptions *)options error:(NSError **)error;
@end
#endif
