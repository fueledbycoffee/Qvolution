#import <AVFAudio/AVFAudio.h>
#include <assert.h>
#include <stdio.h>
int main(void)
{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    NSMutableString *text = [NSMutableString stringWithString:@"hello"];
    AVSpeechUtterance *utterance = [[AVSpeechUtterance alloc] initWithString:text];
    [text appendString:@" changed"];
    assert([utterance.speechString isEqualToString:@"hello"]);
    assert(utterance.rate == AVSpeechUtteranceDefaultSpeechRate);
    utterance.rate = .7f; utterance.pitchMultiplier = 1.5f; utterance.volume = .2f;
    utterance.preUtteranceDelay = .1; utterance.postUtteranceDelay = .3;
    AVSpeechUtterance *copy = [utterance copy];
    assert(copy != utterance && [copy.speechString isEqualToString:utterance.speechString]);
    assert(copy.rate == utterance.rate && copy.pitchMultiplier == utterance.pitchMultiplier && copy.volume == utterance.volume);
    assert(copy.preUtteranceDelay == utterance.preUtteranceDelay && copy.postUtteranceDelay == utterance.postUtteranceDelay);
    AVSpeechSynthesizer *synth = [AVSpeechSynthesizer new];
    assert(!synth.isSpeaking && !synth.isPaused);
    assert(![synth pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate] && ![synth continueSpeaking]);
    assert(![synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate]);
    assert([AVAudioRoutingArbiter sharedRoutingArbiter] == [AVAudioRoutingArbiter sharedRoutingArbiter]);
    [copy release]; [utterance release]; [synth release]; [pool drain];
    puts("utterance ownership/copying and unavailable speech controls PASS");
}
