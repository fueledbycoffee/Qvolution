/* Fresh specification-only public-boundary callers; frozen 2026-10-31.
 * Sources: SPECIFICATION.md sections 2 and 5, PUBLIC-SOURCES.md.
 * These are test entry points for a separate implementer's guest runner.
 * No implementation internals, fake handles, runtime probing, or test run here.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurface.h>
#include <mach/mach.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* R3 Apple public C declaration; intentionally missing from baseline headers. */
extern CVReturn CVPixelBufferCreateWithIOSurface(CFAllocatorRef, IOSurfaceRef,
                                                CFDictionaryRef, CVPixelBufferRef *);
static CVReturn (*const create_typed)(CFAllocatorRef, IOSurfaceRef,
    CFDictionaryRef, CVPixelBufferRef *) = CVPixelBufferCreateWithIOSurface;
/* Strict compile must reject baseline void* return; corrected public header required. */
static IOSurfaceRef (*const get_typed)(CVPixelBufferRef) = CVPixelBufferGetIOSurface;

static CFMutableDictionaryRef dictionary(void)
{
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    assert(d);
    return d;
}

static void number(CFMutableDictionaryRef d, CFStringRef key, int64_t value)
{
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt64Type, &value);
    assert(n);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

IOSurfaceRef spec_create_bgra(Boolean global)
{
    CFMutableDictionaryRef d = dictionary();
    number(d, kIOSurfaceWidth, 3);
    number(d, kIOSurfaceHeight, 2);
    number(d, kIOSurfacePixelFormat, kCVPixelFormatType_32BGRA);
    number(d, kIOSurfaceBytesPerElement, 4);
    size_t row = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, 32);
    assert(row >= 32 && row <= INT64_MAX / 2);
    size_t allocation = IOSurfaceAlignProperty(kIOSurfaceAllocSize, row * 2);
    assert(allocation >= row * 2 && allocation <= INT64_MAX);
    number(d, kIOSurfaceBytesPerRow, row);
    number(d, kIOSurfaceAllocSize, allocation);
    CFDictionarySetValue(d, kIOSurfaceIsGlobal, global ? kCFBooleanTrue : kCFBooleanFalse);
    IOSurfaceRef s = IOSurfaceCreate(d);
    CFRelease(d);
    assert(s);
    return s;
}

static CVPixelBufferRef wrap(IOSurfaceRef s)
{
    CVPixelBufferRef p = NULL;
    assert(create_typed(NULL, s, NULL, &p) == kCVReturnSuccess);
    assert(p && get_typed(p));
    assert(CFGetTypeID(p) == CVPixelBufferGetTypeID());
    assert(IOSurfaceGetID(s) == IOSurfaceGetID(get_typed(p)));
    return p;
}

static void cv_store(CVPixelBufferRef p, uint8_t value)
{
    assert(CVPixelBufferLockBaseAddress(p, 0) == kCVReturnSuccess);
    uint8_t *bytes = CVPixelBufferGetBaseAddress(p);
    assert(bytes);
    bytes[0] = value;
    assert(CVPixelBufferUnlockBaseAddress(p, 0) == kCVReturnSuccess);
}

static uint8_t surface_load(IOSurfaceRef s)
{
    assert(IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL) == KERN_SUCCESS);
    const uint8_t *bytes = IOSurfaceGetBaseAddress(s);
    assert(bytes);
    uint8_t value = bytes[0];
    assert(IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL) == KERN_SUCCESS);
    return value;
}

void spec_packed_ownership_and_alias(void)
{
    IOSurfaceRef s = spec_create_bgra(false);
    CVPixelBufferRef p = wrap(s);
    assert(CVPixelBufferGetWidth(p) == 3 && CVPixelBufferGetHeight(p) == 2);
    assert(CVPixelBufferGetBytesPerRow(p) == IOSurfaceGetBytesPerRow(s));
    assert(CVPixelBufferGetBytesPerRow(p) >= 32);
    assert(CVPixelBufferGetPixelFormatType(p) == kCVPixelFormatType_32BGRA);
    assert(!CVPixelBufferIsPlanar(p) && CVPixelBufferGetPlaneCount(p) == 0);
    assert(CVPixelBufferLockBaseAddress(p, 0) == kCVReturnSuccess);
    assert(CVPixelBufferGetBaseAddressOfPlane(p, 0) == NULL);
    assert(CVPixelBufferUnlockBaseAddress(p, 0) == kCVReturnSuccess);
    assert(CVPixelBufferGetWidthOfPlane(p, 0) == 0);
    assert(CVPixelBufferGetBytesPerRowOfPlane(p, 0) == 0);
    assert(IOSurfaceGetPlaneCount(s) == 0);
    assert(IOSurfaceGetAllocSize(s) >= IOSurfaceGetBytesPerRow(s) * 2);
    assert(IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL) == KERN_SUCCESS);
    assert(IOSurfaceGetBaseAddressOfPlane(s, 0) == IOSurfaceGetBaseAddress(s));
    assert(IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL) == KERN_SUCCESS);
    cv_store(p, 0x31);
    assert(surface_load(s) == 0x31);
    uint32_t before = 0, after = 0;
    assert(IOSurfaceLock(s, 0, &before) == KERN_SUCCESS);
    ((uint8_t *)IOSurfaceGetBaseAddress(s))[0] = 0x72;
    assert(IOSurfaceUnlock(s, 0, &after) == KERN_SUCCESS && after != before);
    assert(CVPixelBufferLockBaseAddress(p, 0) == kCVReturnSuccess);
    assert(((uint8_t *)CVPixelBufferGetBaseAddress(p))[0] == 0x72);
    assert(CVPixelBufferUnlockBaseAddress(p, 0) == kCVReturnSuccess);
    assert(CVBufferRetain((CVBufferRef)p) == (CVBufferRef)p);
    CFRelease(s); /* Only the retained CV backing keeps the original surface alive. */
    CVPixelBufferRelease(p);
    assert(CFRetain(p) == p);
    CVBufferRelease((CVBufferRef)p);
    cv_store(p, 0x43);
    assert(surface_load(get_typed(p)) == 0x43);
    CFRelease(p);
    assert(CVBufferRetain(NULL) == NULL);
    CVBufferRelease(NULL);
}

