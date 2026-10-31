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

CFTypeID IOSurfaceGetTypeID(void)
{
	return (CFTypeID) [IOSurface self];
}

IOSurfaceRef _Nullable IOSurfaceCreate(CFDictionaryRef properties)
{
	return (IOSurfaceRef) [[IOSurface alloc] initWithProperties: (NSDictionary*) properties];
}

IOSurfaceRef _Nullable IOSurfaceLookupFromMachPort(mach_port_t port)
{
	return (IOSurfaceRef) [[IOSurface alloc] initWithMachPort: port];
}

xpc_object_t IOSurfaceCreateXPCObject(IOSurfaceRef surface)
{
	return 0;
}

IOSurfaceRef IOSurfaceLookupFromXPCObject(xpc_object_t obj)
{
#if NOTYET
	if (xpc_get_type(obj) == _xpc_type_mach_send)
	{
		mach_port_t port = xpc_mach_send_get_right(obj);
		return IOSurfaceLookupFromMachPort(port);
	}
#endif
	return NULL;
}

kern_return_t IOSurfaceLock(IOSurfaceRef buffer, IOSurfaceLockOptions options, uint32_t * _Nullable seed)
{
	return [(IOSurface*)buffer lockWithOptions: options seed: seed];
}

kern_return_t IOSurfaceUnlock(IOSurfaceRef buffer, IOSurfaceLockOptions options, uint32_t * _Nullable seed)
{
	return [(IOSurface*)buffer unlockWithOptions: options seed: seed];
}

size_t IOSurfaceGetWidth(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer width];
}

size_t IOSurfaceGetHeight(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer height];
}

void *IOSurfaceGetBaseAddress(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer baseAddress];
}

OSType IOSurfaceGetPixelFormat(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer pixelFormat];
}

void IOSurfaceSetValue(IOSurfaceRef buffer, CFStringRef key, CFTypeRef value)
{
	return [(IOSurface*)buffer setAttachment: (id) value forKey: (NSString*) key];
}


CFTypeRef _Nullable IOSurfaceCopyValue(IOSurfaceRef buffer, CFStringRef key)
{
	return (CFTypeRef) [[(IOSurface*) buffer attachmentForKey: (NSString*) key] retain];
}


void IOSurfaceRemoveValue(IOSurfaceRef buffer, CFStringRef key)
{
	[(IOSurface*)buffer removeAttachmentForKey: (NSString*) key];
}


void IOSurfaceSetValues(IOSurfaceRef buffer, CFDictionaryRef keysAndValues)
{
	[(IOSurface*)buffer setAllAttachments: (NSDictionary*)keysAndValues];
}


CFDictionaryRef _Nullable IOSurfaceCopyAllValues(IOSurfaceRef buffer)
{
	return (CFDictionaryRef) [[(IOSurface*)buffer allAttachments] retain];
}


void IOSurfaceRemoveAllValues(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer removeAllAttachments];
}

size_t IOSurfaceGetBytesPerRow(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer bytesPerRow];
}

void IOSurfaceIncrementUseCount(IOSurfaceRef buffer)
{
	[(IOSurface*)buffer incrementUseCount];
}


void IOSurfaceDecrementUseCount(IOSurfaceRef buffer)
{
	[(IOSurface*)buffer decrementUseCount];
}


int32_t IOSurfaceGetUseCount(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer localUseCount];
}

size_t IOSurfaceGetPlaneCount(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer planeCount];
}

uint32_t IOSurfaceGetSeed(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer seed];
}

size_t IOSurfaceGetWidthOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer widthOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetHeightOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer heightOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetBytesPerElementOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer bytesPerElementOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer bytesPerRowOfPlaneAtIndex: planeIndex];
}


void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer baseAddressOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetElementWidthOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer elementWidthOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetElementHeightOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer elementHeightOfPlaneAtIndex: planeIndex];
}

size_t IOSurfaceGetBytesPerElement(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer bytesPerElementOfPlaneAtIndex: 0];
}


size_t IOSurfaceGetElementWidth(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer elementWidth];
}


size_t IOSurfaceGetElementHeight(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer elementHeight];
}

Boolean IOSurfaceIsInUse(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer isInUse];
}

kern_return_t IOSurfaceSetPurgeable(IOSurfaceRef buffer, uint32_t newState, uint32_t * _Nullable oldState)
{
	return [(IOSurface*)buffer setPurgeable:newState oldState:oldState];
}

/* CPU backing alignment is byte alignment. A future GPU backend must expose
 * its own genuine allocation requirements instead of borrowing this promise. */
static Boolean darlingSurfaceProperty(CFStringRef key, Boolean wide)
{
    if (!key || CFGetTypeID(key) != CFStringGetTypeID()) return false;
    if (wide) return CFEqual(key, kIOSurfaceAllocSize) || CFEqual(key, kIOSurfaceOffset) ||
        CFEqual(key, kIOSurfacePlaneOffset) || CFEqual(key, kIOSurfacePlaneSize);
    const CFStringRef keys[] = {kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerRow,
        kIOSurfaceBytesPerElement, kIOSurfacePlaneWidth, kIOSurfacePlaneHeight,
        kIOSurfacePlaneBytesPerRow, kIOSurfacePlaneBytesPerElement, kIOSurfaceElementWidth,
        kIOSurfaceElementHeight, kIOSurfacePlaneElementWidth, kIOSurfacePlaneElementHeight};
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) if (CFEqual(key, keys[i])) return true;
    return false;
}
IOSurfaceRef IOSurfaceLookup(IOSurfaceID id)
{
    return (IOSurfaceRef)[[IOSurface alloc] initWithSurfaceID:id];
}
IOSurfaceID IOSurfaceGetID(IOSurfaceRef surface)
{
    return [(IOSurface*)surface _darlingSurfaceID];
}
size_t IOSurfaceGetAllocSize(IOSurfaceRef surface)
{
    return [(IOSurface*)surface allocationSize];
}
mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef surface)
{
    return surface ? [(IOSurface*)surface _darlingCreateMachPort] : MACH_PORT_NULL;
}
size_t IOSurfaceGetPropertyMaximum(CFStringRef key)
{
    if (darlingSurfaceProperty(key, true)) return SIZE_MAX < INT64_MAX ? SIZE_MAX : INT64_MAX;
    return darlingSurfaceProperty(key, false) ? UINT32_MAX : 0;
}
size_t IOSurfaceGetPropertyAlignment(CFStringRef key)
{
    return darlingSurfaceProperty(key, true) || darlingSurfaceProperty(key, false) ? 1 : 0;
}
size_t IOSurfaceAlignProperty(CFStringRef key, size_t value)
{
    return IOSurfaceGetPropertyAlignment(key) ? value : 0;
}
