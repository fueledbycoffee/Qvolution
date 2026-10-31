/* Guest scaffolding around the unchanged frozen public callers.
 * Build -UNDEBUG. Root alone schedules execution and runtime integration.
 * A passing CPU run is not full GPU/Electron acceptance. */
#include "typed-callers.c"
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <IOSurface/IOSurfaceDarlingIPC.h>
#include <IOKit/IOKitLib.h>

static void byte_write(int fd) { char b = 'x'; assert(write(fd, &b, 1) == 1); }
static void byte_read(int fd) { char b; assert(read(fd, &b, 1) == 1); }
static void wait_success(pid_t pid) {
    int status = 0; assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
static IOSurfaceID parse_id(const char *s) {
    char *end = NULL; unsigned long n = strtoul(s, &end, 10);
    assert(end && !*end && n && n <= UINT32_MAX); return (IOSurfaceID)n;
}
static pid_t exec_id(const char *self, IOSurfaceID id) {
    char value[32]; snprintf(value, sizeof(value), "%u", id);
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) { execl(self, self, "id-child", value, NULL); _exit(127); }
    return pid;
}
static void global_case(const char *self) {
    IOSurfaceRef s = spec_create_bgra(true);
    assert(IOSurfaceLock(s, 0, NULL) == KERN_SUCCESS);
    ((uint8_t*)IOSurfaceGetBaseAddress(s))[0] = 0x31;
    assert(IOSurfaceUnlock(s, 0, NULL) == KERN_SUCCESS);
    wait_success(exec_id(self, IOSurfaceGetID(s)));
    assert(surface_load(s) == 0x72); CFRelease(s);
    puts("global ID: fresh exec lookup and shared child write passed");
}
static void global_lifetime(const char *self) {
    int toChild[2], toParent[2]; assert(pipe(toChild) == 0 && pipe(toParent) == 0);
    IOSurfaceRef s = spec_create_bgra(true); IOSurfaceID id = IOSurfaceGetID(s);
    char value[32], readfd[32], writefd[32];
    snprintf(value, sizeof(value), "%u", id);
    snprintf(readfd, sizeof(readfd), "%d", toChild[0]); snprintf(writefd, sizeof(writefd), "%d", toParent[1]);
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) { close(toChild[1]); close(toParent[0]); execl(self, self, "id-lifetime-child", value, readfd, writefd, NULL); _exit(127); }
    close(toChild[0]); close(toParent[1]);
    byte_read(toParent[0]); CFRelease(s); // child holds the only framework owner
    byte_write(toChild[1]); byte_read(toParent[0]);
    s = IOSurfaceLookup(id); assert(s && surface_load(s) == 0x72);
    byte_write(toChild[1]); wait_success(pid);
    assert(surface_load(s) == 0x72); CFRelease(s);
    close(toParent[0]); close(toChild[1]);
    puts("global ID: creator release and imported reference lifetime passed");
}
static mach_port_t child_receive_port(void) {
    mach_port_array_t ports = NULL; mach_msg_type_number_t count = 0;
    assert(mach_ports_lookup(mach_task_self(), &ports, &count) == KERN_SUCCESS && count && MACH_PORT_VALID(ports[0]));
    mach_port_t destination = ports[0];
    for (mach_msg_type_number_t i = 1; i < count; ++i) if (MACH_PORT_VALID(ports[i]))
        assert(mach_port_deallocate(mach_task_self(), ports[i]) == KERN_SUCCESS);
    assert(vm_deallocate(mach_task_self(), (vm_address_t)ports, count * sizeof(*ports)) == KERN_SUCCESS);
    mach_port_t receive;
    assert(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &receive) == KERN_SUCCESS);
    assert(mach_port_insert_right(mach_task_self(), receive, receive, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);
    spec_send_surface_right(destination, receive); // descriptor sends a real child receive-port send right
    assert(mach_port_deallocate(mach_task_self(), destination) == KERN_SUCCESS);
    return receive;
}
static void child_rendezvous(void) {
    mach_port_t receive = child_receive_port();
    spec_mach_port_exec_child(receive);
    assert(mach_port_destroy(mach_task_self(), receive) == KERN_SUCCESS);
}
static void mach_case(const char *self) {
    mach_port_t rendezvous;
    assert(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &rendezvous) == KERN_SUCCESS);
    assert(mach_port_insert_right(mach_task_self(), rendezvous, rendezvous, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);
    assert(mach_ports_register(mach_task_self(), &rendezvous, 1) == KERN_SUCCESS);
    mach_port_t exported = spec_export_without_cf_owner();
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) { execl(self, self, "mach-child", NULL); _exit(127); }
    assert(mach_ports_register(mach_task_self(), NULL, 0) == KERN_SUCCESS);
    union { struct surface_message m; uint8_t storage[sizeof(struct surface_message) + MAX_TRAILER_SIZE]; } message;
    memset(&message, 0, sizeof(message));
    assert(mach_msg(&message.m.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(message), rendezvous, 10000, MACH_PORT_NULL) == MACH_MSG_SUCCESS);
    assert((message.m.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) && message.m.body.msgh_descriptor_count == 1 &&
        message.m.surface.type == MACH_MSG_PORT_DESCRIPTOR && MACH_PORT_VALID(message.m.surface.name));
    mach_port_t childSend = message.m.surface.name;
    spec_send_surface_right(childSend, exported);
    assert(mach_port_deallocate(mach_task_self(), childSend) == KERN_SUCCESS);
    wait_success(pid);
    spec_verify_child_write_and_drop_export(exported);
    assert(mach_port_destroy(mach_task_self(), rendezvous) == KERN_SUCCESS);
    puts("Mach: descriptor transfer, port-only lifetime, independent imported CV survival and shared write passed");
}
static CFMutableDictionaryRef bgra_properties(void) {
    CFMutableDictionaryRef d = dictionary();
    number(d, kIOSurfaceWidth, 3); number(d, kIOSurfaceHeight, 2);
    number(d, kIOSurfacePixelFormat, kCVPixelFormatType_32BGRA); number(d, kIOSurfaceBytesPerElement, 4);
    return d;
}
static void foreign_task_child(IOSurfaceID id) {
    mach_port_t receive = child_receive_port();
    union { struct surface_message m; uint8_t storage[sizeof(struct surface_message) + MAX_TRAILER_SIZE]; } incoming;
    memset(&incoming, 0, sizeof(incoming));
    assert(mach_msg(&incoming.m.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(incoming), receive, 10000, MACH_PORT_NULL) == MACH_MSG_SUCCESS);
    assert((incoming.m.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) && incoming.m.body.msgh_descriptor_count == 1 &&
        incoming.m.surface.type == MACH_MSG_PORT_DESCRIPTOR && MACH_PORT_VALID(incoming.m.surface.name));
    mach_port_t foreign = incoming.m.surface.name;
    assert(mach_port_destroy(mach_task_self(), receive) == KERN_SUCCESS);
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceNameMatching("IOSurfaceRoot")); assert(service);
    assert(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &receive) == KERN_SUCCESS);
    union { struct DarlingSurfaceRequest request; struct DarlingSurfaceReply reply;
        uint8_t storage[sizeof(struct DarlingSurfaceRequest) + MAX_TRAILER_SIZE]; } message;
    memset(&message, 0, sizeof(message));
    struct DarlingSurfaceRequest *r = &message.request;
    r->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) | MACH_MSGH_BITS_COMPLEX;
    r->header.msgh_size = sizeof(*r); r->header.msgh_remote_port = service; r->header.msgh_local_port = receive;
    r->header.msgh_id = DARLING_SURFACE_MSG_ID; r->body.msgh_descriptor_count = 2;
    r->task.name = foreign; r->task.disposition = MACH_MSG_TYPE_COPY_SEND; r->task.type = MACH_MSG_PORT_DESCRIPTOR;
    r->capability.disposition = MACH_MSG_TYPE_COPY_SEND; r->capability.type = MACH_MSG_PORT_DESCRIPTOR;
    r->version = DARLING_SURFACE_LAYOUT_VERSION; r->operation = DarlingSurfaceLookupID; r->surfaceID = id;
    assert(mach_msg(&r->header, MACH_SEND_MSG | MACH_RCV_MSG | MACH_SEND_TIMEOUT | MACH_RCV_TIMEOUT,
        sizeof(*r), sizeof(message), receive, 10000, MACH_PORT_NULL) == MACH_MSG_SUCCESS);
    assert(message.reply.header.msgh_size == sizeof(message.reply) && message.reply.result == KERN_PROTECTION_FAILURE &&
        !message.reply.address && !message.reply.surfaceID && !MACH_PORT_VALID(message.reply.capability.name));
    mach_msg_destroy(&message.reply.header);
    assert(mach_port_destroy(mach_task_self(), receive) == KERN_SUCCESS);
    assert(mach_port_deallocate(mach_task_self(), foreign) == KERN_SUCCESS);
    assert(IOObjectRelease(service) == KERN_SUCCESS);
    // Own-task lookup remains authorized after the foreign-task rejection.
    IOSurfaceRef s = IOSurfaceLookup(id); assert(s); CVPixelBufferRef p = wrap(s); CFRelease(s);
    cv_store(p, 0x73); CVPixelBufferRelease(p);
}
static void foreign_task_case(const char *self) {
    IOSurfaceRef s = spec_create_bgra(true); char id[32]; snprintf(id, sizeof(id), "%u", IOSurfaceGetID(s));
    mach_port_t rendezvous; assert(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &rendezvous) == KERN_SUCCESS);
    assert(mach_port_insert_right(mach_task_self(), rendezvous, rendezvous, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);
    assert(mach_ports_register(mach_task_self(), &rendezvous, 1) == KERN_SUCCESS);
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) { execl(self, self, "foreign-task-child", id, NULL); _exit(127); }
    assert(mach_ports_register(mach_task_self(), NULL, 0) == KERN_SUCCESS);
    union { struct surface_message m; uint8_t storage[sizeof(struct surface_message) + MAX_TRAILER_SIZE]; } message;
    memset(&message, 0, sizeof(message));
    assert(mach_msg(&message.m.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(message), rendezvous, 10000, MACH_PORT_NULL) == MACH_MSG_SUCCESS);
    assert((message.m.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) && message.m.body.msgh_descriptor_count == 1 && message.m.surface.type == MACH_MSG_PORT_DESCRIPTOR);
    spec_send_surface_right(message.m.surface.name, mach_task_self()); // genuine parent task right, separately audited child sender
    assert(mach_port_deallocate(mach_task_self(), message.m.surface.name) == KERN_SUCCESS);
    wait_success(pid); assert(surface_load(s) == 0x73); CFRelease(s);
    assert(mach_port_destroy(mach_task_self(), rendezvous) == KERN_SUCCESS);
    puts("authorization: genuine foreign task descriptor from separate exec sender rejected before mapping; own-task path passed");
}
/* Deterministic lost-reply gate: prove the broker queued the acquisition
 * reply, then destroy it unread. The descriptor lease must reclaim its ref. */
