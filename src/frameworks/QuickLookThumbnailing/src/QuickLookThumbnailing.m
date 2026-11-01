#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import <dispatch/dispatch.h>
#include <math.h>

static NSError *unsupportedThumbnail(void)
{
    // No Quick Look extension service or thumbnail renderer is implemented.
    return [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError
        userInfo:@{NSLocalizedDescriptionKey: @"Thumbnail generation is unavailable"}];
}

@implementation QLThumbnailGenerationRequest
@synthesize fileURL = _fileURL, size = _size, scale = _scale;
@synthesize representationTypes = _representationTypes;
@synthesize minimumDimension = _minimumDimension, iconMode = _iconMode;
- (instancetype)initWithFileAtURL:(NSURL *)url size:(CGSize)size scale:(CGFloat)scale
             representationTypes:(QLThumbnailGenerationRequestRepresentationTypes)types
{
    if (!(self = [super init])) return nil;
    if (!url || !isfinite(size.width) || !isfinite(size.height) ||
        !isfinite(scale) || size.width <= 0 || size.height <= 0 || scale <= 0) {
        [self release];
        return nil;
    }
    _fileURL = [url copy]; _size = size; _scale = scale;
    _representationTypes = types;
    return self;
}
- (id)copyWithZone:(NSZone *)zone
{
    QLThumbnailGenerationRequest *copy = [[[self class] allocWithZone:zone]
        initWithFileAtURL:_fileURL size:_size scale:_scale representationTypes:_representationTypes];
    copy.minimumDimension = _minimumDimension;
    copy.iconMode = _iconMode;
    return copy;
}
- (void)dealloc { [_fileURL release]; [super dealloc]; }
@end

@implementation QLThumbnailGenerator
+ (instancetype)sharedGenerator
{
    static QLThumbnailGenerator *generator;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ generator = [self new]; });
    return generator;
}
- (void)generateBestRepresentationForRequest:(QLThumbnailGenerationRequest *)request
    completionHandler:(void (^)(QLThumbnailRepresentation *, NSError *))handler
{
    if (handler) dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @autoreleasepool { handler(nil, unsupportedThumbnail()); }
    });
}
- (void)generateRepresentationsForRequest:(QLThumbnailGenerationRequest *)request
    updateHandler:(void (^)(QLThumbnailRepresentation *, QLThumbnailRepresentationType, NSError *))handler
{
    if (handler) dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @autoreleasepool { handler(nil, QLThumbnailRepresentationTypeThumbnail, unsupportedThumbnail()); }
    });
}
- (void)cancelRequest:(QLThumbnailGenerationRequest *)request
{
    // There is no generation task to cancel; an already queued error may arrive.
}
@end

@implementation QLThumbnailReply
+ (instancetype)replyWithContextSize:(CGSize)size drawingBlock:(BOOL (^)(CGContextRef))block
{
    if (!block || !isfinite(size.width) || !isfinite(size.height) ||
        size.width <= 0 || size.height <= 0) return nil;
    QLThumbnailReply *reply = [[[self alloc] init] autorelease];
    reply->_contextSize = size; reply->_drawingBlock = [block copy];
    return reply;
}
+ (instancetype)replyWithContextSize:(CGSize)size currentContextDrawingBlock:(BOOL (^)(void))block
{
    if (!block || !isfinite(size.width) || !isfinite(size.height) ||
        size.width <= 0 || size.height <= 0) return nil;
    QLThumbnailReply *reply = [[[self alloc] init] autorelease];
    reply->_contextSize = size; reply->_currentContextDrawingBlock = [block copy];
    return reply;
}
+ (instancetype)replyWithImageFileURL:(NSURL *)url
{
    if (!url) return nil;
    QLThumbnailReply *reply = [[[self alloc] init] autorelease];
    reply->_imageFileURL = [url copy];
    return reply;
}
- (void)dealloc
{
    [_drawingBlock release]; [_currentContextDrawingBlock release];
    [_imageFileURL release]; [super dealloc];
}
@end

@implementation QLThumbnailProvider
- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request
    completionHandler:(void (^)(QLThumbnailReply *, NSError *))handler
{
    if (handler) handler(nil, unsupportedThumbnail());
}
@end
