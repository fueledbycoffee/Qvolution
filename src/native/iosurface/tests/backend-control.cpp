/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fake-vulkan.hpp"
#include <dirent.h>
#include <iostream>

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

static void retirementControls()
{
	F::reset();
	unsigned createOperations, submitOperation;
	{
		auto device = Device::open(F::identity()); F::failAt(0);
		auto allocation = Allocation::create(device, binding(), image(), 1000000);
		createOperations = F::operations();
		submitOperation = F::submitOperation();
	}
	const auto baseline = fdCount();
	{
		auto device = Device::open(F::identity());
		F::submitError(VK_ERROR_UNKNOWN); F::failAt(submitOperation);
		std::shared_ptr<RetirementTicket> ticket;
		try { Allocation::create(device, binding(), image(), 1000000); }
		catch (const Failure& f) { require(f.detail == VK_ERROR_UNKNOWN, "unexpected submit failure lost"); ticket = f.retirement; }
		require(ticket && ticket->originalFailure() == VK_ERROR_UNKNOWN, "ambiguous submit destroyed its resources");
		F::fenceStatus(VK_NOT_READY); require(ticket->retry().state == RetirementState::Pending, "ambiguous submit retired early");
		F::fenceStatus(VK_SUCCESS); pollRetirements();
	}
	require(!F::liveObjects() && !pendingRetirements(), "ambiguous submit leaked completed state");
	for (auto failure : {VK_TIMEOUT, VK_ERROR_OUT_OF_HOST_MEMORY}) {
		F::reset();
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
		require(ticket->retry().state == RetirementState::Complete && !pendingRetirements() && weak.expired(),
			"completed retirement retained context");
		ticket.reset();
		require(!F::liveObjects() && fdCount() == baseline, "completed ticket leaked resources");
	}
	F::reset();
	{
		auto device = Device::open(F::identity());
		F::waitError(VK_TIMEOUT); F::failAt(createOperations);
		try { Allocation::create(device, binding(), image(), 1000000); } catch (const Failure&) {}
		F::fenceStatus(VK_ERROR_DEVICE_LOST); pollRetirements();
		require(!pendingRetirements(), "actual device loss failed to retire");
		rejects([&] { device->query(image()); });
	}
	require(!F::liveObjects(), "lost device context leaked");
	F::reset();
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
	require(weak.expired() && !pendingRetirements() && !F::liveObjects() && fdCount() == baseline,
		"explicit orphan retry did not release complete context");
}

int main()
{
	try {
		const auto baseline = fdCount();
		F::reset();
		unsigned deviceOperations;
		{ auto device = Device::open(F::identity()); deviceOperations = F::operations(); }
		for (unsigned i = 1; i <= deviceOperations; ++i) {
			F::reset(); F::failAt(i);
			rejects([] { Device::open(F::identity()); });
			require(!F::liveObjects() && fdCount() == baseline, "device failure leaked ownership");
		}
		F::reset();
		{
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
		}
		F::reset(); F::extensions(false); rejects([] { Device::open(F::identity()); });
		F::reset(); F::deviceType(VK_PHYSICAL_DEVICE_TYPE_CPU); rejects([] { Device::open(F::identity()); });
		F::reset();
		auto wrong = F::identity(); wrong.device[0] ^= 1;
		rejects([&] { Device::open(wrong); });
		require(!F::liveObjects() && fdCount() == baseline, "native backend controls leaked resources");
		retirementControls();
		std::cout << "native backend failure controls passed; control-only Vulkan, no GPU execution\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
