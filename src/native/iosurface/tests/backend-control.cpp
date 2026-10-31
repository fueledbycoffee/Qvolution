/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fake-vulkan.hpp"
#include <dirent.h>
#include <iostream>
#include <functional>
#include <sys/wait.h>
#include <unistd.h>

using namespace Darling::Surface;
namespace F = FakeVulkan;

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static size_t fdCount()
{
	DIR* d = opendir("/proc/self/fd"); require(d, "open own FD directory");
	size_t n = 0; while (auto e = readdir(d)) if (e->d_name[0] != '.') ++n;
	closedir(d); return n;
}
static BrokerBinding binding()
{
	BrokerBinding b; b.prefix.fill(1); b.tag.fill(2);
	b.object = 3; b.generation = 4; b.processGeneration = 5; b.lease = 6; return b;
}
static ImageSpec image() { ImageSpec s; s.width = s.height = 16; return s; }
template<class Function> static void rejects(Function f)
{
	try { f(); } catch (const Failure&) { pollRetirements(); return; }
	throw std::runtime_error("fault was accepted");
}

// Each setup/fault scenario starts before the process cache exists. No test-only
// cache reset is linked into the production lifetime implementation.
static unsigned isolated(const std::function<unsigned()>& run)
{
	int channel[2]; require(!pipe(channel), "control result pipe");
	const auto child = fork(); require(child >= 0, "control fork");
	if (!child) {
		close(channel[0]);
		try {
			const auto result = run();
			require(write(channel[1], &result, sizeof(result)) == sizeof(result), "control result write");
			close(channel[1]); std::exit(0);
		} catch (const std::exception& e) { std::cerr << e.what() << '\n'; std::exit(1); }
	}
	close(channel[1]); unsigned result = 0;
	const auto received = read(channel[0], &result, sizeof(result)); close(channel[0]);
	int status; require(waitpid(child, &status, 0) == child, "control wait");
	require(WIFEXITED(status) && !WEXITSTATUS(status) && received == sizeof(result), "isolated control failed");
	return result;
}

