/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "surface.hpp"
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace Darling::Surface;

static void require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

static Descriptor fixture()
{
	Descriptor d;
	d.binding.prefix.fill(1); d.binding.tag.fill(2);
	d.binding.object = 3; d.binding.generation = 4; d.binding.processGeneration = 5; d.binding.lease = 6;
	d.identity.device.fill(7); d.identity.driver.fill(8);
	d.image.width = 16; d.image.height = 16;
	d.allocationSize = 4096; d.alignment = 4096;
	return d;
}

static NativeFd payload(uint64_t marker)
{
	NativeFd fd(memfd_create("native-fd-control", MFD_CLOEXEC));
	require(fd.get() >= 0, "create control memfd");
	require(write(fd.get(), &marker, sizeof(marker)) == sizeof(marker), "write control payload");
	return fd;
}

static size_t fdCount()
{
	DIR* directory = opendir("/proc/self/fd");
	require(directory, "open own descriptor directory");
	size_t count = 0;
	while (auto entry = readdir(directory)) if (entry->d_name[0] != '.') ++count;
	closedir(directory);
	return count;
}

static void sendRaw(int socket, const char* mode, const Transfer& transfer)
{
	auto encoded = encode(transfer.descriptor);
	std::vector<uint8_t> bytes(encoded.begin(), encoded.end());
	size_t count = 3;
	if (!strcmp(mode, "one-fd")) count = 1;
	if (!strcmp(mode, "four-fds")) count = 4;
	if (!strcmp(mode, "ctrunc")) count = 40;
	if (!strcmp(mode, "short")) bytes.pop_back();
	if (!strcmp(mode, "long")) bytes.push_back(0);
	if (!strcmp(mode, "version")) bytes[4] = 2;
	if (!strcmp(mode, "reserved")) bytes[12] = 1;
	if (!strcmp(mode, "binding")) bytes[72] ^= 1;
	std::vector<int> descriptors(count, transfer.memory.get());
	std::vector<unsigned char> control(CMSG_SPACE(sizeof(int) * count));
	iovec data{bytes.data(), bytes.size()};
	msghdr message{};
	message.msg_iov = &data; message.msg_iovlen = 1;
	message.msg_control = control.data(); message.msg_controllen = control.size();
	auto header = CMSG_FIRSTHDR(&message);
	header->cmsg_level = SOL_SOCKET; header->cmsg_type = SCM_RIGHTS;
	header->cmsg_len = CMSG_LEN(sizeof(int) * count);
	memcpy(CMSG_DATA(header), descriptors.data(), count * sizeof(int));
	require(sendmsg(socket, &message, MSG_NOSIGNAL) == ssize_t(bytes.size()), "send control fault packet");
}

static int child(int socket, const char* mode)
{
	Transfer transfer{fixture(), payload(0x12345678), payload(0xabcdef), payload(0x987654)};
	if (!strcmp(mode, "valid") || !strcmp(mode, "peer") || !strcmp(mode, "uid") || !strcmp(mode, "gid"))
		sendTransfer(socket, transfer);
	else sendRaw(socket, mode, transfer);
	return 0;
}

static void checkExecClosure(const int* fds)
{
	const pid_t pid = fork();
	require(pid >= 0, "fork exec closure control");
	if (!pid) {
		const auto a = std::to_string(fds[0]), b = std::to_string(fds[1]), c = std::to_string(fds[2]);
		execl("/proc/self/exe", "surface-transport-control", "--closed", a.c_str(), b.c_str(), c.c_str(), nullptr);
		_exit(122);
	}
	int status = 0;
	require(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && !WEXITSTATUS(status), "received FD survived exec");
}

static void runProcessCase(const char* mode, bool valid, Error expectedError = Error::Transport)
{
	const size_t baseline = fdCount();
	int sockets[2];
	require(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets), "create control socketpair");
	NativeFd receiver(sockets[0]), sender(sockets[1]);
	const int enabled = 1;
	if (setsockopt(receiver.get(), SOL_SOCKET, SO_PASSCRED, &enabled, sizeof(enabled)))
		throw std::runtime_error(std::string("enable native credentials: ") + strerror(errno));
	const pid_t pid = fork();
	require(pid >= 0, "fork control child");
	if (!pid) {
		close(receiver.get());
		if (fcntl(sender.get(), F_SETFD, 0)) _exit(120);
		const std::string descriptor = std::to_string(sender.get());
		execl("/proc/self/exe", "surface-transport-control", "--sender", descriptor.c_str(), mode, nullptr);
		_exit(121);
	}
	sender = NativeFd();
	ExpectedTransfer expected{fixture(), uint32_t(pid), uint32_t(getuid()), uint32_t(getgid())};
	if (!strcmp(mode, "peer")) ++expected.senderPid;
	if (!strcmp(mode, "uid")) ++expected.senderUid;
	if (!strcmp(mode, "gid")) ++expected.senderGid;
	bool passed = false;
	try {
		auto received = receiveTransfer(receiver.get(), expected);
		require(valid, "accepted invalid transfer");
		const int fds[] = {received.memory.get(), received.ready.get(), received.released.get()};
		const uint64_t markers[] = {0x12345678, 0xabcdef, 0x987654};
		for (unsigned i = 0; i < 3; ++i) {
			require(fcntl(fds[i], F_GETFD) & FD_CLOEXEC, "received descriptor leaked across exec");
			uint64_t actual = 0;
			require(pread(fds[i], &actual, sizeof(actual), 0) == sizeof(actual) && actual == markers[i], "native payload identity mismatch");
		}
		checkExecClosure(fds);
		passed = true;
	} catch (const Failure& failure) {
		require(!valid && failure.code == expectedError, "wrong native transport rejection");
		passed = true;
	}
	int status = 0;
	require(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0, "exec sender failed");
	receiver = NativeFd();
	require(passed && fdCount() == baseline, "received native descriptor leak");
}

