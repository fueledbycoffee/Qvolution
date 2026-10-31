/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Versioned Darling-only descriptor protocol; not an Apple private interface. */
#ifndef IOSURFACE_DARLING_IPC_H
#define IOSURFACE_DARLING_IPC_H
#include <mach/mach.h>
#include <mach/ndr.h>
#include <IOSurface/IOSurfaceDarlingLayout.h>

#define DARLING_SURFACE_MSG_ID 0x44534631
#define DARLING_SURFACE_PROPERTY_LIMIT 3072
enum DarlingSurfaceOperation {
	DarlingSurfaceCreate = 1, DarlingSurfaceLookupID, DarlingSurfaceImport,
	DarlingSurfaceRelease, DarlingSurfaceExport, DarlingSurfaceLock,
	DarlingSurfaceUnlock, DarlingSurfaceState, DarlingSurfaceUseIncrement,
	DarlingSurfaceUseDecrement
};
struct DarlingSurfaceRequest {
	mach_msg_header_t header;
	mach_msg_body_t body;
	mach_msg_port_descriptor_t task;
	mach_msg_port_descriptor_t capability;
	uint32_t version, operation, surfaceID, options, propertyLength;
	uint8_t properties[DARLING_SURFACE_PROPERTY_LIMIT];
};
struct DarlingSurfaceReply {
	mach_msg_header_t header;
	mach_msg_body_t body;
	mach_msg_port_descriptor_t capability;
	NDR_record_t ndr;
	kern_return_t result;
	uint32_t version, surfaceID, seed, useCount, inUse;
	uint64_t address, mappedSize;
	struct DarlingSurfaceLayout layout;
};

static inline bool DarlingSurfaceRequestValid(const struct DarlingSurfaceRequest *r)
{
	return r->header.msgh_size == sizeof(*r) &&
		(r->header.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
		r->body.msgh_descriptor_count == 2 &&
		r->task.type == MACH_MSG_PORT_DESCRIPTOR &&
		r->capability.type == MACH_MSG_PORT_DESCRIPTOR &&
		r->task.disposition == MACH_MSG_TYPE_PORT_SEND &&
		MACH_PORT_VALID(r->task.name) && r->version == DARLING_SURFACE_LAYOUT_VERSION &&
		r->operation >= DarlingSurfaceCreate && r->operation <= DarlingSurfaceUseDecrement &&
		r->propertyLength <= DARLING_SURFACE_PROPERTY_LIMIT &&
		(r->operation == DarlingSurfaceCreate || !r->propertyLength) &&
		(r->operation == DarlingSurfaceImport ?
			(r->capability.disposition == MACH_MSG_TYPE_PORT_SEND && MACH_PORT_VALID(r->capability.name)) :
			r->capability.name == MACH_PORT_NULL);
}
#endif
