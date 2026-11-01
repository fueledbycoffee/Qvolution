#import <Foundation/Foundation.h>
#import <CoreTelephony/CTSubscriberInfo.h>
#include <stdio.h>

int main(void) {
    @autoreleasepool {
        // Repeated calls must return nil in both modes; the runner counts diagnostics.
        BOOL ok = [CTSubscriberInfo subscriber] == nil && [CTSubscriberInfo subscriber] == nil;
        printf("%s subscriber nil on repeated calls\n", ok ? "ok" : "FAIL");
        printf("HARNESS done: 1 checks, %d failed\n", !ok);
        return !ok;
    }
}
