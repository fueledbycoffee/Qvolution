/* Private protocol validation only; integers below are local test inputs and
 * are never exposed as successful public IOSurface IDs or Mach capabilities. */
#include <IOSurface/IOSurfaceDarlingIPC.h>
extern void abort(void);
extern int printf(const char*, ...);
#define check(x) do { if (!(x)) abort(); } while (0)
_Static_assert(sizeof(struct DarlingSurfaceRequest) == 3144, "wire request size");
_Static_assert(sizeof(struct DarlingSurfaceReply) == 184, "wire reply size");
#define wire_offset(type, member, value) _Static_assert(offsetof(struct type, member) == (value), #type "." #member " wire offset")
wire_offset(DarlingSurfaceRequest, task, 28);
wire_offset(DarlingSurfaceRequest, capability, 40);
wire_offset(DarlingSurfaceRequest, version, 52);
wire_offset(DarlingSurfaceRequest, properties, 72);
wire_offset(DarlingSurfaceReply, capability, 28);
wire_offset(DarlingSurfaceReply, ndr, 40);
wire_offset(DarlingSurfaceReply, address, 72);
wire_offset(DarlingSurfaceReply, mappedSize, 80);
wire_offset(DarlingSurfaceReply, layout, 88);
wire_offset(DarlingSurfaceLayout, allocationSize, 24);
wire_offset(DarlingSurfaceLayout, planes, 32);
static struct DarlingSurfaceRequest request(void) {
    struct DarlingSurfaceRequest r = {0};
    r.header.msgh_size = sizeof(r); r.header.msgh_bits = MACH_MSGH_BITS_COMPLEX;
    r.body.msgh_descriptor_count = 2; r.task.type = r.capability.type = MACH_MSG_PORT_DESCRIPTOR;
    r.task.name = 1; r.task.disposition = MACH_MSG_TYPE_PORT_SEND;
    r.version = DARLING_SURFACE_LAYOUT_VERSION; r.operation = DarlingSurfaceCreate;
    return r;
}
int main(void) {
    struct DarlingSurfaceRequest r = request(); check(DarlingSurfaceRequestValid(&r));
    r.header.msgh_size--; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.header.msgh_bits=0; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.body.msgh_descriptor_count=1; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.task.type=MACH_MSG_OOL_DESCRIPTOR; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.task.disposition=MACH_MSG_TYPE_PORT_RECEIVE; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.task.name=MACH_PORT_NULL; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.version++; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.operation=0; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.propertyLength=DARLING_SURFACE_PROPERTY_LIMIT+1; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.capability.name=2; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.operation=DarlingSurfaceLookupID; r.propertyLength=1; check(!DarlingSurfaceRequestValid(&r));
    r=request(); r.operation=DarlingSurfaceImport; check(!DarlingSurfaceRequestValid(&r));
    r.capability.name=2; r.capability.disposition=MACH_MSG_TYPE_PORT_RECEIVE; check(!DarlingSurfaceRequestValid(&r));
    r.capability.disposition=MACH_MSG_TYPE_PORT_SEND; check(DarlingSurfaceRequestValid(&r));
    printf("actual protocol header: layout ABI and descriptor/size/version/operation/payload negatives passed\n");
}
