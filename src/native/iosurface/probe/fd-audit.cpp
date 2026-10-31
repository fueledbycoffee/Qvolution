/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probe.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <charconv>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <iostream>
#include <sstream>

namespace SurfaceProbe {
static bool enabled = false;
bool FdIdentity::operator==(const FdIdentity& o) const
{
	return number == o.number && descriptorFlags == o.descriptorFlags && statusFlags == o.statusFlags &&
		device == o.device && inode == o.inode && specialDevice == o.specialDevice && mode == o.mode &&
		stable == o.stable && target == o.target;
}
std::vector<FdIdentity> snapshotFds()
{
	auto closeDirectory = [](DIR* directory) { closedir(directory); };
	std::unique_ptr<DIR, decltype(closeDirectory)> directory(opendir("/proc/self/fd"), closeDirectory);
	require(bool(directory), "probe FD audit directory failed");
	{
		std::vector<FdIdentity> result;
		const auto ownFd = dirfd(directory.get());
		for (;;) {
			errno = 0;
			auto entry = readdir(directory.get());
			if (!entry) {
				require(errno == 0, "probe FD audit directory read failed");
				break;
			}
			FdIdentity fd;
			const auto end = entry->d_name + strlen(entry->d_name);
			const auto parsed = std::from_chars(entry->d_name, end, fd.number);
			if (parsed.ec != std::errc{} || parsed.ptr != end || fd.number == ownFd) continue;
			struct stat before{}, after{};
			const bool first = !fstat(fd.number, &before);
			std::array<char, 4096> target;
			const auto size = readlinkat(ownFd, entry->d_name, target.data(), target.size());
			require(size != ssize_t(target.size()), "probe FD audit target truncated");
			if (size >= 0) fd.target.assign(target.data(), size_t(size));
			fd.descriptorFlags = fcntl(fd.number, F_GETFD); fd.statusFlags = fcntl(fd.number, F_GETFL);
			const bool second = !fstat(fd.number, &after);
			fd.device = before.st_dev; fd.inode = before.st_ino; fd.specialDevice = before.st_rdev; fd.mode = before.st_mode;
			fd.stable = first && second && size >= 0 && fd.descriptorFlags >= 0 && fd.statusFlags >= 0 &&
				before.st_dev == after.st_dev && before.st_ino == after.st_ino && before.st_rdev == after.st_rdev && before.st_mode == after.st_mode;
			result.push_back(std::move(fd));
		}
		std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.number < b.number; });
		return result;
	}
}
void setFdAudit(bool value) { enabled = value; }
void requireSameFds(const std::vector<FdIdentity>& baseline, const std::vector<FdIdentity>& current)
{
	const auto stable = [](const auto& fd) { return fd.stable; };
	require(std::all_of(baseline.begin(), baseline.end(), stable) && std::all_of(current.begin(), current.end(), stable),
		"probe FD gate encountered an unstable identity");
	require(current == baseline, "probe FD identities changed after first-run baseline");
}
void auditFds(const char* phase, uint32_t run)
{
	if (!enabled) return;
	const auto fds = snapshotFds();
	const auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	std::ostringstream output;
	output << "{\"fd_audit\":" << jsonString(phase) << ",\"pid\":" << getpid() << ",\"run\":" << run << ",\"monotonic_ns\":" << time
		<< ",\"snapshot_directory_excluded\":true,\"fds\":[";
	for (size_t i = 0; i < fds.size(); ++i) {
		const auto& fd = fds[i];
		output << (i ? "," : "") << "{\"fd\":" << fd.number << ",\"target\":" << jsonString(fd.target) << ",\"dev\":" << fd.device
			<< ",\"ino\":" << fd.inode << ",\"rdev\":" << fd.specialDevice << ",\"mode\":" << fd.mode
			<< ",\"fd_flags\":" << fd.descriptorFlags << ",\"status_flags\":" << fd.statusFlags << ",\"stable\":" << (fd.stable ? "true" : "false") << "}";
	}
	output << "]}\n";
	std::cerr << output.str() << std::flush;
}
}
