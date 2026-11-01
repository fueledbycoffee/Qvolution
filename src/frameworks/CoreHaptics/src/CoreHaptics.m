#import <CoreHaptics/CoreHaptics.h>
#include <math.h>
/* GUESS (rungs 5-6): opaque identifier literals have no local value source. */
NSString *const CHHapticDynamicParameterIDHapticIntensityControl = @"CHHapticDynamicParameterIDHapticIntensityControl";
NSString *const CHHapticDynamicParameterIDHapticSharpnessControl = @"CHHapticDynamicParameterIDHapticSharpnessControl";
NSString *const CHHapticEventParameterIDHapticIntensity = @"CHHapticEventParameterIDHapticIntensity";
NSString *const CHHapticEventParameterIDHapticSharpness = @"CHHapticEventParameterIDHapticSharpness";
NSString *const CHHapticEventTypeHapticContinuous = @"CHHapticEventTypeHapticContinuous";

@implementation CHHapticDynamicParameter
@synthesize parameterID = _parameterID, value = _value, relativeTime = _relativeTime;
- (instancetype)initWithParameterID:(NSString *)parameterID value:(float)value relativeTime:(NSTimeInterval)time
{
    if (!(self = [super init])) return nil;
    if (![parameterID isKindOfClass:[NSString class]] || !isfinite(value) || !isfinite(time) || time < 0) {
        [self release]; return nil;
    }
    _parameterID = [parameterID copy]; _value = value; _relativeTime = time;
    return self;
}
- (void)dealloc { [_parameterID release]; [super dealloc]; }
@end

@implementation CHHapticEventParameter
@synthesize parameterID = _parameterID, value = _value;
- (instancetype)initWithParameterID:(NSString *)parameterID value:(float)value
{
    if (!(self = [super init])) return nil;
    if (![parameterID isKindOfClass:[NSString class]] || !isfinite(value)) { [self release]; return nil; }
    _parameterID = [parameterID copy]; _value = value;
    return self;
}
- (void)dealloc { [_parameterID release]; [super dealloc]; }
@end

@implementation CHHapticEvent
@synthesize eventType = _eventType, eventParameters = _eventParameters;
@synthesize relativeTime = _relativeTime, duration = _duration;
- (instancetype)initWithEventType:(NSString *)type parameters:(NSArray *)parameters relativeTime:(NSTimeInterval)time
{
    return [self initWithEventType:type parameters:parameters relativeTime:time duration:0];
}
- (instancetype)initWithEventType:(NSString *)type parameters:(NSArray *)parameters relativeTime:(NSTimeInterval)time duration:(NSTimeInterval)duration
{
    if (!(self = [super init])) return nil;
    if (![type isKindOfClass:[NSString class]] || ![parameters isKindOfClass:[NSArray class]] ||
        !isfinite(time) || time < 0 || !isfinite(duration) || duration < 0 ||
        ([type isEqualToString:CHHapticEventTypeHapticContinuous] && (duration <= 0 || duration > 30))) { [self release]; return nil; }
    for (id parameter in parameters) {
        if (![parameter isKindOfClass:[CHHapticEventParameter class]]) { [self release]; return nil; }
    }
    _eventType = [type copy]; _eventParameters = [parameters copy];
    _relativeTime = time; _duration = duration;
    return self;
}
- (void)dealloc { [_eventType release]; [_eventParameters release]; [super dealloc]; }
@end

@implementation CHHapticPattern
@synthesize duration = _duration;
- (instancetype)initWithEvents:(NSArray *)events parameters:(NSArray *)parameters error:(NSError **)error
{
    if (!(self = [super init])) return nil;
    BOOL valid = [events isKindOfClass:[NSArray class]] && [parameters isKindOfClass:[NSArray class]];
    if (valid) for (id event in events) {
        if (![event isKindOfClass:[CHHapticEvent class]]) { valid = NO; break; }
        double end = [event relativeTime] + [event duration];
        if (!isfinite(end) || [event relativeTime] < 0 || [event duration] < 0) { valid = NO; break; }
        if ([[event eventType] isEqualToString:CHHapticEventTypeHapticContinuous] &&
            ([event duration] <= 0 || [event duration] > 30)) { valid = NO; break; }
        if (end > _duration) _duration = end;
    }
    if (valid) for (id parameter in parameters) {
        if (![parameter isKindOfClass:[CHHapticDynamicParameter class]] ||
            !isfinite([parameter relativeTime]) || [parameter relativeTime] < 0) { valid = NO; break; }
    }
    if (!valid) {
        /* GUESS: malformed patterns use Cocoa's invalid-value error. */
        if (error) *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSKeyValueValidationError userInfo:nil];
        [self release]; return nil;
    }
    _events = [events copy]; _parameters = [parameters copy];
    return self;
}
- (void)dealloc { [_events release]; [_parameters release]; [super dealloc]; }
@end
