/* Host ABI boundary adapter is supplied by clock-native.py; production code
 * and the public clock header are compiled unchanged. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <AudioToolbox/CAClock.h>
int main(int argc, char **argv)
{
    assert(argc == 2);
    void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    assert(handle);
#define LOAD(name) __typeof__(&name) name##Ptr = (__typeof__(&name))dlsym(handle, #name); assert(name##Ptr)
    LOAD(CAClockNew); LOAD(CAClockDispose); LOAD(CAClockGetCurrentTime);
    LOAD(CAClockGetStartTime); LOAD(CAClockSetCurrentTime);
    LOAD(CAClockSetProperty); LOAD(CAClockStart); LOAD(CAClockStop);
    CAClockRef clock = (CAClockRef)(uintptr_t)1;
    assert(CAClockNewPtr(0, &clock) == kAudio_UnimplementedError && !clock);
    assert(CAClockNewPtr(0, NULL) == kAudio_UnimplementedError);
    unsigned char output[256], before[256];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
    assert(CAClockGetCurrentTimePtr(NULL, 0, (CAClockTime *)output) == kAudio_UnimplementedError);
    assert(!memcmp(before, output, sizeof(output)));
    assert(CAClockGetStartTimePtr(NULL, 0, (CAClockTime *)output) == kAudio_UnimplementedError);
    assert(!memcmp(before, output, sizeof(output)));
    assert(CAClockSetCurrentTimePtr(NULL, (CAClockTime *)output) == kAudio_UnimplementedError);
    assert(CAClockSetPropertyPtr(NULL, 0, sizeof(output), output) == kAudio_UnimplementedError);
    assert(CAClockStartPtr(NULL) == kAudio_UnimplementedError);
    assert(CAClockStopPtr(NULL) == kAudio_UnimplementedError);
    assert(CAClockDisposePtr(NULL) == kAudio_UnimplementedError);
    assert(!dlclose(handle));
    puts("CAClock: 8 native dlopen exports; unsupported errors and unchanged query storage PASS");
}
