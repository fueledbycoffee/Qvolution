#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

typedef NS_OPTIONS(NSUInteger, QLThumbnailGenerationRequestRepresentationTypes) {
    QLThumbnailGenerationRequestRepresentationTypeIcon = 1 << 0,
    QLThumbnailGenerationRequestRepresentationTypeLowQualityThumbnail = 1 << 1,
    QLThumbnailGenerationRequestRepresentationTypeThumbnail = 1 << 2,
    QLThumbnailGenerationRequestRepresentationTypeAll = ~0UL
};
typedef NS_ENUM(NSInteger, QLThumbnailRepresentationType) {
    QLThumbnailRepresentationTypeIcon = 0,
    QLThumbnailRepresentationTypeLowQualityThumbnail = 1,
    QLThumbnailRepresentationTypeThumbnail = 2
};
@class QLThumbnailRepresentation, QLFileThumbnailRequest;

@interface QLThumbnailGenerationRequest : NSObject <NSCopying> {
    NSURL *_fileURL;
    CGSize _size;
    CGFloat _scale;
    QLThumbnailGenerationRequestRepresentationTypes _representationTypes;
    CGFloat _minimumDimension;
    BOOL _iconMode;
}
- (instancetype)initWithFileAtURL:(NSURL *)url size:(CGSize)size scale:(CGFloat)scale
             representationTypes:(QLThumbnailGenerationRequestRepresentationTypes)types;
@property(readonly, copy) NSURL *fileURL;
@property(readonly) CGSize size;
@property(readonly) CGFloat scale;
@property(readonly) QLThumbnailGenerationRequestRepresentationTypes representationTypes;
@property CGFloat minimumDimension;
@property BOOL iconMode;
@end

@interface QLThumbnailGenerator : NSObject
+ (instancetype)sharedGenerator;
- (void)generateBestRepresentationForRequest:(QLThumbnailGenerationRequest *)request
    completionHandler:(void (^)(QLThumbnailRepresentation *, NSError *))handler;
- (void)generateRepresentationsForRequest:(QLThumbnailGenerationRequest *)request
    updateHandler:(void (^)(QLThumbnailRepresentation *, QLThumbnailRepresentationType, NSError *))handler;
- (void)cancelRequest:(QLThumbnailGenerationRequest *)request;
@end

@interface QLThumbnailReply : NSObject {
    CGSize _contextSize;
    BOOL (^_drawingBlock)(CGContextRef);
    BOOL (^_currentContextDrawingBlock)(void);
    NSURL *_imageFileURL;
}
+ (instancetype)replyWithContextSize:(CGSize)size drawingBlock:(BOOL (^)(CGContextRef))block;
+ (instancetype)replyWithContextSize:(CGSize)size currentContextDrawingBlock:(BOOL (^)(void))block;
+ (instancetype)replyWithImageFileURL:(NSURL *)url;
@end

@interface QLThumbnailProvider : NSObject
- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request
    completionHandler:(void (^)(QLThumbnailReply *, NSError *))handler;
@end
