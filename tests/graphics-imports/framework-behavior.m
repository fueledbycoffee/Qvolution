#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <ImageIO/CGImageSource.h>
#import <QuartzCore/CAEDRMetadata.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import <dispatch/dispatch.h>
#include <stdio.h>
#include <math.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
int main(void)
{
    @autoreleasepool {
        CGRect rect = CGRectStandardize(CGRectMake(10, 20, -3, -4));
        CHECK(rect.origin.x == 7 && rect.origin.y == 16 && rect.size.width == 3 && rect.size.height == 4);
        CHECK(CGRectEqualToRect(CGRectStandardize(CGRectNull), CGRectNull));
        CGColorRef color = CGColorCreateGenericRGB(0.2, 0.3, 0.4, 1);
        CHECK(color != NULL);
        if (color) { CHECK(CFGetTypeID(color) == CGColorGetTypeID()); CGColorRelease(color); }
        CHECK(CGColorGetTypeID() != 0);
        CFStringRef typeName = CFCopyTypeIDDescription(CGColorGetTypeID());
        CHECK(typeName && CFEqual(typeName, CFSTR("CGColor")));
        if (typeName) CFRelease(typeName);
        CHECK(CGColorSpaceCreateWithICCData(NULL) == NULL);
        CHECK(CGColorSpaceCreateICCBased(4, NULL, NULL, NULL) == NULL);
        CHECK(CGColorSpaceCopyICCData(NULL) == NULL);
        CHECK(CGColorSpaceCopyPropertyList(NULL) == NULL);
        CHECK(CGDataProviderCreateSequential(NULL, NULL) == NULL);
        CHECK(!CGPreflightScreenCaptureAccess() && !CGRequestScreenCaptureAccess());
        CHECK(CGDisplayCreateImage(0) == NULL);
        CHECK(CGDisplayCreateImageForRect(0, CGRectZero) == NULL);
        CHECK(CGDisplayStreamCreate(0, 0, 0, 0, NULL, NULL) == NULL);
        CHECK(CGDisplayStreamStart(NULL) == kCGErrorIllegalArgument);
        CHECK(CGDisplayStreamStop(NULL) == kCGErrorIllegalArgument);
        size_t count = 123;
        CHECK(CGDisplayStreamUpdateGetRects(NULL, 0, &count) == NULL && count == 0);
        CHECK(CGDisplayStreamUpdateGetRects(NULL, 0, NULL) == NULL);
        CHECK(CTFontManagerCreateFontDescriptorFromData(NULL) == NULL);
        CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithNameAndSize(CFSTR("TestFont"), 12);
        CFStringRef language = CFSTR("unchanged");
        CFTypeRef value = CTFontDescriptorCopyLocalizedAttribute(descriptor, kCTFontNameAttribute, &language);
        CHECK(value && CFEqual(value, CFSTR("TestFont")) && language == NULL);
        if (value) CFRelease(value);
        CFRelease(descriptor);
        CFArrayRef types = CGImageSourceCopyTypeIdentifiers();
        CHECK(types && CFArrayGetCount(types) == 6);
        if (types) CFRelease(types);
        CHECK(CGImageSourceGetPrimaryImageIndex(NULL) == 0);
        CHECK(CGImageSourceCreateThumbnailAtIndex(NULL, 0, NULL) == NULL);
        CHECK(![CAEDRMetadata isAvailable]);
        CAEDRMetadata *metadata = [CAEDRMetadata HLGMetadata];
        CHECK(metadata != nil);
        id metadataCopy = [metadata copy]; CHECK(metadataCopy == metadata); [metadataCopy release];
        CHECK([CAEDRMetadata HDR10MetadataWithMinLuminance:4 maxLuminance:1 opticalOutputScale:1] == nil);
        NSURL *url = [NSURL fileURLWithPath:@"/tmp/no-thumbnail-source"];
        QLThumbnailGenerationRequest *request = [[QLThumbnailGenerationRequest alloc]
            initWithFileAtURL:url size:CGSizeMake(32, 24) scale:2
            representationTypes:QLThumbnailGenerationRequestRepresentationTypeAll];
        CHECK(request != nil && request.scale == 2 && request.size.width == 32);
        request.minimumDimension = 16; request.iconMode = YES;
        QLThumbnailGenerationRequest *copy = [request copy];
        CHECK(copy != request && copy.iconMode && copy.minimumDimension == 16 && [copy.fileURL isEqual:url]);
        [copy release];
        dispatch_semaphore_t ready = dispatch_semaphore_create(0);
        __block BOOL gotError = NO;
        [[QLThumbnailGenerator sharedGenerator] generateBestRepresentationForRequest:request
            completionHandler:^(QLThumbnailRepresentation *representation, NSError *error) {
                gotError = representation == nil && error != nil;
                dispatch_semaphore_signal(ready);
            }];
        CHECK(dispatch_semaphore_wait(ready, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)) == 0);
        CHECK(gotError);
        // Keep the semaphore alive if a failed callback is still pending.
        if (gotError) dispatch_release(ready);
        __block BOOL providerFailed = NO;
        QLThumbnailProvider *provider = [QLThumbnailProvider new];
        [provider provideThumbnailForFileRequest:nil completionHandler:^(QLThumbnailReply *reply, NSError *error) {
            providerFailed = reply == nil && error != nil;
        }];
        CHECK(providerFailed);
        CHECK([QLThumbnailReply replyWithImageFileURL:url] != nil);
        [provider release]; [request release];
    }
    printf("RESULT framework-behavior failures=%d\n", failures);
    return failures ? 1 : 0;
}
