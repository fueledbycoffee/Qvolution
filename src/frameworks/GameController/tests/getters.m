#import <GameController/GCDualShockGamepad.h>
#import <GameController/GCDualSenseGamepad.h>
#import <GameController/GCXboxGamepad.h>
#include <objc/runtime.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    Class classes[] = {[GCDualShockGamepad class], [GCDualSenseGamepad class], [GCXboxGamepad class]};
    const char *names[] = {"buttonA", "buttonB", "buttonX", "buttonY", "buttonMenu", "buttonOptions",
        "dpad", "leftThumbstick", "rightThumbstick", "leftShoulder", "rightShoulder", "leftTrigger", "rightTrigger"};
    for (unsigned c = 0; c < 3; ++c) {
        id pad = [classes[c] new];
        for (unsigned i = 0; i < sizeof(names)/sizeof(*names); ++i) {
            SEL selector = sel_registerName(names[i]);
            Method method = class_getInstanceMethod(classes[c], selector);
            if (!method) { fprintf(stderr, "%s missing typed getter %s\n", class_getName(classes[c]), names[i]); return 1; }
            char result[16]; method_getReturnType(method, result, sizeof(result));
            assert(result[0] == '@');
            assert(((id (*)(id, SEL))method_getImplementation(method))(pad, selector) == nil);
        }
        [pad release];
    }
    puts("PASS: 39 typed gamepad getters return nil");
}
