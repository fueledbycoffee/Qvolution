#include <AudioToolbox/CAClock.h>
/* GUESS (rung 6): legacy clocks are unsupported. unimpErr is -4.
 * Failed queries leave caller storage untouched; creation clears its handle. */
OSStatus CAClockNew(UInt32 reserved, CAClockRef *outClock)
{
    if (outClock) *outClock = 0;
    return kAudio_UnimplementedError;
}
OSStatus CAClockDispose(CAClockRef clock) { return kAudio_UnimplementedError; }
OSStatus CAClockSetProperty(CAClockRef clock, CAClockPropertyID property, UInt32 size, const void *data) { return kAudio_UnimplementedError; }
OSStatus CAClockGetCurrentTime(CAClockRef clock, CAClockTimeFormat format, CAClockTime *outTime) { return kAudio_UnimplementedError; }
OSStatus CAClockGetStartTime(CAClockRef clock, CAClockTimeFormat format, CAClockTime *outTime) { return kAudio_UnimplementedError; }
OSStatus CAClockSetCurrentTime(CAClockRef clock, const CAClockTime *time) { return kAudio_UnimplementedError; }
OSStatus CAClockStart(CAClockRef clock) { return kAudio_UnimplementedError; }
OSStatus CAClockStop(CAClockRef clock) { return kAudio_UnimplementedError; }
