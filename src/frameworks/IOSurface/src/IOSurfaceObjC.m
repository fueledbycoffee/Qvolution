/*
 This file is part of Darling.

 Copyright (C) 2020 Lubos Dolezel

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#import <IOSurface/IOSurface.h>
#import <IOSurface/IOSurfaceObjC.h>
#import <IOSurface/IOSurfaceDarlingIPC.h>
#import <Foundation/Foundation.h>
#import <IOKit/IOCFSerialize.h>
#include <mach/mach_vm.h>
#include <stdlib.h>
#include <string.h>

static io_service_t g_surfaceService;
typedef struct { struct DarlingSurfaceReply backing; } ImplData;

static kern_return_t surfaceCall(uint32_t operation, uint32_t id, uint32_t options,
    mach_port_t capability, CFDataRef properties, struct DarlingSurfaceReply *out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!g_surfaceService) return KERN_FAILURE;
    union {
        struct DarlingSurfaceRequest request;
        struct DarlingSurfaceReply reply;
        uint8_t storage[sizeof(struct DarlingSurfaceRequest) + MAX_TRAILER_SIZE];
    } message;
    memset(&message, 0, sizeof(message));
    struct DarlingSurfaceRequest *r = &message.request;
    mach_port_t receive = MACH_PORT_NULL;
    kern_return_t result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &receive);
    if (result != KERN_SUCCESS) return result;
    r->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) | MACH_MSGH_BITS_COMPLEX;
    r->header.msgh_remote_port = g_surfaceService;
    r->header.msgh_local_port = receive;
    r->header.msgh_size = sizeof(*r); r->header.msgh_id = DARLING_SURFACE_MSG_ID;
    r->body.msgh_descriptor_count = 2;
    r->task.name = mach_task_self(); r->task.disposition = MACH_MSG_TYPE_COPY_SEND; r->task.type = MACH_MSG_PORT_DESCRIPTOR;
    r->capability.name = capability; r->capability.disposition = MACH_MSG_TYPE_COPY_SEND;
    r->capability.type = MACH_MSG_PORT_DESCRIPTOR;
    r->version = DARLING_SURFACE_LAYOUT_VERSION; r->operation = operation; r->surfaceID = id; r->options = options;
    if (properties) {
        CFIndex length = CFDataGetLength(properties);
        if (length <= 0 || length > DARLING_SURFACE_PROPERTY_LIMIT) {
            mach_port_destroy(mach_task_self(), receive); return KERN_INVALID_ARGUMENT;
        }
        r->propertyLength = (uint32_t)length;
        memcpy(r->properties, CFDataGetBytePtr(properties), length);
    }
    result = mach_msg(&r->header, MACH_SEND_MSG | MACH_RCV_MSG | MACH_SEND_TIMEOUT | MACH_RCV_TIMEOUT,
        sizeof(*r), sizeof(message), receive, 10000, MACH_PORT_NULL);
    if (result == MACH_MSG_SUCCESS) {
        const bool acquisition = operation == DarlingSurfaceCreate || operation == DarlingSurfaceLookupID || operation == DarlingSurfaceImport;
        struct DarlingSurfaceReply *reply = &message.reply;
        if (reply->header.msgh_size != sizeof(*reply) || reply->header.msgh_id != DARLING_SURFACE_MSG_ID + 100 ||
            !(reply->header.msgh_bits & MACH_MSGH_BITS_COMPLEX) || reply->body.msgh_descriptor_count != 1 ||
            reply->capability.type != MACH_MSG_PORT_DESCRIPTOR || reply->version != DARLING_SURFACE_LAYOUT_VERSION ||
            (MACH_PORT_VALID(reply->capability.name) &&
                ((operation != DarlingSurfaceExport && !acquisition) || reply->capability.disposition != MACH_MSG_TYPE_PORT_SEND)) ||
            (reply->result == KERN_SUCCESS && (acquisition || operation == DarlingSurfaceExport) && !MACH_PORT_VALID(reply->capability.name)))
            result = KERN_INVALID_ARGUMENT;
        else result = reply->result;
        if (result == KERN_SUCCESS && out) {
            *out = *reply;
            reply->capability.name = MACH_PORT_NULL; // transfer ownership to out
        }
        mach_msg_destroy(&message.reply.header);
    }
    else if ((result & ~MACH_MSG_MASK) == MACH_RCV_BODY_ERROR) {
        /* XNU copies partially delivered bodies out on this receive error.
         * Destroy rights that did arrive; send errors still contain our own
         * outbound COPY_SEND descriptors and must not destroy those. */
        mach_msg_destroy(&message.reply.header);
    }
    /* Also destroys late replies and the send-once reply right after a timeout. */
    mach_port_destroy(mach_task_self(), receive);
    return result;
}
static const struct DarlingSurfacePlane *plane(ImplData *data, NSUInteger index)
{
    if (!data || index >= (data->backing.layout.planeCount ? data->backing.layout.planeCount : 1)) return NULL;
    return &data->backing.layout.planes[index];
}
@implementation IOSurface
+ (void)initialize
{
    if (self == [IOSurface self]) {
        /* IOServiceGetMatchingService consumes the matching dictionary. */
        g_surfaceService = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceNameMatching("IOSurfaceRoot"));
    }
}
- (nullable instancetype)initWithResponse:(const struct DarlingSurfaceReply*)reply
{
    self = [super init];
    if (!self) { mach_port_deallocate(mach_task_self(), reply->capability.name); return nil; }
    if (!reply->surfaceID || !DarlingSurfaceLayoutValid(&reply->layout) || !reply->address ||
        reply->address > UINTPTR_MAX || reply->mappedSize < reply->layout.allocationSize ||
        reply->mappedSize > UINTPTR_MAX - reply->address) {
        mach_port_deallocate(mach_task_self(), reply->capability.name);
        [self release]; return nil;
    }
    ImplData *data = calloc(1, sizeof(*data));
    if (!data) {
        mach_port_deallocate(mach_task_self(), reply->capability.name);
        [self release]; return nil;
    }
    data->backing = *reply; self->_impl = data;
    return self;
}
- (nullable instancetype)initWithProperties:(NSDictionary<IOSurfacePropertyKey, id>*)properties
{
    if (!properties || CFGetTypeID((CFTypeRef)properties) != CFDictionaryGetTypeID()) { [self release]; return nil; }
    CFDataRef serialized = IOCFSerialize((CFTypeRef)properties, kIOCFSerializeToBinary);
    if (!serialized) { [self release]; return nil; }
    struct DarlingSurfaceReply reply;
    kern_return_t result = surfaceCall(DarlingSurfaceCreate, 0, 0, MACH_PORT_NULL, serialized, &reply);
    CFRelease(serialized);
    if (result != KERN_SUCCESS) { [self release]; return nil; }
    return [self initWithResponse:&reply];
}
- (nullable instancetype)initWithSurfaceID:(IOSurfaceID)surfaceID
{
    struct DarlingSurfaceReply reply;
    if (surfaceCall(DarlingSurfaceLookupID, surfaceID, 0, MACH_PORT_NULL, NULL, &reply) != KERN_SUCCESS) {
        [self release]; return nil;
    }
    return [self initWithResponse:&reply];
}
- (nullable instancetype)initWithMachPort:(mach_port_t)port
{
    struct DarlingSurfaceReply reply;
    if (!MACH_PORT_VALID(port) || surfaceCall(DarlingSurfaceImport, 0, 0, port, NULL, &reply) != KERN_SUCCESS) {
        [self release]; return nil;
    }
    return [self initWithResponse:&reply];
}
- (void)dealloc
{
    ImplData *data = _impl;
    if (data) { mach_port_deallocate(mach_task_self(), data->backing.capability.name); free(data); }
    [super dealloc];
}
- (CFTypeID)_cfTypeID { return IOSurfaceGetTypeID(); }
- (NSUInteger)hash { return [self _darlingSurfaceID]; }
- (BOOL)isEqual:(id)object {
    return [object isKindOfClass:[IOSurface class]] && [self _darlingSurfaceID] == [(IOSurface*)object _darlingSurfaceID];
}
- (IOSurfaceID)_darlingSurfaceID { return _impl ? ((ImplData*)_impl)->backing.surfaceID : 0; }
- (mach_port_t)_darlingCreateMachPort {
    struct DarlingSurfaceReply reply;
    return surfaceCall(DarlingSurfaceExport, [self _darlingSurfaceID], 0, MACH_PORT_NULL, NULL, &reply) == KERN_SUCCESS ?
        reply.capability.name : MACH_PORT_NULL;
}
- (NSInteger)allocationSize { return _impl ? ((ImplData*)_impl)->backing.layout.allocationSize : 0; }
- (NSInteger)width { return _impl ? ((ImplData*)_impl)->backing.layout.width : 0; }
- (NSInteger)height { return _impl ? ((ImplData*)_impl)->backing.layout.height : 0; }
- (OSType)pixelFormat { return _impl ? ((ImplData*)_impl)->backing.layout.pixelFormat : 0; }
- (NSUInteger)planeCount { return _impl ? ((ImplData*)_impl)->backing.layout.planeCount : 0; }
- (void*)baseAddress { return [self baseAddressOfPlaneAtIndex:0]; }
- (NSInteger)bytesPerRow { return [self bytesPerRowOfPlaneAtIndex:0]; }
- (NSInteger)bytesPerElement { return [self bytesPerElementOfPlaneAtIndex:0]; }
- (NSInteger)elementWidth { return 1; }
- (NSInteger)elementHeight { return 1; }
- (NSInteger)widthOfPlaneAtIndex:(NSUInteger)i { const struct DarlingSurfacePlane *p = plane(_impl, i); return p ? p->width : 0; }
- (NSInteger)heightOfPlaneAtIndex:(NSUInteger)i { const struct DarlingSurfacePlane *p = plane(_impl, i); return p ? p->height : 0; }
- (NSInteger)bytesPerRowOfPlaneAtIndex:(NSUInteger)i { const struct DarlingSurfacePlane *p = plane(_impl, i); return p ? p->bytesPerRow : 0; }
- (NSInteger)bytesPerElementOfPlaneAtIndex:(NSUInteger)i { const struct DarlingSurfacePlane *p = plane(_impl, i); return p ? p->bytesPerElement : 0; }
- (NSInteger)elementWidthOfPlaneAtIndex:(NSUInteger)i { return plane(_impl, i) ? 1 : 0; }
- (NSInteger)elementHeightOfPlaneAtIndex:(NSUInteger)i { return plane(_impl, i) ? 1 : 0; }
- (void*)baseAddressOfPlaneAtIndex:(NSUInteger)i {
    const struct DarlingSurfacePlane *p = plane(_impl, i);
    return p ? (void*)(uintptr_t)(((ImplData*)_impl)->backing.address + p->offset) : NULL;
}
- (uint32_t)seed {
    struct DarlingSurfaceReply reply;
    return surfaceCall(DarlingSurfaceState, [self _darlingSurfaceID], 0, MACH_PORT_NULL, NULL, &reply) == KERN_SUCCESS ? reply.seed : 0;
}
- (BOOL)isInUse {
    struct DarlingSurfaceReply reply;
    return surfaceCall(DarlingSurfaceState, [self _darlingSurfaceID], 0, MACH_PORT_NULL, NULL, &reply) == KERN_SUCCESS && reply.inUse;
}
- (int32_t)localUseCount {
    struct DarlingSurfaceReply reply;
    return surfaceCall(DarlingSurfaceState, [self _darlingSurfaceID], 0, MACH_PORT_NULL, NULL, &reply) == KERN_SUCCESS ? reply.useCount : 0;
}
- (void)incrementUseCount { surfaceCall(DarlingSurfaceUseIncrement, [self _darlingSurfaceID], 0, MACH_PORT_NULL, NULL, NULL); }
- (void)decrementUseCount { surfaceCall(DarlingSurfaceUseDecrement, [self _darlingSurfaceID], 0, MACH_PORT_NULL, NULL, NULL); }
- (kern_return_t)lockWithOptions:(IOSurfaceLockOptions)options seed:(uint32_t*)seed {
    struct DarlingSurfaceReply reply;
    kern_return_t result = surfaceCall(DarlingSurfaceLock, [self _darlingSurfaceID], options, MACH_PORT_NULL, NULL, &reply);
    if (result == KERN_SUCCESS && seed) *seed = reply.seed;
    return result;
}
- (kern_return_t)unlockWithOptions:(IOSurfaceLockOptions)options seed:(uint32_t*)seed {
    struct DarlingSurfaceReply reply;
    kern_return_t result = surfaceCall(DarlingSurfaceUnlock, [self _darlingSurfaceID], options, MACH_PORT_NULL, NULL, &reply);
    if (result == KERN_SUCCESS && seed) *seed = reply.seed;
    return result;
}
@end
