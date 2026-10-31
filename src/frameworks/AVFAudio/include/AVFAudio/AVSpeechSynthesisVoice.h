/* SPDX-License-Identifier: GPL-3.0-or-later */
#import <Foundation/Foundation.h>

/* Partial public slice. Scalar metadata and coding require separate specification. */
@interface AVSpeechSynthesisVoice : NSObject {
@private
    NSString *_qvIdentifier,*_qvName,*_qvLanguage;
    NSArray *_qvLanguages,*_qvPriorities;
}
+ (NSArray<AVSpeechSynthesisVoice *> *)speechVoices;
+ (AVSpeechSynthesisVoice *)voiceWithIdentifier:(NSString *)identifier;
+ (AVSpeechSynthesisVoice *)voiceWithLanguage:(NSString *)languageCode;
+ (NSString *)currentLanguageCode;
@property(nonatomic,readonly) NSString *identifier;
@property(nonatomic,readonly) NSString *name;
@property(nonatomic,readonly) NSString *language;
@end
