#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
long linux_syscall(long a, long b, long c, long d, long e, long f, int nr)
{
    long result = syscall(nr, a, b, c, d, e, f);
    return result == -1 ? -errno : result;
}
void __simple_abort(void) { abort(); }