/* Runner supplies the public CV read-only constant after its value provenance
 * is established; this specification does not guess that numeric value. */
void spec_readonly(CVPixelBufferRef p, CVPixelBufferLockFlags readOnlyFlags)
{
    IOSurfaceRef s = get_typed(p);
    uint32_t before = IOSurfaceGetSeed(s);
    assert(CVPixelBufferLockBaseAddress(p, readOnlyFlags) == kCVReturnSuccess);
    assert(CVPixelBufferGetBaseAddress(p));
    assert(CVPixelBufferUnlockBaseAddress(p, readOnlyFlags) == kCVReturnSuccess);
    assert(IOSurfaceGetSeed(s) == before);
}

void spec_two_plane_layout(void)
{
    CFMutableDictionaryRef d = dictionary(), y = dictionary(), uv = dictionary();
    number(d, kIOSurfaceWidth, 4); number(d, kIOSurfaceHeight, 4);
    number(d, kIOSurfacePixelFormat, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    size_t row = IOSurfaceAlignProperty(kIOSurfacePlaneBytesPerRow, 16);
    assert(row >= 16 && row <= INT64_MAX / 8);
    size_t offset = IOSurfaceAlignProperty(kIOSurfacePlaneOffset, row * 4);
    assert(offset >= row * 4 && offset <= INT64_MAX - row * 2);
    size_t allocation = IOSurfaceAlignProperty(kIOSurfaceAllocSize, offset + row * 2);
    assert(allocation >= offset + row * 2 && allocation <= INT64_MAX);
    number(d, kIOSurfaceBytesPerElement, 1);
    number(d, kIOSurfaceBytesPerRow, IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, row));
    number(d, kIOSurfaceAllocSize, allocation);
    number(y, kIOSurfacePlaneWidth, 4); number(y, kIOSurfacePlaneHeight, 4);
    number(y, kIOSurfacePlaneBytesPerElement, 1); number(y, kIOSurfacePlaneBytesPerRow, row);
    number(y, kIOSurfacePlaneOffset, 0); number(y, kIOSurfacePlaneSize, row * 4);
    number(uv, kIOSurfacePlaneWidth, 2); number(uv, kIOSurfacePlaneHeight, 2);
    number(uv, kIOSurfacePlaneBytesPerElement, 2); number(uv, kIOSurfacePlaneBytesPerRow, row);
    number(uv, kIOSurfacePlaneOffset, offset); number(uv, kIOSurfacePlaneSize, row * 2);
    const void *values[] = {y, uv};
    CFArrayRef planes = CFArrayCreate(NULL, values, 2, &kCFTypeArrayCallBacks);
    assert(planes); CFDictionarySetValue(d, kIOSurfacePlaneInfo, planes);
    IOSurfaceRef s = IOSurfaceCreate(d);
    CFRelease(planes); CFRelease(y); CFRelease(uv); CFRelease(d);
    assert(s);
    CVPixelBufferRef p = wrap(s);
    assert(CVPixelBufferIsPlanar(p) && CVPixelBufferGetPlaneCount(p) == 2);
    assert(CVPixelBufferGetWidthOfPlane(p, 0) == 4);
    assert(CVPixelBufferGetWidthOfPlane(p, 1) == 2);
    assert(CVPixelBufferGetHeightOfPlane(p, 0) == 4);
    assert(CVPixelBufferGetHeightOfPlane(p, 1) == 2);
    assert(CVPixelBufferGetBytesPerRowOfPlane(p, 0) == row);
    assert(CVPixelBufferGetBytesPerRowOfPlane(p, 1) == row);
    assert(CVPixelBufferLockBaseAddress(p, 0) == kCVReturnSuccess);
    uint8_t *a = CVPixelBufferGetBaseAddressOfPlane(p, 0);
    uint8_t *b = CVPixelBufferGetBaseAddressOfPlane(p, 1);
    assert(a && b && b == a + offset); a[0] = 0x21; b[0] = 0x86;
    assert(CVPixelBufferUnlockBaseAddress(p, 0) == kCVReturnSuccess);
    assert(IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL) == KERN_SUCCESS);
    assert(((uint8_t *)IOSurfaceGetBaseAddressOfPlane(s, 0))[0] == 0x21);
    assert(((uint8_t *)IOSurfaceGetBaseAddressOfPlane(s, 1))[0] == 0x86);
    assert(IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL) == KERN_SUCCESS);
    CFRelease(s); CVPixelBufferRelease(p);
}

