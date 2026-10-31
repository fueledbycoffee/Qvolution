#include <CoreMedia/CMSampleBuffer.h>
#include <CoreMedia/CoreMedia.h>
#include <stddef.h>
_Static_assert(__builtin_types_compatible_p(__typeof__(&kCMTimingInfoInvalid), const CMSampleTimingInfo *), "public const aggregate data type");
_Static_assert(sizeof(CMSampleTimingInfo)==3*sizeof(CMTime), "project contiguous time aggregate ABI");
_Static_assert(offsetof(CMSampleTimingInfo,duration)==0, "duration first");
_Static_assert(offsetof(CMSampleTimingInfo,presentationTimeStamp)==sizeof(CMTime), "presentation second");
_Static_assert(offsetof(CMSampleTimingInfo,decodeTimeStamp)==2*sizeof(CMTime), "decode third");
_Static_assert(_Alignof(CMSampleTimingInfo)==_Alignof(CMTime), "existing natural CMTime alignment policy");
