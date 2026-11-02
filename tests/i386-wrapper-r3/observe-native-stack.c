/* Observe only this authored native ELF process, before loading any Mach-O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	FILE* maps = fopen("/proc/self/maps", "r");
	assert(maps);
	char line[1024];
	while (fgets(line, sizeof(line), maps)) {
		unsigned long start, end;
		assert(sscanf(line, "%lx-%lx", &start, &end) == 2);
		if (start < 0xffff0000UL && end > 0xfffe0000UL) {
			printf("%s", line);
			fclose(maps);
			return strstr(line, "[stack]") ? 10 : 11;
		}
	}
	fclose(maps);
	return 0;
}
