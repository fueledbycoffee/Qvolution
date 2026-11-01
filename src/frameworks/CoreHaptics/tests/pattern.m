#import <CoreHaptics/CoreHaptics.h>
#include <assert.h>
#include <stdio.h>
int main(void)
{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    CHHapticEventParameter *parameter = [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:.5f];
    CHHapticEvent *event = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous parameters:@[parameter] relativeTime:2 duration:3];
    assert(event && event.duration == 3 && event.relativeTime == 2 && [event.eventParameters count] == 1);
    NSError *error = nil;
    CHHapticPattern *pattern = [[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];
    assert(pattern && !error && pattern.duration == 5);
    CHHapticPattern *invalid = [[CHHapticPattern alloc] initWithEvents:@[@"bad"] parameters:@[] error:&error];
    assert(!invalid && error);
    CHHapticEvent *tooLong = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous parameters:@[] relativeTime:0 duration:31];
    assert(!tooLong);
    CHHapticDynamicParameter *dynamic = [[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl value:.5f relativeTime:1];
    assert(dynamic && dynamic.relativeTime == 1 && dynamic.value == .5f);
    [dynamic release]; [pattern release]; [event release]; [parameter release]; [pool drain];
    puts("haptic event data, pattern duration and invalid input errors PASS");
}
