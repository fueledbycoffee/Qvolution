// Framework integration check; native-achievement-report.py isolates the same source.
// Build with -fblocks -fno-objc-arc -framework Foundation -framework GameKit.
// Default: assert Darling's documented failure policy. --platform-errors prints
// the host's results without assuming that macOS uses Darling's error policy.
#import <Foundation/Foundation.h>
#import <GameKit/GameKit.h>
#include <dispatch/dispatch.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static NSLock *lock;
static int calls, errors, wrongError, wrongThread, inlineCalls;
static BOOL workerReturned;
static _Thread_local int inReport;

static void submitReports(void) {
    @autoreleasepool {
        GKAchievement *a = [[GKAchievement alloc] initWithIdentifier:@"qv.test.achievement"];
        a.percentComplete = 50.0;
        int token = 123;
        void (^handler)(NSError *) = ^(NSError *error) {
            [lock lock];
            calls++;
            errors += error != nil;
            wrongError += ![error.domain isEqualToString:GKErrorDomain] ||
                          error.code != GKErrorNotSupported || token != 123;
            wrongThread += ![NSThread isMainThread];
            inlineCalls += inReport != 0;
            [lock unlock];
        };
        inReport++;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [a reportAchievementWithCompletionHandler:handler];
        [a reportAchievementWithCompletionHandler:nil];
#pragma clang diagnostic pop
        [GKAchievement reportAchievements:@[a] withCompletionHandler:handler];
        [GKAchievement reportAchievements:@[a] withCompletionHandler:nil];
        [GKAchievement resetAchievementsWithCompletionHandler:handler];
        [GKAchievement resetAchievementsWithCompletionHandler:nil];
        inReport--;
        // Failing report/reset must not erase local progress.
        if (a.percentComplete != 50.0) abort();
        [a release];
    }
}

int main(int argc, char **argv) {
    @autoreleasepool {
        BOOL checkPolicy = !(argc == 2 && strcmp(argv[1], "--platform-errors") == 0);
        lock = [NSLock new];
        GKAchievement *a = [[GKAchievement alloc] initWithIdentifier:@"qv.test.achievement"];
        double values[] = {100.0, 50.0, 0.0, 101.0, NAN, INFINITY};
        for (int i = 0; i < 6; i++) {
            a.percentComplete = values[i];
            if ([a isCompleted] != (i == 0)) return 1;
        }
        printf("GKAchievement.isCompleted(100): 1\n");
        printf("GKAchievement.isCompleted(50): 0\n");
        printf("GKAchievement.respondsTo(reportAchievementWithCompletionHandler:): %d\n",
               [a respondsToSelector:@selector(reportAchievementWithCompletionHandler:)]);
        [a release];
        @try {
            submitReports();
            printf("GKAchievement.reportAchievementWithCompletionHandler.raised: 0\n");
        } @catch (NSException *e) {
            printf("GKAchievement.reportAchievementWithCompletionHandler.raised: 1\n");
            return 1;
        }
        [lock lock];
        BOOL deferred = calls == 0;
        [lock unlock];
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            submitReports();
            [lock lock];
            workerReturned = YES;
            [lock unlock];
        });
        NSDate *until = [NSDate dateWithTimeIntervalSinceNow:5];
        BOOL finished = NO;
        do {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
            [lock lock];
            finished = workerReturned && calls >= 6;
            [lock unlock];
        } while (!finished && [until timeIntervalSinceNow] > 0);
        [lock lock];
        printf("GKAchievement.report/reset.handlerCalls: %d\n", calls);
        printf("GKAchievement.report/reset.errors: %d\n", errors);
        printf("GKAchievement.report/reset.wrongError: %d\n", wrongError);
        printf("GKAchievement.report/reset.wrongThread: %d\n", wrongThread);
        printf("GKAchievement.report/reset.inlineCalls: %d\n", inlineCalls);
        BOOL passed = deferred && finished && calls == 6 && !wrongThread &&
                      !inlineCalls && (!checkPolicy || !wrongError);
        [lock unlock];
        return passed ? 0 : 1;
    }
}
