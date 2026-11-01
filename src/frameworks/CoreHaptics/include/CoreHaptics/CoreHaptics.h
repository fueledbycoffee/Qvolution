#pragma once
#import <Foundation/Foundation.h>
FOUNDATION_EXPORT NSString *const CHHapticDynamicParameterIDHapticIntensityControl;
FOUNDATION_EXPORT NSString *const CHHapticDynamicParameterIDHapticSharpnessControl;
FOUNDATION_EXPORT NSString *const CHHapticEventParameterIDHapticIntensity;
FOUNDATION_EXPORT NSString *const CHHapticEventParameterIDHapticSharpness;
FOUNDATION_EXPORT NSString *const CHHapticEventTypeHapticContinuous;

@interface CHHapticDynamicParameter : NSObject {
    NSString *_parameterID;
    float _value;
    NSTimeInterval _relativeTime;
}
- (instancetype)initWithParameterID:(NSString *)parameterID value:(float)value relativeTime:(NSTimeInterval)time;
@property(readonly, copy) NSString *parameterID;
@property float value;
@property NSTimeInterval relativeTime;
@end

@interface CHHapticEventParameter : NSObject {
    NSString *_parameterID;
    float _value;
}
- (instancetype)initWithParameterID:(NSString *)parameterID value:(float)value;
@property(readonly, copy) NSString *parameterID;
@property float value;
@end

@interface CHHapticEvent : NSObject {
    NSString *_eventType;
    NSArray *_eventParameters;
    NSTimeInterval _relativeTime, _duration;
}
- (instancetype)initWithEventType:(NSString *)type parameters:(NSArray *)parameters relativeTime:(NSTimeInterval)time;
- (instancetype)initWithEventType:(NSString *)type parameters:(NSArray *)parameters relativeTime:(NSTimeInterval)time duration:(NSTimeInterval)duration;
@property(readonly, copy) NSString *eventType;
@property(readonly, copy) NSArray *eventParameters;
@property NSTimeInterval relativeTime;
@property NSTimeInterval duration;
@end

@interface CHHapticPattern : NSObject {
    NSArray *_events, *_parameters;
    NSTimeInterval _duration;
}
- (instancetype)initWithEvents:(NSArray *)events parameters:(NSArray *)parameters error:(NSError **)error;
@property(readonly) NSTimeInterval duration;
@end
