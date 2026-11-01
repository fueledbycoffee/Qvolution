#import <ImageIO/CGImageSource.h>
#import <CoreFoundation/CFArray.h>
#import <Onyx2D/O2ImageSource.h>

const CFStringRef kCGImageSourceCreateThumbnailFromImageAlways = CFSTR("kCGImageSourceCreateThumbnailFromImageAlways");
const CFStringRef kCGImageSourceCreateThumbnailFromImageIfAbsent = CFSTR("kCGImageSourceCreateThumbnailFromImageIfAbsent");
const CFStringRef kCGImageSourceCreateThumbnailWithTransform = CFSTR("kCGImageSourceCreateThumbnailWithTransform");
const CFStringRef kCGImageSourceShouldAllowFloat = CFSTR("kCGImageSourceShouldAllowFloat");
const CFStringRef kCGImageSourceShouldCache = CFSTR("kCGImageSourceShouldCache");
const CFStringRef kCGImageSourceThumbnailMaxPixelSize = CFSTR("kCGImageSourceThumbnailMaxPixelSize");

@interface _O2ImageSource : O2ImageSource
@end

CGImageSourceRef CGImageSourceCreateWithData(CFDataRef data,CFDictionaryRef options) {
	return (CGImageSourceRef)[O2ImageSource newImageSourceWithData:data options:options];
}

CGImageSourceRef CGImageSourceCreateWithURL(CFURLRef url,CFDictionaryRef options) {
	return (CGImageSourceRef)[O2ImageSource newImageSourceWithURL:(NSURL *)url options:options];
}

size_t CGImageSourceGetCount(CGImageSourceRef self) {
   return [self count];
}

CGImageRef CGImageSourceCreateImageAtIndex(CGImageSourceRef self,size_t index,CFDictionaryRef options) {
   return (CGImageRef)[self createImageAtIndex:index options:options];
}

CFDictionaryRef CGImageSourceCopyPropertiesAtIndex(CGImageSourceRef self, size_t index,CFDictionaryRef options) {
   return (CFDictionaryRef)[self copyPropertiesAtIndex:index options:options];
}

CFDictionaryRef CGImageSourceCopyProperties(CGImageSourceRef self, CFDictionaryRef options) {
   if (!self) return NULL;
   if ([self count] > 0) {
      return CGImageSourceCopyPropertiesAtIndex(self, 0, options);
   }
   return NULL;
}

CFStringRef CGImageSourceGetType(CGImageSourceRef self)
{
    return [(O2ImageSource*)self type];
}

CGImageSourceRef CGImageSourceCreateWithDataProvider(CGDataProviderRef provider, CFDictionaryRef options)
{
    printf("STUB %s\n", __PRETTY_FUNCTION__);
    return nil;
}

CFArrayRef CGImageSourceCopyTypeIdentifiers(void)
{
    // Keep this list tied to the decoder classes linked into Onyx2D.
    const void *types[] = { CFSTR("public.png"), CFSTR("public.tiff"),
        CFSTR("public.jpeg"), CFSTR("com.microsoft.bmp"),
        CFSTR("com.compuserve.gif"), CFSTR("com.apple.icns") };
    return CFArrayCreate(NULL, types, sizeof(types) / sizeof(types[0]),
                         &kCFTypeArrayCallBacks);
}

size_t CGImageSourceGetPrimaryImageIndex(CGImageSourceRef source)
{
    // Current decoders have no primary-image metadata (HEIF is unsupported).
    return 0;
}

CGImageRef CGImageSourceCreateThumbnailAtIndex(CGImageSourceRef source,
                                              size_t index,
                                              CFDictionaryRef options)
{
    // No backend thumbnail/orientation pipeline exists. A full-size decoded
    // image would violate max-pixel-size and transform options; report failure.
    return NULL;
}
