#pragma once
#import <AVFAudio/AVSpeechUtterance.h>
typedef NS_ENUM(NSInteger, AVSpeechBoundary) {
    AVSpeechBoundaryImmediate = 0, AVSpeechBoundaryWord = 1
};
@class AVSpeechSynthesizer;
@protocol AVSpeechSynthesizerDelegate <NSObject>
@optional
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didCancelSpeechUtterance:(AVSpeechUtterance *)utterance;
@end
@interface AVSpeechSynthesizer : NSObject {
    id<AVSpeechSynthesizerDelegate> _delegate;
}
@property(assign) id<AVSpeechSynthesizerDelegate> delegate;
@property(readonly, getter=isSpeaking) BOOL speaking;
@property(readonly, getter=isPaused) BOOL paused;
- (void)speakUtterance:(AVSpeechUtterance *)utterance;
- (BOOL)pauseSpeakingAtBoundary:(AVSpeechBoundary)boundary;
- (BOOL)stopSpeakingAtBoundary:(AVSpeechBoundary)boundary;
- (BOOL)continueSpeaking;
@end
