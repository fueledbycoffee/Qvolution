#import <MediaPlayer/MediaPlayer.h>

NSString *const MPMediaItemPropertyAlbumTitle = @"MPMediaItemPropertyAlbumTitle";
NSString *const MPMediaItemPropertyAlbumTrackNumber = @"MPMediaItemPropertyAlbumTrackNumber";
NSString *const MPMediaItemPropertyArtist = @"MPMediaItemPropertyArtist";
NSString *const MPMediaItemPropertyArtwork = @"MPMediaItemPropertyArtwork";
NSString *const MPMediaItemPropertyPlaybackDuration = @"MPMediaItemPropertyPlaybackDuration";
NSString *const MPMediaItemPropertyTitle = @"MPMediaItemPropertyTitle";

NSString *const MPNowPlayingInfoPropertyElapsedPlaybackTime = @"MPNowPlayingInfoPropertyElapsedPlaybackTime";
NSString *const MPNowPlayingInfoPropertyMediaType = @"MPNowPlayingInfoPropertyMediaType";
NSString *const MPNowPlayingInfoPropertyPlaybackProgress = @"MPNowPlayingInfoPropertyPlaybackProgress";
NSString *const MPNowPlayingInfoPropertyPlaybackRate = @"MPNowPlayingInfoPropertyPlaybackRate";
NSString *const MPNowPlayingInfoPropertyIsLiveStream = @"MPNowPlayingInfoPropertyIsLiveStream";

@implementation MPMediaItemArtwork

- (instancetype) initWithBoundsSize: (NSSize) boundsSize requestHandler: (NSImage * (^)(NSSize size)) requestHandler {
    self = [super init];
    if (self && requestHandler) {
        _image = [requestHandler(boundsSize) retain];
    }
    return self;
}

- (instancetype) initWithImage: (NSImage *) image {
    self = [super init];
    if (self) {
        _image = [image retain];
    }
    return self;
}

- (void) dealloc {
    [_image release];
    [super dealloc];
}

- (NSImage *) imageWithSize: (NSSize) size {
    return _image;
}

@end

@implementation MPNowPlayingInfoCenter

@synthesize nowPlayingInfo = _nowPlayingInfo;
@synthesize playbackState = _playbackState;

static MPNowPlayingInfoCenter *s_defaultCenter = nil;

+ (MPNowPlayingInfoCenter *) defaultCenter {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s_defaultCenter = [[MPNowPlayingInfoCenter alloc] init];
    });
    return s_defaultCenter;
}

- (void) dealloc {
    [_nowPlayingInfo release];
    [super dealloc];
}

@end

@implementation MPRemoteCommand

@synthesize enabled = _enabled;

- (instancetype) init {
    self = [super init];
    if (self) {
        _enabled = YES;
    }
    return self;
}

- (void) addTarget: (id) target action: (SEL) action {
    [_target release];
    _target = [target retain];
    _action = action;
}

- (void) removeTarget: (id) target action: (SEL) action {
    if (_target == target && _action == action) {
        [_target release];
        _target = nil;
        _action = NULL;
    }
}

- (void) removeTarget: (id) target {
    if (_target == target) {
        [_target release];
        _target = nil;
        _action = NULL;
    }
}

- (void) dealloc {
    [_target release];
    [super dealloc];
}

- (id) addTargetWithHandler: (id) handler {
    return self;
}

- (void) invokeWithEvent: (MPRemoteCommandEvent *) event {
    if (_enabled && _target && _action && [_target respondsToSelector: _action]) {
        [_target performSelector: _action withObject: event];
    }
}

@end

@implementation MPChangePlaybackPositionCommand
@end

@implementation MPSkipIntervalCommand

@synthesize preferredIntervals = _preferredIntervals;

- (void) dealloc {
    [_preferredIntervals release];
    [super dealloc];
}

@end

@implementation MPRemoteCommandEvent

@synthesize command = _command;
@synthesize timestamp = _timestamp;

