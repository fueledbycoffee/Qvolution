// Original typed data-symbol contract; no sample/codec payload is fabricated.
#include <CoreMedia/CMTime.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#define CHECK(c,m) do{if(!(c)){fprintf(stderr,"FAIL: %s\n",m);return 1;}}while(0)
typedef struct {
 CMTime duration;
 CMTime presentationTimeStamp;
 CMTime decodeTimeStamp;
} ProbeSampleTimingInfo;
_Static_assert(sizeof(CMTime)==24,"existing fixed-width CMTime ABI");
_Static_assert(sizeof(ProbeSampleTimingInfo)==3*sizeof(CMTime),"three contiguous public time fields");
_Static_assert(offsetof(ProbeSampleTimingInfo,presentationTimeStamp)==sizeof(CMTime),"presentation field order");
_Static_assert(offsetof(ProbeSampleTimingInfo,decodeTimeStamp)==2*sizeof(CMTime),"decode field order");
static int equalTime(CMTime a,CMTime b){return a.value==b.value&&a.timescale==b.timescale&&a.flags==b.flags&&a.epoch==b.epoch;}
int main(void){
 void *library=dlopen("/System/Library/Frameworks/CoreMedia.framework/CoreMedia",RTLD_LAZY|RTLD_LOCAL);
 CHECK(library,"actual CoreMedia framework loads");
 const CMTime *invalid=dlsym(library,"kCMTimeInvalid"),*zero=dlsym(library,"kCMTimeZero");
 CHECK(invalid&&zero,"existing typed CMTime constants positive control");
 CHECK(!(invalid->flags&kCMTimeFlags_Valid)&&zero->flags&kCMTimeFlags_Valid,"invalid and valid-zero states distinct");
 CHECK(invalid->value==0&&invalid->timescale==0&&invalid->flags==0&&invalid->epoch==0,"existing canonical invalid model");
 const ProbeSampleTimingInfo *timing=dlsym(library,"kCMTimingInfoInvalid");
 CHECK(timing,"public invalid timing aggregate data export");
 CHECK(equalTime(timing->duration,*invalid)&&equalTime(timing->presentationTimeStamp,*invalid)&&equalTime(timing->decodeTimeStamp,*invalid),"all three public timing fields are canonical invalid");
 ProbeSampleTimingInfo snapshot=*timing,modified=*timing;CMTime invalidCopy=*invalid,zeroCopy=*zero;
 modified.presentationTimeStamp=zeroCopy;
 CHECK(equalTime(modified.presentationTimeStamp,zeroCopy)&&equalTime(snapshot.presentationTimeStamp,invalidCopy)&&equalTime(timing->presentationTimeStamp,invalidCopy),"local valid-zero replacement does not mutate export or independent copy");
 CHECK(dlsym(library,"QvolutionAbsentTimingAggregate")==NULL,"missing-data negative control");
 dlclose(library);
 CHECK(equalTime(snapshot.duration,invalidCopy)&&equalTime(snapshot.presentationTimeStamp,invalidCopy)&&equalTime(snapshot.decodeTimeStamp,invalidCopy),"owned value copy survives library handle lifetime");
 puts("PASS: actual typed invalid timing data and independent value-copy semantics; no decoding qualification");return 0;
}
