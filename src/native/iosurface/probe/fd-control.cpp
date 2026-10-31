/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probe.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <algorithm>
#include <iostream>

using namespace SurfaceProbe;
int main()
{
	try {
		std::vector<std::string> arguments{"probe", "--enumeration-only", "--fd-audit", "--runs", "2"};
		std::vector<char*> argv; for (auto& argument : arguments) argv.push_back(argument.data());
		const auto options = parseOptions(int(argv.size()), argv.data());
		require(options.enumerationOnly && options.fdAudit && options.runs == 2, "FD diagnostic parser mismatch");
		const auto baseline = snapshotFds();
		{
			NativeFd memory(memfd_create("surface-probe-fd-control", MFD_CLOEXEC));
			require(memory.get() >= 0, "FD control memfd failed");
			auto duplicate = memory.duplicate();
			const auto records = snapshotFds();
			auto find = [&](int fd) -> const FdIdentity& {
				const auto entry = std::find_if(records.begin(), records.end(), [fd](const auto& record) { return record.number == fd; });
				require(entry != records.end(), "FD audit missed an owned descriptor"); return *entry;
			};
			const auto& a = find(memory.get()); const auto& b = find(duplicate.get());
			require(a.stable && b.stable && a.number != b.number && a.inode == b.inode && a.device == b.device && a.target == b.target,
				"FD audit did not identify duplicate payload");
			require((a.descriptorFlags & FD_CLOEXEC) && (b.descriptorFlags & FD_CLOEXEC), "FD audit missed CLOEXEC flags");
			require(records.size() == baseline.size() + 2, "FD audit included its snapshot directory");
			setFdAudit(true); auditFds("cpu_control", 0);
			require(snapshotFds() == records, "FD audit changed descriptor ownership");
		}
		require(snapshotFds() == baseline, "FD audit or native owner leaked a descriptor");
		std::cout << "native FD identity/duplication/closure controls passed; no Vulkan calls\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