- (void) dealloc {
    [_command release];
    [super dealloc];
}

@end

@implementation MPChangePlaybackPositionCommandEvent

@synthesize positionTime = _positionTime;

@end

@implementation MPRemoteCommandCenter

@synthesize pauseCommand = _pauseCommand;
@synthesize playCommand = _playCommand;
@synthesize stopCommand = _stopCommand;
@synthesize togglePlayPauseCommand = _togglePlayPauseCommand;
@synthesize nextTrackCommand = _nextTrackCommand;
@synthesize previousTrackCommand = _previousTrackCommand;
@synthesize changePlaybackPositionCommand = _changePlaybackPositionCommand;
@synthesize skipForwardCommand = _skipForwardCommand;
@synthesize skipBackwardCommand = _skipBackwardCommand;
@synthesize seekForwardCommand = _seekForwardCommand;
@synthesize seekBackwardCommand = _seekBackwardCommand;

static MPRemoteCommandCenter *s_sharedCommandCenter = nil;

+ (MPRemoteCommandCenter *) sharedCommandCenter {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s_sharedCommandCenter = [[MPRemoteCommandCenter alloc] init];
    });
    return s_sharedCommandCenter;
}

- (instancetype) init {
    self = [super init];
    if (self) {
        _commands = [NSMutableDictionary new];
        _pauseCommand = [[MPRemoteCommand alloc] init];
        _playCommand = [[MPRemoteCommand alloc] init];
        _stopCommand = [[MPRemoteCommand alloc] init];
        _togglePlayPauseCommand = [[MPRemoteCommand alloc] init];
        _nextTrackCommand = [[MPRemoteCommand alloc] init];
        _previousTrackCommand = [[MPRemoteCommand alloc] init];
        _changePlaybackPositionCommand = [[MPChangePlaybackPositionCommand alloc] init];
        _skipForwardCommand = [[MPSkipIntervalCommand alloc] init];
        _skipBackwardCommand = [[MPSkipIntervalCommand alloc] init];
        _seekForwardCommand = [[MPRemoteCommand alloc] init];
        _seekBackwardCommand = [[MPRemoteCommand alloc] init];
    }
    return self;
}

- (void) dealloc {
    [_commands release];
    [_pauseCommand release];
    [_playCommand release];
    [_stopCommand release];
    [_togglePlayPauseCommand release];
    [_nextTrackCommand release];
    [_previousTrackCommand release];
    [_changePlaybackPositionCommand release];
    [_skipForwardCommand release];
    [_skipBackwardCommand release];
    [_seekForwardCommand release];
    [_seekBackwardCommand release];
    [super dealloc];
}

@end

/* GUESS (rungs 5-6): public key names used as opaque metadata keys. */
NSString *const MPNowPlayingInfoPropertyAssetURL = @"MPNowPlayingInfoPropertyAssetURL";
NSString *const MPNowPlayingInfoPropertyChapterCount = @"MPNowPlayingInfoPropertyChapterCount";
NSString *const MPNowPlayingInfoPropertyChapterNumber = @"MPNowPlayingInfoPropertyChapterNumber";
NSString *const MPNowPlayingInfoPropertyCurrentPlaybackDate = @"MPNowPlayingInfoPropertyCurrentPlaybackDate";
NSString *const MPNowPlayingInfoPropertyDefaultPlaybackRate = @"MPNowPlayingInfoPropertyDefaultPlaybackRate";
NSString *const MPNowPlayingInfoPropertyPlaybackQueueCount = @"MPNowPlayingInfoPropertyPlaybackQueueCount";
NSString *const MPNowPlayingInfoPropertyPlaybackQueueIndex = @"MPNowPlayingInfoPropertyPlaybackQueueIndex";

@implementation MPChangePlaybackRateCommandEvent
@synthesize playbackRate = _playbackRate;
@end

@implementation MPSkipIntervalCommandEvent
@synthesize interval = _interval;
@end
