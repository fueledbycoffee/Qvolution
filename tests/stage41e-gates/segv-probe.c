/* Authored control: hardware SIGSEGV in this foreign-to-allowlist app. */
#include <sys/mman.h>
#include <unistd.h>
int main(void) {
    void *page = mmap(0, getpagesize(), PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (page == MAP_FAILED) return 2;
    *(volatile unsigned char *)page = 1;
    return 3;
}
