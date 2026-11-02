/* Native structural test of the production 32-bit stack builder, not a guest. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static bool i386_startup_records;
#define GEN_32BIT
#include "../src/startup/mldr/stack.c"

void elfcalls_make(struct elf_calls* calls) { memset(calls, 0, sizeof(*calls)); }
uintptr_t __darling_arm64_tsd_slot_offset(void) { return 0; }

int main(void)
{
	char* av[] = {"hello", "argument", NULL};
	char* ev[] = {"A=1", "B=2", NULL};
	unsigned char* memory = malloc(16384);
	assert(memory);
	uintptr_t top = (uintptr_t)(memory + 16384);
	for (unsigned int enabled = 0; enabled != 2; ++enabled) {
		memset(memory, 0x55, 16384);
		i386_startup_records = enabled;
		struct load_results lr = {.mh = 0x1234, .stack_top = top,
			.argc = 2, .envc = 2, .argv = av, .envp = ev, .kernfd = 7};
		setup_stack32("hello", &lr);
		uint32_t* p = (uint32_t*)lr.stack_top;
		assert(*p++ == 0x1234 && *p++ == 2);
		for (unsigned int i = 0; i != 2; ++i)
			assert(*p++ == (uint32_t)(uintptr_t)av[i]);
		assert(*p++ == 0);
		for (unsigned int i = 0; i != 2; ++i)
			assert(*p++ == (uint32_t)(uintptr_t)ev[i]);
		assert(*p++ == 0);
		for (unsigned int i = 0; i != 5; ++i) {
			/* Stack metadata is within this allocation, even when testing
			 * 32-bit pointer words in a native 64-bit process. */
			intptr_t distance = (int32_t)(p[i] - (uint32_t)top);
			char* text = (char*)(top + distance);
			assert(text >= (char*)(p + 6 + enabled) && text < (char*)top);
			assert(memchr(text, 0, (char*)top - text));
		}
		if (enabled)
			assert(p[5] != 0);
		assert(p[5 + enabled] == 0);
		for (unsigned int i = 0; i != 64; ++i)
			assert(memory[i] == 0x55);
	}
	free(memory);
	puts("PASS: i386 argv/envp and opt-in apple metadata do not overlap");
	return 0;
}
