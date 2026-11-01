int main(void)
{
    struct __CVBuffer storage = {1920, 1080, NULL};
    CVBufferRef buffer = &storage;
    CFStringRef key = CFSTR("test"), value = CFSTR("value");
    assert(!CVBufferHasAttachment(buffer, key));
    assert(!CVBufferHasAttachment(NULL, key));
    assert(!CVBufferHasAttachment(buffer, NULL));
    CVBufferSetAttachment(buffer, key, value, kCVAttachmentMode_ShouldNotPropagate);
    assert(CVBufferHasAttachment(buffer, key));
    CFStringRef equalKey = CFStringCreateWithCString(NULL, "test", kCFStringEncodingUTF8);
    assert(CVBufferHasAttachment(buffer, equalKey));
    CFRelease(equalKey);
    CFTypeRef copy = CVBufferCopyAttachment(buffer, key, NULL);
    assert(copy && CFEqual(copy, value)); CFRelease(copy);
    CVBufferRemoveAttachment(buffer, key);
    assert(!CVBufferHasAttachment(buffer, key));
    CVBufferSetAttachment(buffer, key, value, kCVAttachmentMode_ShouldPropagate);
    CVBufferRemoveAllAttachments(buffer);
    assert(!CVBufferHasAttachment(buffer, key));
    CGRect rect = CVImageBufferGetCleanRect(buffer);
    assert(rect.origin.x==0 && rect.origin.y==0 && rect.size.width==1920 && rect.size.height==1080);
    rect = CVImageBufferGetCleanRect(NULL);
    assert(rect.size.width==0 && rect.size.height==0);
    CFMutableDictionaryRef aperture = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    double dimensions[] = {1880, 1040, 10, -5};
    const CFStringRef keys[] = {kCVImageBufferCleanApertureWidthKey,kCVImageBufferCleanApertureHeightKey,kCVImageBufferCleanApertureHorizontalOffsetKey,kCVImageBufferCleanApertureVerticalOffsetKey};
    for (int i=0;i<4;++i) {
        CFNumberRef n = CFNumberCreate(NULL,kCFNumberDoubleType,&dimensions[i]);
        CFDictionarySetValue(aperture,keys[i],n); CFRelease(n);
    }
    CVBufferSetAttachment(buffer,kCVImageBufferCleanApertureKey,aperture,1);
    rect=CVImageBufferGetCleanRect(buffer);
    assert(rect.origin.x==30 && rect.origin.y==15 && rect.size.width==1880 && rect.size.height==1040);
    CFDictionarySetValue(aperture,keys[0],CFSTR("bad"));
    rect=CVImageBufferGetCleanRect(buffer); assert(rect.size.width==1920);
    double nan=NAN; CFNumberRef n=CFNumberCreate(NULL,kCFNumberDoubleType,&nan);
    CFDictionarySetValue(aperture,keys[0],n); CFRelease(n);
    rect=CVImageBufferGetCleanRect(buffer); assert(rect.size.width==1920);
    // Policy: incomplete dimensions are malformed, so use the full rectangle.
    CFDictionaryRemoveValue(aperture, keys[0]);
    CFDictionaryRemoveValue(aperture, keys[1]);
    CVBufferSetAttachment(buffer, kCVImageBufferCleanApertureKey, aperture, 1);
    rect = CVImageBufferGetCleanRect(buffer);
    assert(rect.origin.x == 0 && rect.origin.y == 0 && rect.size.width == 1920 && rect.size.height == 1080);
    double height = 1040;
    n = CFNumberCreate(NULL, kCFNumberDoubleType, &height);
    CFDictionarySetValue(aperture, keys[1], n); CFRelease(n);
    rect = CVImageBufferGetCleanRect(buffer);
    assert(rect.origin.x == 0 && rect.origin.y == 0 && rect.size.width == 1920 && rect.size.height == 1080);
    CVBufferRemoveAllAttachments(buffer);
    CFRelease(aperture); CFRelease(storage.attachments);
    assert(!CVIsCompressedPixelFormatAvailable('BGRA'));
    assert(!CVIsCompressedPixelFormatAvailable(0));
    puts("PASS: attachment presence/lifecycle, clean aperture, invalid metadata, compression capability");
}
