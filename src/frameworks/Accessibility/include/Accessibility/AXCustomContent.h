#import <Foundation/Foundation.h>

// Public API names/types: local Apple Accessibility documentation index.
// GUESS: the index does not publish enum raw values; use default=0, high=1.
typedef NS_ENUM(NSUInteger, AXCustomContentImportance) {
    AXCustomContentImportanceDefault = 0,
    AXCustomContentImportanceHigh = 1,
};

@interface AXCustomContent : NSObject <NSCopying, NSSecureCoding> {
    NSAttributedString *_attributedLabel;
    NSAttributedString *_attributedValue;
    AXCustomContentImportance _importance;
}
+ (instancetype) customContentWithLabel: (NSString *) label value: (NSString *) value;
+ (instancetype) customContentWithAttributedLabel: (NSAttributedString *) label
                                 attributedValue: (NSAttributedString *) value;
@property(readonly, copy) NSString *label;
@property(readonly, copy) NSString *value;
@property(readonly, copy) NSAttributedString *attributedLabel;
@property(readonly, copy) NSAttributedString *attributedValue;
@property AXCustomContentImportance importance;
@end
