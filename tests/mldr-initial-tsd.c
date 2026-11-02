#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../src/startup/mldr/tsd.h"

extern void sys_thread_set_tsd_base(void*, int);
extern void second_thread_set_tsd_base(void*, int);

static __thread volatile unsigned native_tls = 0x12345678;

int main(int argc, char** argv)
{
	int* native_errno = &errno;
	pid_t pid = getpid();
	if (argc == 1 || strcmp(argv[1], "--no-install"))
		assert(mldr_install_guest_tsd_page() == 0);
	uintptr_t value;
#if defined(__i386__)
	__asm__ volatile("movl %%fs:8, %0" : "=r"(value));
	assert(value == 0);
	__asm__ volatile("movl %0, %%fs:8" : : "r"((uintptr_t)0xabcdef));
	__asm__ volatile("movl %%fs:8, %0" : "=r"(value));
#else
	__asm__ volatile("movq %%gs:16, %0" : "=r"(value));
	assert(value == 0);
	__asm__ volatile("movq %0, %%gs:16" : : "r"((uintptr_t)0xabcdef));
	__asm__ volatile("movq %%gs:16, %0" : "=r"(value));
#endif
	assert(value == 0xabcdef);
	uintptr_t replacement[64] = {0};
	for (int round = 0; round < 128; ++round) {
		sys_thread_set_tsd_base(replacement, 0);
		second_thread_set_tsd_base(replacement, 0);
	}
#if defined(__i386__)
	__asm__ volatile("movl %%fs:8, %0" : "=r"(value));
#else
	__asm__ volatile("movq %%gs:16, %0" : "=r"(value));
#endif
	assert(value == 0);
	assert(&errno == native_errno && native_tls == 0x12345678);
	errno = EDOM;
	assert(getpid() == pid && errno == EDOM);
	puts("initial guest TSD read/write and native TLS preservation passed");
	return 0;
}
