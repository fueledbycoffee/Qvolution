#pragma once
#import <Foundation/Foundation.h>
#import <AVFAudio/AVSpeechSynthesisVoice.h>
FOUNDATION_EXPORT float const AVSpeechUtteranceDefaultSpeechRate;
FOUNDATION_EXPORT float const AVSpeechUtteranceMaximumSpeechRate;
@interface AVSpeechUtterance : NSObject <NSCopying> {
    NSString *_speechString;
    AVSpeechSynthesisVoice *_voice;
    float _rate, _pitchMultiplier, _volume;
    NSTimeInterval _preUtteranceDelay, _postUtteranceDelay;
}
+ (instancetype)speechUtteranceWithString:(NSString *)string;
- (instancetype)initWithString:(NSString *)string;
@property(readonly, copy) NSString *speechString;
@property(retain) AVSpeechSynthesisVoice *voice;
@property float rate;
@property float pitchMultiplier;
@property float volume;
@property NSTimeInterval preUtteranceDelay;
@property NSTimeInterval postUtteranceDelay;
@end
