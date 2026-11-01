#ifndef DARLING_CACLOCK_H
#define DARLING_CACLOCK_H
#include <CoreAudio/CoreAudioTypes.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct OpaqueCAClock *CAClockRef;
typedef UInt32 CAClockPropertyID;
typedef UInt32 CAClockTimeFormat;
/* Opaque until a public layout is vendored; unsupported calls never access it. */
typedef struct CAClockTime CAClockTime;
OSStatus CAClockNew(UInt32 reserved, CAClockRef *outClock);
OSStatus CAClockDispose(CAClockRef clock);
OSStatus CAClockSetProperty(CAClockRef clock, CAClockPropertyID property, UInt32 size, const void *data);
OSStatus CAClockGetCurrentTime(CAClockRef clock, CAClockTimeFormat format, CAClockTime *outTime);
OSStatus CAClockGetStartTime(CAClockRef clock, CAClockTimeFormat format, CAClockTime *outTime);
OSStatus CAClockSetCurrentTime(CAClockRef clock, const CAClockTime *time);
OSStatus CAClockStart(CAClockRef clock);
OSStatus CAClockStop(CAClockRef clock);
#ifdef __cplusplus
}
#endif
#endif
