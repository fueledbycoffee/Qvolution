#!/usr/bin/env python3
"""Run mldr's production allocation block against real Linux VM collisions."""
import argparse
from pathlib import Path
import subprocess
import resource

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--ref')
p.add_argument('--bits', type=int, choices=(32, 64), default=64)
p.add_argument('--i386-address', action='store_true', help='use the actual i386 commpage boundary in a 64-bit probe')
a = p.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
a.out.mkdir(parents=True, exist_ok=True)
if a.ref:
    text = subprocess.check_output(['git', '-C', str(a.source), 'show',
                                    a.ref + ':src/startup/mldr/mldr.c'], text=True)
else:
    text = (a.source / 'src/startup/mldr/mldr.c').read_text()
block = text.split('static void setup_space(struct load_results* lr, bool is_64_bit) {', 1)[1]
block = block.split('\n\tunset_special_env();', 1)[0]
compat = text.split('static bool is_kernel_at_least(int major, int minor) {', 1)[1]
compat = 'static bool is_kernel_at_least(int major, int minor) {' + compat.split(
    '\nstatic void vchroot_unexpand_interpreter', 1)[0]
code = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "loader.h"
#define PAGE_SIZE ((size_t)sysconf(_SC_PAGESIZE))
#define MLDR_I386_STAGE(x) ((void)0)
static int kernel_major = -1, kernel_minor = -1;
static uintptr_t boundary;
static void commpage_setup(bool bits) { (void)bits; }
static unsigned long commpage_address(bool bits) { (void)bits; return boundary; }
''' + compat + '''
static unsigned long allocate(struct load_results* lr, bool is_64_bit) {
''' + block + r'''
    return size;
}
int main(int argc, char** argv) {
    assert(argc == 4);
    int occupied = atoi(argv[1]), limit_mode = atoi(argv[2]);
    size_t size = PAGE_SIZE * 16;
    struct rlimit limit;
    assert(getrlimit(RLIMIT_STACK, &limit) == 0);
    if (limit_mode == 0) limit.rlim_cur = PAGE_SIZE * 32;
    if (limit_mode == 1) limit.rlim_cur = PAGE_SIZE * 2;
    if (limit_mode == 2) limit.rlim_cur = RLIM_INFINITY;
    assert(setrlimit(RLIMIT_STACK, &limit) == 0);
    if (limit.rlim_cur != RLIM_INFINITY) size = limit.rlim_cur;
    /* A full and a partial overlap reproduce both an old stack and another VMA.
     * This arena never overwrites the real host stack, even with ASLR. */
    size_t arena_size = size + PAGE_SIZE * 2;
    bool low = atoi(argv[3]);
    void* hint = low ? (void*)(0xffff0000UL - arena_size + PAGE_SIZE) : NULL;
    unsigned char* arena = mmap(hint, arena_size, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(arena != MAP_FAILED);
    if (low) assert(arena == hint);
    boundary = (uintptr_t)(arena + arena_size - PAGE_SIZE);
    memset(arena, 0x5a, arena_size);
    size_t overlap = occupied == 2 ? PAGE_SIZE : size;
    assert(munmap(arena, arena_size - PAGE_SIZE - overlap) == 0);
    if (!occupied) assert(munmap((void*)(boundary - overlap), overlap) == 0);
    struct load_results lr = {0};
    assert(allocate(&lr, sizeof(void*) == 8) == size);
    unsigned char* stack = (void*)(lr.stack_top - size);
    /* A native 64-bit kernel may relocate above the simulated i386 boundary. */
    if (!low) assert(lr.stack_top <= boundary);
    if (occupied) {
        assert(lr.stack_top <= boundary - overlap || (uintptr_t)stack >= boundary);
        for (size_t i = 0; i < overlap; ++i)
            assert(((unsigned char*)(boundary - overlap))[i] == 0x5a);
    }
    for (size_t i = 0; i < PAGE_SIZE; ++i) assert(((unsigned char*)boundary)[i] == 0x5a);
    memset(stack, 0xa5, size);
    assert(stack[0] == 0xa5 && stack[size - 1] == 0xa5);
    assert(munmap(stack, size) == 0);
    if (occupied) assert(munmap((void*)(boundary - overlap), overlap) == 0);
    assert(munmap((void*)boundary, PAGE_SIZE) == 0);
    printf("PASS: %zu-bit overlap=%d limit=%d preserved mappings and writable stack\n",
           sizeof(void*) * 8, occupied, limit_mode);
}
'''
probe = a.out / 'probe.c'
probe.write_text(code)
failures = 0
for bits in (a.bits,):
    binary = a.out / ('probe-' + str(bits))
    subprocess.run(['/usr/bin/clang', '-m' + str(bits), '-Wall', '-Wextra', '-Werror',
                    '-I' + str(a.source / 'src/startup/mldr'), str(probe), '-o', str(binary)], check=True)
    for occupied in (0, 1, 2):
        for limit in (0, 1, 2):
            for repeat in range(20):
                result = subprocess.run([str(binary), str(occupied), str(limit), str(int(a.i386_address))],
                                        capture_output=True, text=True)
                name = f'{bits}-{occupied}-{limit}-{repeat}'
                (a.out / (name + '.stdout')).write_text(result.stdout)
                (a.out / (name + '.stderr')).write_text(result.stderr)
                if result.returncode:
                    failures += 1
                if repeat == 0:
                    print(f'{bits}-bit overlap={occupied} limit={limit}: exit {result.returncode}', flush=True)
print(f'{180 - failures}/180 native allocation cases passed; failures={failures}')
raise SystemExit(bool(failures))
