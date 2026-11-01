#import <Foundation/Foundation.h>
#import <Accessibility/AXCustomContent.h>
#import <objc/runtime.h>
#include <assert.h>
#include <dlfcn.h>
int main(int argc, char **argv) {
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    assert(argc == 2);
    void *image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!image) { fprintf(stderr, "%s\n", dlerror()); return 1; }
    Class content = objc_getClass("AXCustomContent");
    NSMutableString *label = [NSMutableString stringWithString: @"Track"];
    id object = [content customContentWithLabel: label value: @"Three"];
    [label appendString: @" changed"];
    assert([[object label] isEqualToString: @"Track"]);
    assert([[object value] isEqualToString: @"Three"]);
    [object setImportance: AXCustomContentImportanceHigh];
    id copy = [object copy];
    assert([copy importance] == AXCustomContentImportanceHigh);
    assert([copy attributedLabel] != nil && [copy attributedValue] != nil);
    [copy release];
    assert([content customContentWithLabel: nil value: @"x"] == nil);
    puts("PASS AXCustomContent preserves label/value copies and importance");
    [pool drain];
    return 0;
}
