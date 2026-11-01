#import <Foundation/Foundation.h>
#import <Accessibility/AXCustomContent.h>
#import <objc/runtime.h>
#include <stdio.h>
#include <dlfcn.h>
#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL: %s\n", #expression); \
        return 1; \
    } \
} while (0)

int main(int argc, char **argv) {
    CHECK(argc == 2);
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    void *image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!image) { fprintf(stderr, "%s\n", dlerror()); return 1; }
    Class content = objc_getClass("AXCustomContent");
    NSMutableString *label = [NSMutableString stringWithString: @"Track"];
    id object = [content customContentWithLabel: label value: @"Three"];
    [label appendString: @" changed"];
    CHECK([[object label] isEqualToString: @"Track"]);
    CHECK([[object value] isEqualToString: @"Three"]);
    [object setImportance: AXCustomContentImportanceHigh];
    id copy = [object copy];
    CHECK([copy importance] == AXCustomContentImportanceHigh);
    CHECK([copy attributedLabel] != nil && [copy attributedValue] != nil);
    [copy release];
    CHECK([content customContentWithLabel: nil value: @"x"] == nil);
    puts("PASS AXCustomContent preserves label/value copies and importance");
    [pool drain];
    return 0;
}
