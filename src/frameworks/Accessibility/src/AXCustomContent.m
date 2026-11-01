/* Rung 3: catalyst/docs-cache/index__accessibility.json, AXCustomContent.
 * A value object; this framework does not claim to deliver announcements.
 */
#import <Accessibility/AXCustomContent.h>

@implementation AXCustomContent
@synthesize importance = _importance;

+ (instancetype) customContentWithLabel: (NSString *) label value: (NSString *) value {
    if (!label || !value)
        return nil;
    NSAttributedString *a = [[NSAttributedString alloc] initWithString: label];
    NSAttributedString *b = [[NSAttributedString alloc] initWithString: value];
    id content = [self customContentWithAttributedLabel: a attributedValue: b];
    [a release];
    [b release];
    return content;
}

+ (instancetype) customContentWithAttributedLabel: (NSAttributedString *) label
                                 attributedValue: (NSAttributedString *) value {
    if (!label || !value)
        return nil;
    AXCustomContent *content = [[[self alloc] init] autorelease];
    if (!content) return nil;
    content->_attributedLabel = [label copy];
    content->_attributedValue = [value copy];
    return content;
}

- (NSString *) label { return [_attributedLabel string]; }
- (NSString *) value { return [_attributedValue string]; }
- (NSAttributedString *) attributedLabel { return _attributedLabel; }
- (NSAttributedString *) attributedValue { return _attributedValue; }

- (void) dealloc {
    [_attributedLabel release];
    [_attributedValue release];
    [super dealloc];
}

- (id) copyWithZone: (NSZone *) zone {
    AXCustomContent *copy = [[[self class] allocWithZone: zone] init];
    if (!copy) return nil;
    copy->_attributedLabel = [_attributedLabel copy];
    copy->_attributedValue = [_attributedValue copy];
    copy->_importance = _importance;
    return copy;
}

// GUESS: Darling-local archive keys. No Apple archive wire format is claimed.
+ (BOOL) supportsSecureCoding { return YES; }
- (void) encodeWithCoder: (NSCoder *) coder {
    [coder encodeObject: _attributedLabel forKey: @"DarlingAXCustomContentAttributedLabel"];
    [coder encodeObject: _attributedValue forKey: @"DarlingAXCustomContentAttributedValue"];
    [coder encodeInteger: _importance forKey: @"DarlingAXCustomContentImportance"];
}
- (instancetype) initWithCoder: (NSCoder *) coder {
    self = [super init];
    if (!self)
        return nil;
    _attributedLabel = [[coder decodeObjectOfClass: [NSAttributedString class]
                                           forKey: @"DarlingAXCustomContentAttributedLabel"] copy];
    _attributedValue = [[coder decodeObjectOfClass: [NSAttributedString class]
                                           forKey: @"DarlingAXCustomContentAttributedValue"] copy];
    _importance = [coder decodeIntegerForKey: @"DarlingAXCustomContentImportance"];
    if (![_attributedLabel isKindOfClass: [NSAttributedString class]] ||
        ![_attributedValue isKindOfClass: [NSAttributedString class]]) {
        [self release];
        return nil;
    }
    return self;
}
@end
