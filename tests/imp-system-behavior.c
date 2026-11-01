#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFURLEnumerator.h>
#include <DiskArbitration/DiskArbitration.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern const CFStringRef NSStringTransformFullwidthToHalfwidth;

static unsigned callbacks;
static void rejected(DADiskRef disk, DADissenterRef dissenter, void *context)
{
    assert(context == &callbacks);
    assert(dissenter);
    assert(DADissenterGetStatus(dissenter) == kDAReturnUnsupported);
    ++callbacks;
}

int main(void)
{
    DADissenterRef dissenter = DADissenterCreate(NULL, kDAReturnBusy, CFSTR("busy"));
    assert(dissenter && DADissenterGetStatus(dissenter) == kDAReturnBusy);
    assert(CFEqual(DADissenterGetStatusString(dissenter), CFSTR("busy")));
    CFRelease(dissenter);
    DASessionRef session = DASessionCreate(NULL);
    DADiskRef disk = DADiskCreateFromBSDName(NULL, session, "test-no-device");
    assert(disk);
    DADiskClaim(disk, kDADiskClaimOptionDefault, NULL, NULL, rejected, &callbacks);
    DADiskUnmount(disk, kDADiskUnmountOptionDefault, rejected, &callbacks);
    DADiskEject(disk, kDADiskEjectOptionDefault, rejected, &callbacks);
    assert(callbacks == 3);
    DADiskUnclaim(disk);
    assert(CFDictionaryGetValue(kDADiskDescriptionMatchVolumeMountable,
        kDADiskDescriptionVolumeMountableKey) == kCFBooleanTrue);
    assert(CFArrayContainsValue(kDADiskDescriptionWatchVolumePath,
        CFRangeMake(0, CFArrayGetCount(kDADiskDescriptionWatchVolumePath)),
        kDADiskDescriptionVolumePathKey));
    CFRelease(disk);
    CFRelease(session);

    CFStringRef key = SCDynamicStoreKeyCreateProxies(NULL);
    assert(key && CFEqual(key, CFSTR("State:/Network/Global/Proxies")));
    CFRelease(key);
    assert(!strcmp(SCErrorString(kSCStatusOK), "Success!"));
    assert(!strcmp(SCErrorString(kSCStatusInvalidArgument), "Invalid argument"));
    assert(SCDynamicStoreCreateRunLoopSource(NULL, NULL, 0) == NULL);

    CFMutableStringRef text = CFStringCreateMutableCopy(NULL, 0, CFSTR("ＡＢＣ"));
    CFRange transformed = CFRangeMake(0, 3);
    assert(CFStringTransform(text, &transformed, NSStringTransformFullwidthToHalfwidth, false));
    assert(CFEqual(text, CFSTR("ABC")));
    CFStringReplaceAll(text, CFSTR("ＡＢＣ"));
    assert(CFStringTransform(text, NULL, NSStringTransformFullwidthToHalfwidth, false));
    assert(CFEqual(text, CFSTR("ABC")));
    CFStringReplaceAll(text, CFSTR("ＸＡＢＹ"));
    transformed = CFRangeMake(1, 2);
    assert(CFStringTransform(text, &transformed, NSStringTransformFullwidthToHalfwidth, false));
    assert(CFEqual(text, CFSTR("ＸABＹ")));
    assert(transformed.location == 1 && transformed.length == 2);
    transformed = CFRangeMake(0, 99);
    assert(!CFStringTransform(text, &transformed, NSStringTransformFullwidthToHalfwidth, false));
    assert(CFEqual(text, CFSTR("ＸABＹ")));
    CFRelease(text);

    CFURLEnumeratorRef enumerator = CFURLEnumeratorCreateForMountedVolumes(NULL, 0, NULL);
    assert(enumerator);
    unsigned count = 0;
    Boolean rootFound = false;
    CFURLRef url = NULL;
    CFErrorRef error = NULL;
    CFURLEnumeratorResult result;
    while ((result = CFURLEnumeratorGetNextURL(enumerator, &url, &error)) == kCFURLEnumeratorSuccess) {
        assert(url && !error);
        CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
        if (CFEqual(path, CFSTR("/"))) rootFound = true;
        CFRelease(path);
        CFRelease(url);
        assert(++count < 10000);
    }
    assert(result == kCFURLEnumeratorEnd && !url && !error);
    assert(count && rootFound);
    CFRelease(enumerator);
    puts("RESULT imp-system-behavior failures=0");
    return 0;
}
