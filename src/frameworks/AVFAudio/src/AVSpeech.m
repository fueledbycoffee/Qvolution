#import <AVFAudio/AVSpeechSynthesizer.h>
#import <AVFAudio/AVAudioRoutingArbiter.h>

/* GUESS (rung 6): public rate defaults, pending a local constant-value probe. */
float const AVSpeechUtteranceDefaultSpeechRate = 0.5f;
float const AVSpeechUtteranceMaximumSpeechRate = 1.0f;

@implementation AVSpeechUtterance
@synthesize speechString = _speechString, voice = _voice;
@synthesize rate = _rate, pitchMultiplier = _pitchMultiplier, volume = _volume;
@synthesize preUtteranceDelay = _preUtteranceDelay, postUtteranceDelay = _postUtteranceDelay;
+ (instancetype)speechUtteranceWithString:(NSString *)string
{
    return [[[self alloc] initWithString:string] autorelease];
}
- (instancetype)init { return [self initWithString:@""]; }
- (instancetype)initWithString:(NSString *)string
{
    if (!(self = [super init])) return nil;
    if (![string isKindOfClass:[NSString class]]) { [self release]; return nil; }
    _speechString = [string copy];
    _rate = AVSpeechUtteranceDefaultSpeechRate;
    _pitchMultiplier = 1.0f;
    _volume = 1.0f;
    return self;
}
- (id)copyWithZone:(NSZone *)zone
{
    AVSpeechUtterance *copy = [[[self class] allocWithZone:zone] initWithString:_speechString];
    copy.voice = _voice; copy.rate = _rate; copy.pitchMultiplier = _pitchMultiplier;
    copy.volume = _volume; copy.preUtteranceDelay = _preUtteranceDelay;
    copy.postUtteranceDelay = _postUtteranceDelay;
    return copy;
}
- (void)dealloc { [_speechString release]; [_voice release]; [super dealloc]; }
@end

@implementation AVSpeechSynthesizer
@synthesize delegate = _delegate;
- (BOOL)isSpeaking { return NO; }
- (BOOL)isPaused { return NO; }
- (void)speakUtterance:(AVSpeechUtterance *)utterance
{
    /* No speech backend: report cancellation, never successful silent speech.
     * GUESS scheduling: enqueue the cancellation on the public main queue. */
    if (!utterance) return;
    id<AVSpeechSynthesizerDelegate> delegate = _delegate;
    if ([delegate respondsToSelector:@selector(speechSynthesizer:didCancelSpeechUtterance:)]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [delegate speechSynthesizer:self didCancelSpeechUtterance:utterance];
        });
    }
}
- (BOOL)pauseSpeakingAtBoundary:(AVSpeechBoundary)boundary { return NO; }
- (BOOL)stopSpeakingAtBoundary:(AVSpeechBoundary)boundary { return NO; }
- (BOOL)continueSpeaking { return NO; }
@end

@implementation AVAudioRoutingArbiter
+ (instancetype)sharedRoutingArbiter
{
    static AVAudioRoutingArbiter *arbiter;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ arbiter = [self new]; });
    return arbiter;
}
- (void)beginArbitrationWithCategory:(AVAudioRoutingArbitrationCategory)category
                 completionHandler:(void (^)(BOOL, NSError *))handler
{
    if (handler) {
        /* GUESS: the host has no routing arbitration; unimpErr is explicit. */
        NSError *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:-4 userInfo:nil];
        dispatch_async(dispatch_get_main_queue(), ^{ handler(NO, error); });
    }
}
- (void)leaveArbitration {}
@end