static void retirementControls()
{
	const auto createOperations = isolated([] {
		F::reset(); auto device = Device::open(F::identity()); F::failAt(0);
		auto allocation = Allocation::create(device, binding(), image(), 1000000);
		return F::operations();
	});
	const auto submitOperation = isolated([] {
		F::reset(); auto device = Device::open(F::identity()); F::failAt(0);
		auto allocation = Allocation::create(device, binding(), image(), 1000000);
		return F::submitOperation();
	});
	isolated([&] {
		F::reset();
		auto device = Device::open(F::identity());
		F::submitError(VK_ERROR_UNKNOWN); F::failAt(submitOperation);
		std::shared_ptr<RetirementTicket> ticket;
		try { Allocation::create(device, binding(), image(), 1000000); }
		catch (const Failure& f) { require(f.detail == VK_ERROR_UNKNOWN, "unexpected submit failure lost"); ticket = f.retirement; }
		require(ticket && ticket->originalFailure() == VK_ERROR_UNKNOWN, "ambiguous submit destroyed its resources");
		F::fenceStatus(VK_NOT_READY); require(ticket->retry().state == RetirementState::Pending, "ambiguous submit retired early");
		F::fenceStatus(VK_SUCCESS); pollRetirements();
		require(F::liveObjects() == 2 && !pendingRetirements(), "ambiguous submit leaked completed state");
		return 0;
	});
	for (auto failure : {VK_TIMEOUT, VK_ERROR_OUT_OF_HOST_MEMORY}) isolated([&] {
		F::reset();
		const auto baseline = fdCount();
		auto device = Device::open(F::identity());
		std::weak_ptr<Device> weak = device;
		F::waitError(failure); F::failAt(createOperations);
		std::shared_ptr<RetirementTicket> ticket;
		try { Allocation::create(device, binding(), image(), 1000000); }
		catch (const Failure& f) {
			require(f.detail == failure && f.retirement && f.retirement->originalFailure() == failure,
				"original wait failure/ticket lost");
			require(f.code == (failure == VK_TIMEOUT ? Error::Timeout : Error::Vulkan), "wrong wait failure category");
			ticket = f.retirement;
		}
		require(ticket && pendingRetirements() == 1, "failed submission not retained");
		const auto retained = F::liveObjects();
		device.reset();
		require(!weak.expired(), "pending submission lost device context");
		for (auto status : {VK_NOT_READY, VK_ERROR_OUT_OF_HOST_MEMORY}) {
			F::fenceStatus(status);
			const auto result = ticket->retry();
			require(result.state == RetirementState::Pending && result.result == status &&
				ticket->originalFailure() == failure && pendingRetirements() == 1 && F::liveObjects() == retained,
				"non-completion retry freed or rewrote pending state");
		}
		F::fenceStatus(VK_SUCCESS);
		require(ticket->retry().state == RetirementState::Complete && !pendingRetirements() && !weak.expired(),
			"completed retirement lost process context");
		ticket.reset();
		require(F::liveObjects() == 2 && fdCount() == baseline, "completed ticket leaked resources");
		return 0;
	});
	isolated([&] {
		F::reset();
		auto device = Device::open(F::identity());
		F::waitError(VK_TIMEOUT); F::failAt(createOperations);
		try { Allocation::create(device, binding(), image(), 1000000); } catch (const Failure&) {}
		F::fenceStatus(VK_ERROR_DEVICE_LOST); pollRetirements();
		require(!pendingRetirements(), "actual device loss failed to retire");
		rejects([&] { device->query(image()); });
		require(F::liveObjects() == 2, "lost device context leaked allocation objects");
		return 0;
	});
	isolated([&] {
		F::reset();
		const auto baseline = fdCount();
		auto device = Device::open(F::identity());
		std::weak_ptr<Device> weak = device;
		F::waitError(VK_TIMEOUT); F::fenceStatus(VK_NOT_READY);
		for (size_t i = 0; i < RetirementCapacity; ++i) {
			F::failAt(createOperations);
			try { Allocation::create(device, binding(), image(), 1000000); }
			catch (const Failure& f) { require(f.retirement && f.detail == VK_TIMEOUT, "missing orphan retirement ticket"); }
			require(pendingRetirements() == i + 1, "orphan pending owner lost");
		}
		F::failAt(0);
		bool rejected = false;
		try { Allocation::create(device, binding(), image(), 1000000); }
		catch (const Failure& f) { rejected = f.code == Error::RetirementCapacity; }
		require(rejected && pendingRetirements() == RetirementCapacity, "retirement registry exceeded bound");
		device.reset(); pollRetirements();
		require(!weak.expired() && pendingRetirements() == RetirementCapacity, "orphan context retired without completion");
		F::fenceStatus(VK_SUCCESS); pollRetirements();
		require(!weak.expired() && !pendingRetirements() && F::liveObjects() == 2 && fdCount() == baseline,
			"explicit orphan retry did not release complete resources");
		return 0;
	});
}

