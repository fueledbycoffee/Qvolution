#ifndef DARLING_STARTUP_READY_H
#define DARLING_STARTUP_READY_H

#include <errno.h>
#include <stdbool.h>
#include <unistd.h>

static bool startupReadReady(int fd) {
	char byte;
	ssize_t count;
	do {
		count = read(fd, &byte, 1);
	} while (count < 0 && errno == EINTR);
	if (count == 1 && byte == '.')
		return true;
	if (count >= 0)
		errno = count == 0 ? EPIPE : EPROTO;
	return false;
}

#endif
