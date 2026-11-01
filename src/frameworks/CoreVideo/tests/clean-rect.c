#include <CoreVideo/CoreVideo.h>
#include <CoreGraphics/CGGeometry.h>
#include <assert.h>
#include <stdio.h>
static void number(CFMutableDictionaryRef dict, CFStringRef key, double value)
{
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberDoubleType, &value);
    CFDictionarySetValue(dict, key, n); CFRelease(n);
}
static void rect(CGRect r, double x, double y, double w, double h)
{
    assert(r.origin.x == x && r.origin.y == y && r.size.width == w && r.size.height == h);
}
int main(void)
{
    CVPixelBufferRef buffer = NULL;
    assert(CVPixelBufferCreate(NULL, 640, 480, kCVPixelFormatType_32BGRA, NULL, &buffer) == 0 && buffer);
    rect(CVImageBufferGetCleanRect(buffer), 0, 0, 640, 480);
    rect(CVImageBufferGetCleanRect(NULL), 0, 0, 0, 0);
    CFMutableDictionaryRef aperture = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    number(aperture, kCVImageBufferCleanApertureWidthKey, 320);
    number(aperture, kCVImageBufferCleanApertureHeightKey, 200);
    number(aperture, kCVImageBufferCleanApertureHorizontalOffsetKey, 20);
    number(aperture, kCVImageBufferCleanApertureVerticalOffsetKey, -10);
    CVBufferSetAttachment(buffer, kCVImageBufferCleanApertureKey, aperture, kCVAttachmentMode_ShouldPropagate);
    rect(CVImageBufferGetCleanRect(buffer), 180, 130, 320, 200);
    number(aperture, kCVImageBufferCleanApertureWidthKey, 1000);
    rect(CVImageBufferGetCleanRect(buffer), 0, 0, 640, 480);
    CVBufferSetAttachment(buffer, kCVImageBufferCleanApertureKey, CFSTR("bad"), kCVAttachmentMode_ShouldPropagate);
    rect(CVImageBufferGetCleanRect(buffer), 0, 0, 640, 480);
    CVPixelBufferPoolRef pool = NULL;
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    number(attrs, kCVPixelBufferWidthKey, 640); number(attrs, kCVPixelBufferHeightKey, 480);
    number(attrs, kCVPixelBufferPixelFormatTypeKey, kCVPixelFormatType_32BGRA);
    assert(CVPixelBufferPoolCreate(NULL, NULL, attrs, &pool) == 0 && pool);
    CVPixelBufferRef pooled = NULL;
    assert(CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &pooled) == 0 && pooled);
    void *bytes = CVPixelBufferGetBaseAddress(pooled);
    ((unsigned char *)bytes)[0] = 0x7b;
    CVPixelBufferPoolFlush(pool, kCVPixelBufferPoolFlushExcessBuffers);
    assert(CVPixelBufferGetBaseAddress(pooled) == bytes && ((unsigned char *)bytes)[0] == 0x7b);
    CVPixelBufferRelease(pooled); CFRelease(attrs);
    CVPixelBufferPoolRelease(pool); CFRelease(aperture); CVPixelBufferRelease(buffer);
    puts("clean aperture, invalid metadata fallback and flush ownership PASS");
}
