#ifndef DARLING_DAEMON_STDIO_H
#define DARLING_DAEMON_STDIO_H
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>

/* Call as the prefix owner. A daemon must never retain a launcher's pipes,
 * including when stderr diagnostics are requested. launchd inherits these too. */
static bool darlingDetachStdio(const char *logPath)
{
 int nullfd = open("/dev/null", O_RDWR | O_CLOEXEC);
 if (nullfd < 0) return false;
 int input = fcntl(nullfd, F_DUPFD_CLOEXEC, 3);
 close(nullfd);
 if (input < 0) return false;
 int logfd = open(logPath, O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600);
 if (logfd < 0) { close(input); return false; }
 struct stat st;
 if (fstat(logfd, &st) != 0 || !S_ISREG(st.st_mode)) { close(logfd); close(input); return false; }
 int output = fcntl(logfd, F_DUPFD_CLOEXEC, 3);
 close(logfd);
 if (output < 0) { close(input); return false; }
 bool ok = dup2(input, STDIN_FILENO) >= 0 && dup2(input, STDOUT_FILENO) >= 0 &&
           dup2(output, STDERR_FILENO) >= 0;
 close(input);
 close(output);
 return ok;
}
#endif
