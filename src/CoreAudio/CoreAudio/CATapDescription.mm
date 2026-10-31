#import <CoreAudio/CATapDescription.h>

@implementation CATapDescription
@synthesize processes = _processes;
@synthesize bundleIDs = _bundleIDs;
@synthesize name = _name;
@synthesize UUID = _UUID;
@synthesize deviceUID = _deviceUID;
@synthesize stream = _stream;
@synthesize exclusive = _exclusive;
@synthesize mixdown = _mixdown;
@synthesize mono = _mono;
@synthesize privateTap = _privateTap;
@synthesize processRestoreEnabled = _processRestoreEnabled;
@synthesize muteBehavior = _muteBehavior;

- (instancetype)init {
    self = [super init];
    if (self) {
        _processes = [[NSArray alloc] init];
        _bundleIDs = [[NSArray alloc] init];
        _name = [@"" copy];
        _UUID = [[NSUUID alloc] init];
        _muteBehavior = CATapUnmuted;
    }
    return self;
}

- (instancetype)initStereoMixdownOfProcesses:(NSArray<NSNumber *> *)processes {
    self = [self init];
    if (self) {
        self.processes = processes;
        self.mixdown = YES;
    }
    return self;
}

- (instancetype)initMonoMixdownOfProcesses:(NSArray<NSNumber *> *)processes {
    self = [self initStereoMixdownOfProcesses:processes];
    if (self) self.mono = YES;
    return self;
}

- (instancetype)initStereoGlobalTapButExcludeProcesses:(NSArray<NSNumber *> *)processes {
    self = [self initStereoMixdownOfProcesses:processes];
    if (self) self.exclusive = YES;
    return self;
}

- (instancetype)initMonoGlobalTapButExcludeProcesses:(NSArray<NSNumber *> *)processes {
    self = [self initMonoMixdownOfProcesses:processes];
    if (self) self.exclusive = YES;
    return self;
}

- (instancetype)initWithProcesses:(NSArray<NSNumber *> *)processes andDeviceUID:(NSString *)deviceUID withStream:(NSInteger)stream {
    self = [self init];
    if (self) {
        self.processes = processes;
        self.deviceUID = deviceUID;
        self.stream = [NSNumber numberWithInteger:stream];
    }
    return self;
}

- (instancetype)initExcludingProcesses:(NSArray<NSNumber *> *)processes andDeviceUID:(NSString *)deviceUID withStream:(NSInteger)stream {
    self = [self initWithProcesses:processes andDeviceUID:deviceUID withStream:stream];
    if (self) self.exclusive = YES;
    return self;
}

- (void)dealloc {
    [_processes release];
    [_bundleIDs release];
    [_name release];
    [_UUID release];
    [_deviceUID release];
    [_stream release];
    [super dealloc];
}
@end