static void lost_queued_reply(void) {
    IOSurfaceRef surface = spec_create_bgra(true);
    IOSurfaceID id = IOSurfaceGetID(surface);
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceNameMatching("IOSurfaceRoot"));
    assert(service);
    mach_port_t receive; assert(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &receive) == KERN_SUCCESS);
    struct DarlingSurfaceRequest r; memset(&r, 0, sizeof(r));
    r.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) | MACH_MSGH_BITS_COMPLEX;
    r.header.msgh_size = sizeof(r); r.header.msgh_remote_port = service; r.header.msgh_local_port = receive;
    r.header.msgh_id = DARLING_SURFACE_MSG_ID; r.body.msgh_descriptor_count = 2;
    r.task.name = mach_task_self(); r.task.disposition = MACH_MSG_TYPE_COPY_SEND; r.task.type = MACH_MSG_PORT_DESCRIPTOR;
    r.capability.disposition = MACH_MSG_TYPE_COPY_SEND; r.capability.type = MACH_MSG_PORT_DESCRIPTOR;
    r.version = DARLING_SURFACE_LAYOUT_VERSION; r.operation = DarlingSurfaceLookupID; r.surfaceID = id;
    assert(mach_msg(&r.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(r), 0, MACH_PORT_NULL, 10000, MACH_PORT_NULL) == MACH_MSG_SUCCESS);
    bool queued = false;
    for (unsigned i = 0; i < 1000; ++i) {
        mach_port_status_t status; mach_msg_type_number_t count = MACH_PORT_RECEIVE_STATUS_COUNT;
        assert(mach_port_get_attributes(mach_task_self(), receive, MACH_PORT_RECEIVE_STATUS, (mach_port_info_t)&status, &count) == KERN_SUCCESS);
        if (status.mps_msgcount) { queued = true; break; }
        usleep(1000);
    }
    assert(queued); // send-success/enqueued, not merely timeout/send-failure rollback
    assert(mach_port_destroy(mach_task_self(), receive) == KERN_SUCCESS);
    assert(IOObjectRelease(service) == KERN_SUCCESS); CFRelease(surface);
    bool reclaimed = false;
    for (unsigned i = 0; i < 1000; ++i) {
        IOSurfaceRef probe = IOSurfaceLookup(id);
        if (!probe) { reclaimed = true; break; }
        CFRelease(probe); usleep(1000);
    }
    assert(reclaimed);
    puts("acquisition: unread successfully queued reply drops lease and reclaims last mapping/ID passed");
}
static void cv_attributes_and_pool(void) {
    CFMutableDictionaryRef attributes = dictionary(), properties = dictionary();
    CFDictionarySetValue(attributes, kCVPixelBufferIOSurfacePropertiesKey, properties); CFRelease(properties);
    CVPixelBufferRef p = NULL;
    CFDictionarySetValue(attributes, kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey, kCFBooleanTrue);
    assert(CVPixelBufferCreate(NULL, 3, 2, kCVPixelFormatType_32BGRA, attributes, &p) != kCVReturnSuccess && !p);
    CFDictionaryRemoveValue(attributes, kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey);
    number(attributes, kCVPixelBufferBytesPerRowAlignmentKey, 32);
    assert(CVPixelBufferCreate(NULL, 3, 2, kCVPixelFormatType_32BGRA, attributes, &p) != kCVReturnSuccess && !p);
    CFDictionaryRemoveValue(attributes, kCVPixelBufferBytesPerRowAlignmentKey);
    number(attributes, kCVPixelBufferWidthKey, 3); number(attributes, kCVPixelBufferHeightKey, 2);
    number(attributes, kCVPixelBufferPixelFormatTypeKey, kCVPixelFormatType_32BGRA);
    CVPixelBufferPoolRef pool = NULL;
    assert(CVPixelBufferPoolCreate(NULL, NULL, attributes, &pool) == kCVReturnSuccess && pool);
    CVPixelBufferRef a = NULL, b = NULL;
    assert(CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &a) == kCVReturnSuccess && CVPixelBufferGetIOSurface(a));
    assert(CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &b) == kCVReturnSuccess && CVPixelBufferGetIOSurface(b));
    assert(IOSurfaceGetID(CVPixelBufferGetIOSurface(a)) != IOSurfaceGetID(CVPixelBufferGetIOSurface(b)));
    CVPixelBufferRelease(a); CVPixelBufferRelease(b); CVPixelBufferPoolRelease(pool); CFRelease(attributes);
    puts("CV attributes: unsupported GPU/alignment fail explicitly; pool forwards genuine surface allocation without reuse passed");
}
static void negatives(void) {
    CFMutableDictionaryRef d = bgra_properties();
    CFDictionarySetValue(d, kIOSurfaceWidth, CFSTR("wrong type")); assert(!IOSurfaceCreate(d)); CFRelease(d);
    d = bgra_properties(); number(d, kIOSurfaceBytesPerRow, 11); assert(!IOSurfaceCreate(d)); CFRelease(d);
    d = bgra_properties(); number(d, kIOSurfaceAllocSize, 23); assert(!IOSurfaceCreate(d)); CFRelease(d);
    d = bgra_properties(); number(d, kIOSurfaceWidth, INT64_MAX); assert(!IOSurfaceCreate(d)); CFRelease(d);
    d = bgra_properties(); number(d, kIOSurfacePixelFormat, 0x12345678); assert(!IOSurfaceCreate(d)); CFRelease(d);
    CVPixelBufferRef out = (CVPixelBufferRef)(uintptr_t)1;
    assert(CVPixelBufferCreateWithIOSurface(NULL, NULL, NULL, &out) != kCVReturnSuccess && !out);
    mach_port_t unrelated;
    assert(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &unrelated) == KERN_SUCCESS);
    assert(mach_port_insert_right(mach_task_self(), unrelated, unrelated, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);
    assert(!IOSurfaceLookupFromMachPort(unrelated));
    mach_port_type_t type; assert(mach_port_type(mach_task_self(), unrelated, &type) == KERN_SUCCESS && (type & MACH_PORT_TYPE_SEND));
    assert(mach_port_destroy(mach_task_self(), unrelated) == KERN_SUCCESS);
    IOSurfaceRef a = spec_create_bgra(true), b = IOSurfaceLookup(IOSurfaceGetID(a)); assert(b);
    int32_t before = IOSurfaceGetUseCount(a); IOSurfaceIncrementUseCount(a);
    assert(IOSurfaceGetUseCount(b) == before + 1 && IOSurfaceIsInUse(b));
    IOSurfaceDecrementUseCount(b); assert(IOSurfaceGetUseCount(a) == before);
    uint32_t seed = IOSurfaceGetSeed(a), after = 0;
    assert(IOSurfaceLock(a, kIOSurfaceLockReadOnly, NULL) == KERN_SUCCESS);
    assert(IOSurfaceUnlock(a, kIOSurfaceLockReadOnly, &after) == KERN_SUCCESS && after == seed);
    CFRelease(a); CFRelease(b);
    puts("Darling policy: malformed layouts/imports and per-process usage/IOSurface read-only seeds passed");
}
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "foreign-task-child")) { foreign_task_child(parse_id(argv[2])); return 0; }
    if (argc == 3 && !strcmp(argv[1], "id-child")) { spec_global_id_exec_child(parse_id(argv[2])); return 0; }
    if (argc == 5 && !strcmp(argv[1], "id-lifetime-child")) {
        IOSurfaceRef s = IOSurfaceLookup(parse_id(argv[2])); assert(s); CVPixelBufferRef p = wrap(s); CFRelease(s);
        int readfd = atoi(argv[3]), writefd = atoi(argv[4]); byte_write(writefd); byte_read(readfd);
        cv_store(p, 0x72); byte_write(writefd); byte_read(readfd); CVPixelBufferRelease(p);
        close(readfd); close(writefd); return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "mach-child")) { child_rendezvous(); return 0; }
    assert(argc == 1);
    spec_packed_ownership_and_alias(); spec_two_plane_layout(); negatives();
    global_case(argv[0]); global_lifetime(argv[0]); mach_case(argv[0]); foreign_task_case(argv[0]); lost_queued_reply(); cv_attributes_and_pool();
    puts("CPU milestone only: CV read-only numeric ABI, GPU import and actual Electron workflows remain pending");
    return 0;
}