/* Fresh exec child receives only a global ID. Parent creates global BGRA,
 * writes 0x31, keeps a reference live, then checks 0x72 after child completion. */
void spec_global_id_exec_child(IOSurfaceID id)
{
    IOSurfaceRef s = IOSurfaceLookup(id);
    assert(s && IOSurfaceGetID(s) == id && surface_load(s) == 0x31);
    CVPixelBufferRef p = wrap(s);
    CFRelease(s); cv_store(p, 0x72); CVPixelBufferRelease(p);
}

struct surface_message {
    mach_msg_header_t header;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t surface;
};

/* destination is a genuine send right supplied by the runner's rendezvous.
 * The right is transferred as a descriptor, never a numeric payload. */
void spec_send_surface_right(mach_port_t destination, mach_port_t surface)
{
    struct surface_message m;
    memset(&m, 0, sizeof(m));
    m.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
    m.header.msgh_size = sizeof(m); m.header.msgh_remote_port = destination;
    m.header.msgh_id = 1; m.body.msgh_descriptor_count = 1;
    m.surface.name = surface; m.surface.disposition = MACH_MSG_TYPE_COPY_SEND;
    m.surface.type = MACH_MSG_PORT_DESCRIPTOR;
    assert(mach_msg(&m.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
        sizeof(m), 0, MACH_PORT_NULL, 10000, MACH_PORT_NULL) == MACH_MSG_SUCCESS);
}

void spec_mach_port_exec_child(mach_port_t receive)
{
    union { struct surface_message m; uint8_t storage[sizeof(struct surface_message) + MAX_TRAILER_SIZE]; } b;
    memset(&b, 0, sizeof(b));
    assert(mach_msg(&b.m.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
        sizeof(b), receive, 10000, MACH_PORT_NULL) == MACH_MSG_SUCCESS);
    assert((b.m.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
        b.m.body.msgh_descriptor_count == 1 && b.m.surface.type == MACH_MSG_PORT_DESCRIPTOR);
    mach_port_t right = b.m.surface.name;
    IOSurfaceRef s = IOSurfaceLookupFromMachPort(right);
    assert(s && surface_load(s) == 0x31 && IOSurfaceIsInUse(s));
    int32_t local = IOSurfaceGetUseCount(s);
    IOSurfaceIncrementUseCount(s); assert(IOSurfaceGetUseCount(s) == local + 1);
    IOSurfaceDecrementUseCount(s); assert(IOSurfaceGetUseCount(s) == local);
    CVPixelBufferRef p = wrap(s);
    assert(mach_port_deallocate(mach_task_self(), right) == KERN_SUCCESS);
    CFRelease(s); /* Imported port and surface owner are gone; CV wrapper survives. */
    cv_store(p, 0x72); CVPixelBufferRelease(p);
}

/* Parent port-only lifetime case: caller owns returned right, releases it only
 * after the independently launched child finishes and parent reads the change. */
mach_port_t spec_export_without_cf_owner(void)
{
    IOSurfaceRef s = spec_create_bgra(false);
    assert(IOSurfaceLock(s, 0, NULL) == KERN_SUCCESS);
    ((uint8_t *)IOSurfaceGetBaseAddress(s))[0] = 0x31;
    assert(IOSurfaceUnlock(s, 0, NULL) == KERN_SUCCESS);
    mach_port_t p = IOSurfaceCreateMachPort(s);
    assert(MACH_PORT_VALID(p)); CFRelease(s);
    s = IOSurfaceLookupFromMachPort(p);
    assert(s && surface_load(s) == 0x31 && IOSurfaceIsInUse(s));
    CFRelease(s); return p;
}

void spec_verify_child_write_and_drop_export(mach_port_t p)
{
    IOSurfaceRef s = IOSurfaceLookupFromMachPort(p);
    assert(s && surface_load(s) == 0x72);
    assert(mach_port_deallocate(mach_task_self(), p) == KERN_SUCCESS);
    assert(surface_load(s) == 0x72); CFRelease(s);
}
