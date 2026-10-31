#include <IOSurface/IOSurfaceDarlingLayout.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct DarlingSurfaceLayout packed(void) {
    struct DarlingSurfaceLayout l = { .version = 1, .pixelFormat = DARLING_SURFACE_BGRA,
        .width = 3, .height = 2, .allocationSize = 128,
        .planes = {{ .offset = 8, .size = 64, .width = 3, .height = 2,
            .bytesPerRow = 32, .bytesPerElement = 4 }} };
    return l;
}
int main(void) {
    struct DarlingSurfaceLayout l = packed();
    assert(DarlingSurfaceLayoutValid(&l));
    l.planes[0].bytesPerRow = 11; assert(!DarlingSurfaceLayoutValid(&l));
    l = packed(); l.allocationSize = 71; assert(!DarlingSurfaceLayoutValid(&l));
    l = packed(); l.planes[0].offset = UINT64_MAX - 4; assert(!DarlingSurfaceLayoutValid(&l));
    l = packed(); l.planes[0].height = UINT32_MAX; assert(!DarlingSurfaceLayoutValid(&l));
    l = packed(); l.planeCount = 1; assert(!DarlingSurfaceLayoutValid(&l));
    l = packed(); l.pixelFormat = 0; assert(!DarlingSurfaceLayoutValid(&l));
    l = packed(); l.global = 2; assert(!DarlingSurfaceLayoutValid(&l));
    l = (struct DarlingSurfaceLayout){ .version = 1, .pixelFormat = DARLING_SURFACE_420V,
        .width = 4, .height = 4, .planeCount = 2, .allocationSize = 112,
        .planes = {{0, 64, 4, 4, 16, 1}, {80, 32, 2, 2, 16, 2}} };
    assert(DarlingSurfaceLayoutValid(&l));
    l.planes[1].offset = 63; assert(!DarlingSurfaceLayoutValid(&l));
    l.planes[1].offset = 81; assert(!DarlingSurfaceLayoutValid(&l));
    l.planes[1].offset = 80; l.planes[1].bytesPerElement = 1;
    assert(!DarlingSurfaceLayoutValid(&l));
    puts("actual normalized layout: padded BGRA/420v and overflow/format/extent negatives passed");
}
