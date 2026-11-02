#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>
#define QVS_OPEN(path, flags) open(path, flags)
#define QVS_READ(fd, buffer, size) read(fd, buffer, size)
#define QVS_WRITE(fd, buffer, size) write(fd, buffer, size)
#define QVS_CLOSE(fd) close(fd)
#include "signal-origin.h"
static void fault(int number, siginfo_t *info, void *context) {
    (void)number; (void)info;
    ucontext_t *ctxt = context;
    qvsSignalOrigin((uintptr_t)ctxt->uc_mcontext.gregs[REG_RIP]);
    _exit(139);
}
int main(int argc, char **argv) {
    if (argc == 2) {
        (void)argv;
        char row[8192];
        snprintf(row, sizeof(row), "1000-2000 r-xp 0000 01:01 1 %s", qvsImages[0][0]);
        assert(qvsEqual(qvsMapImage(row, 0x1234), qvsImages[0][1]));
        assert(qvsMapImage(row, 0x2000) == NULL);
        assert(qvsEqual(qvsMapImage("1000-2000 r-xp 0000 01:01 1 /foreign/image", 0x1234), "app"));
        assert(qvsEqual(qvsMapImage("1000-2000 rw-p 0000 00:00 0", 0x1234), "app"));
        assert(qvsMapImage("fffffffffffffffff-fffffffffffffffff", 0x1234) == NULL);
        puts("PASS maps metadata classification, boundaries and overflow");
        return 0;
    }
    struct sigaction action = {.sa_sigaction = fault, .sa_flags = SA_SIGINFO};
    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGSEGV, &action, NULL) == 0);
    void *page = mmap(NULL, getpagesize(), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(page != MAP_FAILED);
    *(volatile unsigned char *)page = 1;
    return 3;
}
