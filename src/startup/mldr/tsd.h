#ifndef MLDR_TSD_H
#define MLDR_TSD_H

#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#if defined(__i386__)
#include <asm/ldt.h>
#elif defined(__x86_64__)
#include <asm/prctl.h>
#endif

#if defined(__i386__) || defined(__x86_64__)
/* MIG reads guest TSD before libpthread installs its main thread. Linux TLS
 * remains in GS on i386 and FS on x86_64; use the other segment for the guest. */
static int mldr_install_guest_tsd_page(void)
{
	void* tsd = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (tsd == MAP_FAILED)
		return -1;
#if defined(__i386__)
	struct user_desc desc = {
		.entry_number = (unsigned int)-1,
		.base_addr = (uintptr_t)tsd,
		.limit = 4095,
		.seg_32bit = 1,
		.useable = 1,
	};
	int result = syscall(SYS_set_thread_area, &desc);
	if (result == 0) {
		unsigned short selector = desc.entry_number * 8 + 3;
		__asm__ volatile("movw %0, %%fs" : : "r"(selector) : "memory");
	}
#else
	int result = syscall(SYS_arch_prctl, ARCH_SET_GS, tsd);
#endif
	if (result != 0) {
		int error = errno;
		munmap(tsd, 4096);
		errno = error;
		return -1;
	}
	return 0;
}
#endif

#endif
