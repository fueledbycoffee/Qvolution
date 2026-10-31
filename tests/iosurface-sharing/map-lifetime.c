/* Executes the actual memory.c interval/refcount code. Hook shims model one
 * thread and host unmap; this is not a guest mapping or Mach security test. */
#ifndef CV_SURFACE_MEMORY_SOURCE
#define CV_SURFACE_MEMORY_SOURCE "../../src/external/darlingserver/duct-tape/src/memory.c"
#endif
#include CV_SURFACE_MEMORY_SOURCE
extern int printf(const char *, ...);
extern void abort(void);
extern int fflush(void*);
#define check(x) do { if (!(x)) { printf("failed line %d: %s\n", __LINE__, #x); fflush(NULL); abort(); } } while (0)
void dtape_mutex_init(dtape_mutex_t *m) { m->dtape_owner = 0; }
void dtape_mutex_lock(dtape_mutex_t *m) { check(!m->dtape_owner); m->dtape_owner = 1; }
void dtape_mutex_unlock(dtape_mutex_t *m) { check(m->dtape_owner); m->dtape_owner = 0; }
static int fail_unmap, real_maps;
static uintptr_t source_start;
static size_t source_size;
static uintptr_t map_file(void *context, int fd, size_t pages, int protection,
    uintptr_t hint, size_t page_offset, dtape_memory_flags_t flags) {
    (void)context;
    /* Linux MAP_FIXED is 0x10, matching the host Thread::mapFile route. */
    int map_flags = MAP_SHARED | ((flags & dtape_memory_flag_overwrite) ? 0x10 : 0);
    void *p = mmap((void*)hint, pages * 4096, protection, map_flags, fd, page_offset * 4096);
    return p == MAP_FAILED ? 0 : (uintptr_t)p;
}
static bool read_memory(void *context, uintptr_t address, void *bytes, size_t size) {
    (void)context; memmove(bytes, (void*)address, size); return true;
}
static bool region(void *context, uintptr_t address, dtape_memory_region_info_t *r) {
    (void)context;
    if (address < source_start || address >= source_start + source_size) return false;
    *r = (dtape_memory_region_info_t){.start_address = source_start, .page_count = source_size / 4096,
        .protection = dtape_memory_protection_read | dtape_memory_protection_write};
    return true;
}
void dtape_stub_log(const char *function, int safety, const char *section) { (void)function; (void)safety; (void)section; abort(); }
void dtape_log(dtape_log_level_t level, const char *format, ...) { (void)level; (void)format; abort(); }
static int unmap_pages(void *context, uintptr_t address, size_t pages) {
    (void)context; return fail_unmap ? -1 : (real_maps ? munmap((void*)address, pages * 4096) : 0);
}
static const dtape_hooks_t hooks = {.task_free_pages = unmap_pages, .task_map_file = map_file,
    .task_read_memory = read_memory, .task_get_memory_region_info = region};
