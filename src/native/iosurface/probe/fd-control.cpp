/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probe.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <algorithm>
#include <iostream>
#include <dirent.h>
#include <cerrno>

static int readsUntilFailure = -1;
extern "C" dirent* __real_readdir(DIR*);
extern "C" dirent* __wrap_readdir(DIR* directory)
{
	if (readsUntilFailure == 0) { errno = EIO; return nullptr; }
	if (readsUntilFailure > 0) --readsUntilFailure;
	return __real_readdir(directory);
}

using namespace SurfaceProbe;
int main(int argc, char** argv)
{
	try {
		const bool negative = argc == 2 && std::string(argv[1]) == "--negative-control";
		require(argc == 1 || negative, "usage: surface-probe-fd-control [--negative-control]");
		std::vector<std::string> arguments{"probe", "--enumeration-only", "--fd-audit", "--runs", "2"};
		std::vector<char*> argv; for (auto& argument : arguments) argv.push_back(argument.data());
		const auto options = parseOptions(int(argv.size()), argv.data());
		require(options.enumerationOnly && options.fdAudit && options.runs == 2, "FD diagnostic parser mismatch");
		const auto baseline = snapshotFds();
		readsUntilFailure = 2;
		bool rejected = false;
		try { snapshotFds(); }
		catch (const std::exception& e) { rejected = std::string(e.what()) == "probe FD audit directory read failed"; }
		readsUntilFailure = -1;
		require(rejected, "FD audit accepted an incomplete directory scan");
		require(snapshotFds() == baseline, "failed FD audit leaked its directory");
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
		{
			NativeFd memory(memfd_create("surface-probe-gate-payload", MFD_CLOEXEC));
			NativeFd spare(memfd_create("surface-probe-gate-spare", MFD_CLOEXEC));
			require(memory.get() >= 0 && spare.get() >= 0, "FD gate fixture failed");
			const auto before = snapshotFds();
			requireSameFds(before, before);
			auto unstable = before; unstable.front().stable = false;
			bool rejected = false;
			try { requireSameFds(before, unstable); } catch (const std::exception&) { rejected = true; }
			require(rejected, "FD gate accepted an unstable identity");
			// Model a lost exported duplicate while an unrelated descriptor closes.
			const int retained = memory.duplicate().release();
			spare = NativeFd();
			const auto after = snapshotFds();
			require(before.size() == after.size(), "negative control must defeat a count-only gate");
			std::cerr << "{\"negative_control\":\"retained-owned-duplicate\",\"fd\":" << retained
				<< ",\"count_unchanged\":true}\n";
			if (negative) { requireSameFds(before, after); return 0; }
			NativeFd cleanup(retained);
			rejected = false;
			try { requireSameFds(before, after); } catch (const std::exception&) { rejected = true; }
			require(rejected, "FD gate accepted a retained owned duplicate at unchanged count");
		}
		requireSameFds(baseline, snapshotFds());
		std::cout << "native FD identity/duplication/closure controls passed; no Vulkan calls\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
