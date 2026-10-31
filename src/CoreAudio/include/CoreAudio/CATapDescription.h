#ifndef CATAPDESCRIPTION_H
#define CATAPDESCRIPTION_H

#ifdef __OBJC__
#import <Foundation/NSObject.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSUUID.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, CATapMuteBehavior) {
    CATapUnmuted = 0,
    CATapMuted = 1,
    CATapMutedWhenTapped = 2
};

@interface CATapDescription : NSObject {
@private
    NSArray<NSNumber *> *_processes;
    NSArray<NSString *> *_bundleIDs;
    NSString *_name;
    NSUUID *_UUID;
    NSString *_deviceUID;
    NSNumber *_stream;
    BOOL _exclusive;
    BOOL _mixdown;
    BOOL _mono;
    BOOL _privateTap;
    BOOL _processRestoreEnabled;
    CATapMuteBehavior _muteBehavior;
}
- (instancetype)init;
- (instancetype)initMonoMixdownOfProcesses:(NSArray<NSNumber *> *)processes;
- (instancetype)initStereoMixdownOfProcesses:(NSArray<NSNumber *> *)processes;
- (instancetype)initMonoGlobalTapButExcludeProcesses:(NSArray<NSNumber *> *)processes;
- (instancetype)initStereoGlobalTapButExcludeProcesses:(NSArray<NSNumber *> *)processes;
- (instancetype)initWithProcesses:(NSArray<NSNumber *> *)processes andDeviceUID:(NSString *)deviceUID withStream:(NSInteger)stream;
- (instancetype)initExcludingProcesses:(NSArray<NSNumber *> *)processes andDeviceUID:(NSString *)deviceUID withStream:(NSInteger)stream;

@property (atomic, copy) NSArray<NSNumber *> *processes;
@property (atomic, copy) NSArray<NSString *> *bundleIDs;
@property (atomic, copy) NSString *name;
@property (atomic, copy) NSUUID *UUID;
@property (atomic, copy, nullable) NSString *deviceUID;
@property (atomic, copy, nullable) NSNumber *stream;
@property (atomic, getter=isExclusive) BOOL exclusive;
@property (atomic, getter=isMixdown) BOOL mixdown;
@property (atomic, getter=isMono) BOOL mono;
@property (atomic, getter=isPrivate, setter=setPrivate:) BOOL privateTap;
@property (atomic, getter=isProcessRestoreEnabled) BOOL processRestoreEnabled;
@property (atomic, getter=isMuted) CATapMuteBehavior muteBehavior;
@end

NS_ASSUME_NONNULL_END
#endif
#endif
