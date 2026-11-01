#pragma once
#import <Foundation/Foundation.h>
typedef NS_ENUM(NSInteger, AVAudioRoutingArbitrationCategory) {
    AVAudioRoutingArbitrationCategoryPlayback = 0,
    AVAudioRoutingArbitrationCategoryPlayAndRecord = 1,
    AVAudioRoutingArbitrationCategoryPlayAndRecordVoice = 2
};
@interface AVAudioRoutingArbiter : NSObject
+ (instancetype)sharedRoutingArbiter;
- (void)beginArbitrationWithCategory:(AVAudioRoutingArbitrationCategory)category
                 completionHandler:(void (^)(BOOL deviceChanged, NSError *error))handler;
- (void)leaveArbitration;
@end