const dtape_hooks_t *dtape_hooks = &hooks;
static struct dtape_task kernel_fixture;
task_t kernel_task = &kernel_fixture.xnu_task;
void (panic)(const char *format, ...) { (void)format; abort(); }
static size_t entries(vm_map_t map) {
    dtape_map_shared_entry_t *e; size_t count = 0;
    RB_FOREACH(e, dtape_map_shared_entry_head, &map->shared_entries) ++count;
    return count;
}
int main(void) {
    struct dtape_task task = {0};
    vm_map_t map = dtape_vm_map_create(&task);
    check(map);
    int fd = memfd_create("surface-lifetime-test", MFD_CLOEXEC);
    check(fd >= 0 && ftruncate(fd, 8 * 4096) == 0);
    dtape_map_shared_descriptor_t *d = dtape_map_shared_descriptor_create(fd, 8 * 4096);
    check(d);
    dtape_map_shared_entry_t *e = dtape_map_shared_entry_create(0x10000, 8 * 4096, 0, d);
    dtape_map_insert_shared_entry(map, e);
    check(entries(map) == 1);
    dtape_map_shared_entry_t *found[4];
    check(dtape_map_find_shared_entries_locked(map, 0x18000, 4096, found, 4) == 0); // adjacent is not overlap
    fail_unmap = 1;
    check(vm_map_remove(map, 0x12000, 0x14000, 0) == KERN_FAILURE && entries(map) == 1);
    fail_unmap = 0;
    check(vm_map_remove(map, 0x12000, 0x14000, 0) == KERN_SUCCESS && entries(map) == 2);
    e = RB_MIN(dtape_map_shared_entry_head, &map->shared_entries);
    check(e->address == 0x10000 && e->size == 0x2000 && e->page_offset == 0);
    e = RB_NEXT(dtape_map_shared_entry_head, &map->shared_entries, e);
    check(e->address == 0x14000 && e->size == 0x4000 && e->page_offset == 4);
    check(vm_map_remove(map, 0x14000, 0x15000, 0) == KERN_SUCCESS);
    e = RB_MAX(dtape_map_shared_entry_head, &map->shared_entries);
    check(e->address == 0x15000 && e->size == 0x3000 && e->page_offset == 5);
    check(vm_map_remove(map, 0x17000, 0x18000, 0) == KERN_SUCCESS);
    check(e->size == 0x2000);
    check(vm_map_remove(map, 0x10000, 0x18000, 0) == KERN_SUCCESS && entries(map) == 0);
    dtape_map_shared_descriptor_release(d); // last descriptor release must close the real memfd
    check(ftruncate(fd, 1) == -1);
    dtape_vm_map_destroy(map);
    printf("actual memory.c: half-open adjacency, failed unmap, split/trim offsets, final memfd close passed\n");
    real_maps = 1;
    source_size = 4 * 4096;
    source_start = (uintptr_t)mmap(NULL, source_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    check((void*)source_start != MAP_FAILED);
    for (unsigned i = 0; i < 4; ++i) ((uint8_t*)source_start)[i * 4096] = i + 1;
    struct dtape_task source_task = {0}, first_task = {0}, second_task = {0};
    vm_map_t src = dtape_vm_map_create(&source_task), first = dtape_vm_map_create(&first_task), second = dtape_vm_map_create(&second_task);
    vm_prot_t current = 0, maximum = 0;
    mach_vm_offset_t a = 0, b = 0;
    check(mach_vm_remap_external_shared(first, &a, source_size, 0, VM_FLAGS_ANYWHERE, src,
        source_start, &current, &maximum, VM_INHERIT_NONE) == KERN_SUCCESS);
    check(a != source_start && ((uint8_t*)a)[0] == 1 && ((uint8_t*)a)[8192] == 3);
    ((uint8_t*)a)[0] = 0x51;
    check(((uint8_t*)source_start)[0] == 0x51);
    check(vm_map_remove(src, source_start + 4096, source_start + 8192, 0) == KERN_SUCCESS);
    check(mach_vm_remap_external_shared(second, &b, 8192, 0, VM_FLAGS_ANYWHERE, src,
        source_start + 8192, &current, &maximum, VM_INHERIT_NONE) == KERN_SUCCESS);
    check(((uint8_t*)b)[0] == 3 && ((uint8_t*)b)[4096] == 4);
    ((uint8_t*)b)[0] = 0x72;
    check(((uint8_t*)a)[8192] == 0x72 && ((uint8_t*)source_start)[8192] == 0x72);
    check(vm_map_remove(first, a, a + source_size, 0) == KERN_SUCCESS);
    check(vm_map_remove(second, b, b + 8192, 0) == KERN_SUCCESS);
    check(vm_map_remove(src, source_start, source_start + 4096, 0) == KERN_SUCCESS);
    check(vm_map_remove(src, source_start + 8192, source_start + source_size, 0) == KERN_SUCCESS);
    check(!entries(src) && !entries(first) && !entries(second));
    dtape_vm_map_destroy(src); dtape_vm_map_destroy(first); dtape_vm_map_destroy(second);
    printf("actual copy=false remap: real memfd/MAP_SHARED, two-way writes, split-offset reimport, cleanup passed\n");
    return 0;
}
