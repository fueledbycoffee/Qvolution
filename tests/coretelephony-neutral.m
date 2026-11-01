#import <Foundation/Foundation.h>
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#import <CoreTelephony/CTCarrier.h>
#include <stdio.h>

static int checks, failed, deallocs;
static void expect(BOOL ok, const char *label) {
    checks++; failed += !ok;
    printf("%s %s\n", ok ? "ok" : "FAIL", label); fflush(stdout);
}
@interface CTMarker : NSObject @end
@implementation CTMarker
- (void)dealloc { deallocs++; [super dealloc]; }
@end

int main(void) {
    @autoreleasepool {
        CTTelephonyNetworkInfo *info = [[CTTelephonyNetworkInfo alloc] init];
        expect(info.subscriberCellularProvider == nil && info.serviceSubscriberCellularProviders == nil &&
            info.currentRadioAccessTechnology == nil && info.serviceCurrentRadioAccessTechnology == nil &&
            info.dataServiceIdentifier == nil, "network getters nil");
        CTCarrier *carrier = [[CTCarrier alloc] init];
        expect(carrier.carrierName == nil && carrier.mobileCountryCode == nil && carrier.mobileNetworkCode == nil &&
            carrier.isoCountryCode == nil && carrier.allowsVOIP == NO, "carrier getters nil/NO");
        [carrier release];
        CTMarker *first = [[CTMarker alloc] init];
        info.subscriberCellularProviderDidUpdateNotifier = ^(CTCarrier *c) { (void)first; };
        [first release];
        expect(deallocs == 0 && info.subscriberCellularProviderDidUpdateNotifier != nil, "notifier owns copied block");
        info.subscriberCellularProviderDidUpdateNotifier = nil;
        expect(deallocs == 1 && info.subscriberCellularProviderDidUpdateNotifier == nil, "clearing notifier releases capture");
        CTMarker *second = [[CTMarker alloc] init];
        info.serviceSubscriberCellularProvidersDidUpdateNotifier = ^(NSString *s) { (void)second; };
        [second release];
        expect(deallocs == 1 && info.serviceSubscriberCellularProvidersDidUpdateNotifier != nil, "service notifier owns copied block");
        [info release];
        expect(deallocs == 2, "dealloc releases service notifier");
    }
    printf("HARNESS done: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
