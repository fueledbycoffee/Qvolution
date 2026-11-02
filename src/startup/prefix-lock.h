#ifndef DARLING_PREFIX_LOCK_H
#define DARLING_PREFIX_LOCK_H
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

/* Lock the directory inode: no lock file makes an empty prefix non-empty. */
static int darlingLockPrefix(const char *path)
{
 int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
 if (fd < 0) return -1;
 while (flock(fd, LOCK_EX) < 0) {
  if (errno == EINTR) continue;
  int error = errno; close(fd); errno = error; return -1;
 }
 return fd;
}
#endif
