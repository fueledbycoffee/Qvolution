#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

extern off_t kernel_lseek(int, off_t, int) __asm__("___lseek");
extern int kernel_sigaltstack(const stack_t *, stack_t *) __asm__("___sigaltstack");
extern int kernel_gettimeofday(struct timeval *, void *) __asm__("___gettimeofday");
extern int kernel_pipe(int *) __asm__("___pipe");
extern int kernel_get_ldt(int, void *, int) __asm__("_i386_get_ldt");
extern int kernel_set_ldt(int, void *, int) __asm__("_i386_set_ldt");

static int check(const char *name, int64_t value, int error) {
    if (value != -1 || errno != error) {
        fprintf(stderr, "%s returned %lld errno %d (expected -1/%d)\n",
            name, (long long)value, errno, error);
        return 1;
    }
    printf("installed %s: -1/errno %d PASS\n", name, error);
    return 0;
}

static void *thread_control(void *unused) {
    (void)unused;
    errno = 0;
    return (void *)(uintptr_t)check("pthread lseek", kernel_lseek(-1, 0, SEEK_SET), EBADF);
}

static int process_controls(void) {
    pthread_t thread;
    void *result;
    if (pthread_create(&thread, 0, thread_control, 0) || pthread_join(thread, &result) || result)
        return 1;
    for (int kind = 0; kind < 2; ++kind) {
        pid_t pid = kind ? vfork() : fork();
        if (!pid) _exit(0);
        int status;
        if (pid < 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status))
            return 1;
        printf("installed %s parent/child PASS\n", kind ? "vfork" : "fork");
    }
    return 0;
}

static int pipe_error(void) {
    struct rlimit old, limit;
    if (getrlimit(RLIMIT_NOFILE, &old)) return 1;
    limit = old;
    if (limit.rlim_cur > 64) limit.rlim_cur = 64;
    if (setrlimit(RLIMIT_NOFILE, &limit)) return 1;
    int opened[64], count = 0, fd;
    while (count < 64 && (fd = open("/dev/null", O_RDONLY)) >= 0) opened[count++] = fd;
    int exhausted = count < 64 && errno == EMFILE;
    int descriptors[] = {0x3344, 0x5566};
    errno = 0;
    int failed = check("pipe", kernel_pipe(descriptors), EMFILE) || !exhausted ||
        descriptors[0] != 0x3344 || descriptors[1] != 0x5566;
    while (count) close(opened[--count]);
    if (setrlimit(RLIMIT_NOFILE, &old)) failed = 1;
    return failed;
}

int main(void) {
    int failed = 0;
    errno = 0;
    failed |= check("lseek", kernel_lseek(-1, 0, SEEK_SET), EBADF);
    stack_t ss = {0};
    ss.ss_flags = 0x4000;
    errno = 0;
    failed |= check("sigaltstack", kernel_sigaltstack(&ss, 0), EINVAL);
    errno = 0;
    failed |= check("syscall(close)", syscall(SYS_close, -1), EBADF);
    struct timeval tv = {0};
    errno = 0;
    /* The handler passes tz to Linux; tv is marshalled in user space. */
    failed |= check("gettimeofday", kernel_gettimeofday(&tv, (void *)1), EFAULT);
#if defined(__i386__)
    errno = 0;
    failed |= check("get_ldt", kernel_get_ldt(-1, 0, 0), ENOSYS);
    errno = 0;
    failed |= check("set_ldt", kernel_set_ldt(-1, 0, 0), ENOSYS);
#endif
    failed |= pipe_error();
    failed |= process_controls();
    return failed;
}