template<class Function> static void rejects(Error code, Function function)
{
	try { function(); } catch (const Failure& f) { require(f.code == code, "wrong validation failure"); return; }
	throw std::runtime_error("invalid value accepted");
}

static void descriptorControls()
{
	const auto d = fixture();
	require(encode(decode(encode(d))) == encode(d), "canonical descriptor roundtrip");
	auto changed = d;
	changed.memoryType = VK_MAX_MEMORY_TYPES;
	rejects(Error::InvalidDescriptor, [&] { encode(changed); });
	changed = d; changed.dedicated = 0;
	rejects(Error::InvalidDescriptor, [&] { encode(changed); });
	changed = d; changed.offset = 4096;
	rejects(Error::InvalidDescriptor, [&] { encode(changed); });
	changed = d; changed.allocationSize = 1;
	rejects(Error::InvalidDescriptor, [&] { encode(changed); });
	changed = d; changed.alignment = 3;
	rejects(Error::InvalidDescriptor, [&] { encode(changed); });
	changed = d; changed.image.format = VK_FORMAT_R8G8B8A8_UNORM;
	rejects(Error::UnsupportedFormat, [&] { encode(changed); });
	changed = d; changed.epoch = 2;
	rejects(Error::InvalidDescriptor, [&] { encode(changed); });
	changed = d; changed.memoryHandle = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
	rejects(Error::InvalidDescriptor, [&] { encode(changed); });
	changed = d; changed.semaphoreHandle = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
	rejects(Error::InvalidDescriptor, [&] { encode(changed); });
	changed = d; changed.identity.device[0] ^= 1;
	rejects(Error::UnsupportedDevice, [&] { validateHardwarePair(d.identity, changed.identity, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU); });
	for (auto type : {VK_PHYSICAL_DEVICE_TYPE_CPU, VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU, VK_PHYSICAL_DEVICE_TYPE_OTHER})
		rejects(Error::UnsupportedDevice, [&] { validateHardwarePair(d.identity, d.identity, type); });
	NativeFd closed;
	rejects(Error::System, [&] { closed.duplicate(); });
	int sockets[2];
	require(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets), "create closed error control pair");
	NativeFd receiver(sockets[0]), sender(sockets[1]);
	ExpectedTransfer expected{d, uint32_t(getpid()), uint32_t(getuid()), uint32_t(getgid())};
	rejects(Error::Transport, [&] { receiveTransfer(receiver.get(), expected); });
	Transfer transfer{d, NativeFd(), payload(2), payload(3)};
	rejects(Error::System, [&] { sendTransfer(sender.get(), transfer); });
	transfer.memory = payload(1); receiver = NativeFd();
	bool rejected = false;
	try { sendTransfer(sender.get(), transfer); }
	catch (const Failure& f) { rejected = f.code == Error::System || f.code == Error::Transport; }
	require(rejected, "closed peer accepted native transfer");
}

int main(int argc, char** argv)
{
	try {
		if (argc == 4 && !strcmp(argv[1], "--sender")) return child(std::stoi(argv[2]), argv[3]);
		if (argc == 5 && !strcmp(argv[1], "--closed")) {
			for (int i = 2; i < 5; ++i) require(fcntl(std::stoi(argv[i]), F_GETFD) < 0 && errno == EBADF, "native FD crossed exec");
			return 0;
		}
		descriptorControls();
		for (unsigned repeat = 0; repeat < 8; ++repeat) {
			runProcessCase("valid", true);
			for (auto mode : {"one-fd", "four-fds", "ctrunc", "short", "long"}) runProcessCase(mode, false);
			runProcessCase("version", false, Error::InvalidDescriptor);
			runProcessCase("reserved", false, Error::InvalidDescriptor);
			runProcessCase("binding", false, Error::BindingMismatch);
			for (auto mode : {"peer", "uid", "gid"}) runProcessCase(mode, false, Error::PeerMismatch);
		}
		std::cout << "native transport controls passed; 96 independent exec transfers and 8 exec closure checks; no GPU calls\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