int main()
{
	try {
		const auto baseline = fdCount() + 1; // Isolated child owns its result pipe.
		const auto deviceOperations = isolated([] {
			F::reset(); auto device = Device::open(F::identity()); return F::operations();
		});
		for (unsigned i = 1; i <= deviceOperations; ++i) isolated([&] {
			F::reset(); F::failAt(i);
			rejects([] { Device::open(F::identity()); });
			require(F::liveObjects() == (i == 1 ? 0u : 1u) && fdCount() == baseline, "device failure leaked ownership");
			return 0;
		});
		isolated([&] {
			F::reset();
			auto device = Device::open(F::identity());
			const auto deviceObjects = F::liveObjects();
			unsigned createOperations;
			F::failAt(0);
			{ auto allocation = Allocation::create(device, binding(), image(), 1000000); createOperations = F::operations(); }
			for (unsigned i = 1; i <= createOperations; ++i) {
				F::failAt(i);
				rejects([&] { Allocation::create(device, binding(), image(), 1000000); });
				require(F::liveObjects() == deviceObjects && fdCount() == baseline, "allocation failure leaked ownership");
			}
			F::waitError(VK_ERROR_OUT_OF_HOST_MEMORY); F::failAt(createOperations);
			rejects([&] { Allocation::create(device, binding(), image(), 1000000); });
			require(F::liveObjects() == deviceObjects && fdCount() == baseline, "uncertain completion retired resources early");
			F::waitError(VK_ERROR_OUT_OF_HOST_MEMORY);
			for (unsigned i = 1; i <= 3; ++i) {
				F::failAt(0);
				auto allocation = Allocation::create(device, binding(), image(), 1000000);
				F::failAt(i);
				rejects([&] { allocation->exportTransfer(binding()); });
				require(fdCount() == baseline, "partial export leaked descriptor");
				F::failAt(0);
				{ auto transfer = allocation->exportTransfer(binding()); }
				rejects([&] { allocation->exportTransfer(binding()); });
			}
			unsigned importOperations;
			F::failAt(0);
			{
				auto source = Allocation::create(device, binding(), image(), 1000000);
				auto transfer = source->exportTransfer(binding());
				const auto initialized = F::initializationCount();
				F::failAt(0);
				{
					auto imported = Allocation::import(device, std::move(transfer));
					importOperations = F::operations();
					require(F::initializationCount() == initialized, "import discarded populated data");
					rejects([&] { imported->exportTransfer(binding()); });
					source.reset();
					require(F::liveObjects() > deviceObjects, "creator drop retired imported allocation");
				}
			}
			for (unsigned i = 1; i <= importOperations; ++i) {
				F::failAt(0);
				{
					auto source = Allocation::create(device, binding(), image(), 1000000);
					auto transfer = source->exportTransfer(binding());
					F::failAt(i);
					rejects([&] { Allocation::import(device, std::move(transfer)); });
					require(fdCount() == baseline, "failed import leaked native FD");
				}
				require(F::liveObjects() == deviceObjects, "failed import leaked Vulkan objects");
			}
			F::failAt(0); F::memoryTypeBits(0);
			rejects([&] { Allocation::create(device, binding(), image(), 1000000); });
			F::memoryTypeBits(1);
			{
				auto source = Allocation::create(device, binding(), image(), 1000000);
				auto transfer = source->exportTransfer(binding()); transfer.descriptor.memoryType = 1;
				rejects([&] { Allocation::import(device, std::move(transfer)); });
				require(fdCount() == baseline, "bad memory type leaked descriptor");
			}
			F::imageFeatures(VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT);
			rejects([&] { Allocation::create(device, binding(), image(), 1000000); });
			F::imageFeatures(VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT);
			F::semaphoreFeatures(VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT);
			rejects([&] { Allocation::create(device, binding(), image(), 1000000); });
			require(F::liveObjects() == deviceObjects, "unsupported capabilities allocated objects");
			return 0;
		});
		isolated([] { F::reset(); F::extensions(false); rejects([] { Device::open(F::identity()); }); return 0; });
		isolated([] { F::reset(); F::deviceType(VK_PHYSICAL_DEVICE_TYPE_CPU); rejects([] { Device::open(F::identity()); }); return 0; });
		isolated([&] {
			F::reset(); auto wrong = F::identity(); wrong.device[0] ^= 1;
			rejects([&] { Device::open(wrong); });
			require(F::liveObjects() == 1 && fdCount() == baseline, "native backend controls leaked resources");
			return 0;
		});
		retirementControls();
		std::cout << "native backend failure controls passed; control-only Vulkan, no GPU execution\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
