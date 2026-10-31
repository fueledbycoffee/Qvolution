/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "surface.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

namespace Darling::Surface {

NativeFd::~NativeFd() { if (_fd >= 0) close(_fd); }
NativeFd::NativeFd(NativeFd&& other) noexcept : _fd(other.release()) {}
NativeFd& NativeFd::operator=(NativeFd&& other) noexcept
{
	if (this != &other) { NativeFd old(_fd); _fd = other.release(); }
	return *this;
}
int NativeFd::release() noexcept { const int fd = _fd; _fd = -1; return fd; }
NativeFd NativeFd::duplicate() const
{
	int fd;
	do { fd = fcntl(_fd, F_DUPFD_CLOEXEC, 0); } while (fd < 0 && errno == EINTR);
	if (fd < 0) throw Failure(Error::System, "duplicate native descriptor", errno);
	return NativeFd(fd);
}

static void validateSocket(int fd, bool receiving)
{
	int type = 0, passcred = 0;
	socklen_t size = sizeof(type);
	if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_SEQPACKET)
		throw Failure(Error::Transport, "native seqpacket socket required");
	sockaddr_un address{};
	size = sizeof(address);
	if (getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) || address.sun_family != AF_UNIX)
		throw Failure(Error::Transport, "native Unix socket required");
	size = sizeof(address);
	if (getpeername(fd, reinterpret_cast<sockaddr*>(&address), &size) || address.sun_family != AF_UNIX)
		throw Failure(Error::Transport, "connected native Unix socket required");
	if (receiving) {
		size = sizeof(passcred);
		if (getsockopt(fd, SOL_SOCKET, SO_PASSCRED, &passcred, &size) || !passcred)
			throw Failure(Error::Transport, "SO_PASSCRED required before receiving");
	}
}

static void validateFd(int fd)
{
	const int flags = fcntl(fd, F_GETFD);
	if (flags < 0) throw Failure(Error::System, "invalid native descriptor", errno);
	if (!(flags & FD_CLOEXEC)) throw Failure(Error::Transport, "native descriptor lacks CLOEXEC");
}

void sendTransfer(int socket, Transfer transfer)
{
	if (transfer.memory.get() < 0 || transfer.ready.get() < 0 || transfer.released.get() < 0)
		throw Failure(Error::InvalidState, "native transfer is empty or already consumed");
	validateSocket(socket, false);
	auto wire = encode(transfer.descriptor);
	const int fds[] = {transfer.memory.get(), transfer.ready.get(), transfer.released.get()};
	for (auto fd : fds) validateFd(fd);
	alignas(cmsghdr) unsigned char control[CMSG_SPACE(sizeof(fds))]{};
	iovec data{wire.data(), wire.size()};
	msghdr message{};
	message.msg_iov = &data; message.msg_iovlen = 1;
	message.msg_control = control; message.msg_controllen = sizeof(control);
	auto header = CMSG_FIRSTHDR(&message);
	header->cmsg_level = SOL_SOCKET; header->cmsg_type = SCM_RIGHTS; header->cmsg_len = CMSG_LEN(sizeof(fds));
	memcpy(CMSG_DATA(header), fds, sizeof(fds));
	const auto sent = sendmsg(socket, &message, MSG_NOSIGNAL);
	if (sent < 0) throw Failure(Error::System, "send native GPU descriptors", errno);
	if (sent != ssize_t(wire.size())) throw Failure(Error::Transport, "short native descriptor packet");
}

Transfer receiveTransfer(int socket, const ExpectedTransfer& expected)
{
	validateSocket(socket, true);
	auto expectedWire = encode(expected.descriptor);
	if (!expected.senderPid || expected.senderPid > INT32_MAX)
		throw Failure(Error::PeerMismatch, "explicit native sender process required");
	std::array<uint8_t, WireSize + 1> bytes{};
	alignas(cmsghdr) unsigned char control[CMSG_SPACE(sizeof(ucred)) + CMSG_SPACE(16 * sizeof(int))]{};
	iovec data{bytes.data(), bytes.size()};
	msghdr message{};
	message.msg_iov = &data; message.msg_iovlen = 1;
	message.msg_control = control; message.msg_controllen = sizeof(control);
	ssize_t received;
	do { received = recvmsg(socket, &message, MSG_CMSG_CLOEXEC); } while (received < 0 && errno == EINTR);
	if (received < 0) throw Failure(Error::System, "receive native GPU descriptors", errno);
	// Adopt every delivered FD before validating anything that can throw.
	std::array<NativeFd, sizeof(control) / sizeof(int)> fds;
	size_t fdCount = 0;
	unsigned credentialCount = 0;
	ucred credentials{};
	bool badAncillary = false;
	for (auto header = CMSG_FIRSTHDR(&message); header; header = CMSG_NXTHDR(&message, header)) {
		const auto available = size_t(control + message.msg_controllen - reinterpret_cast<unsigned char*>(header));
		if (header->cmsg_len < CMSG_LEN(0) || header->cmsg_len > available) { badAncillary = true; break; }
		const size_t payload = header->cmsg_len - CMSG_LEN(0);
		if (header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_RIGHTS) {
			if (!payload || payload % sizeof(int)) badAncillary = true;
			for (size_t i = 0; i < payload / sizeof(int); ++i) {
				int fd; memcpy(&fd, CMSG_DATA(header) + i * sizeof(int), sizeof(fd));
				fds[fdCount++] = NativeFd(fd);
			}
		} else if (header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_CREDENTIALS && payload == sizeof(ucred)) {
			++credentialCount;
			memcpy(&credentials, CMSG_DATA(header), sizeof(credentials));
		} else badAncillary = true;
	}
	if (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC) || received != ssize_t(WireSize) ||
		badAncillary || credentialCount != 1 || fdCount != 3)
		throw Failure(Error::Transport, "malformed or truncated native descriptor packet");
	if (credentials.pid != pid_t(expected.senderPid) || credentials.uid != expected.senderUid ||
		credentials.gid != expected.senderGid)
		throw Failure(Error::PeerMismatch, "native sender credentials mismatch");
	WireDescriptor wire;
	memcpy(wire.data(), bytes.data(), wire.size());
	auto descriptor = decode(wire);
	if (wire != expectedWire) throw Failure(Error::BindingMismatch, "descriptor differs from broker grant");
	for (size_t i = 0; i < fdCount; ++i) validateFd(fds[i].get());
	return Transfer{descriptor, std::move(fds[0]), std::move(fds[1]), std::move(fds[2])};
}

}
