/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fake-vulkan.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

using namespace Darling::Surface;
namespace F = FakeVulkan;
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void teardown()
{
	if (F::instanceCreates() != 1 || F::instanceDestroys() != 1 ||
		F::deviceCreates() != 1 || F::deviceDestroys() != 1 || !F::teardownOrdered() || F::liveObjects()) {
		std::cerr << "process Vulkan teardown failed\n";
		std::_Exit(1);
	}
	std::cout << "process Vulkan teardown passed\n";
}
int main(int argc, char** argv)
{
	try {
		F::reset();
		std::atexit(teardown); // Runs after the subsequently constructed process owners.
		BrokerBinding binding;
		binding.prefix.fill(1); binding.tag.fill(2);
		binding.object = binding.generation = binding.processGeneration = binding.lease = 1;
		ImageSpec image; image.width = image.height = 16;
		auto concurrent = [] {
			std::vector<std::shared_ptr<Device>> opened(16);
			std::vector<std::thread> threads;
			for (auto& entry : opened) threads.emplace_back([&entry] { entry = Device::open(F::identity()); });
			for (auto& thread : threads) thread.join();
			for (const auto& entry : opened) require(entry == opened.front(), "concurrent open returned another device");
			return opened.front();
		};
		std::weak_ptr<Device> previous;
		if (argc == 2 && std::string(argv[1]) == "--concurrent-first") previous = concurrent();
		else require(argc == 1, "unexpected reuse control arguments");
		for (unsigned i = 0; i < 32; ++i) {
			auto producer = Device::open(F::identity());
			if (i) require(producer == previous.lock(), "device recreated after creator drop");
			previous = producer;
			auto source = Allocation::create(producer, binding, image, 1000000);
			auto transfer = source->exportTransfer(binding);
			auto imported = Allocation::import(Device::open(F::identity()), std::move(transfer));
			source.reset(); producer.reset(); imported.reset();
			require(F::instanceCreates() == 1 && F::deviceCreates() == 1 &&
				!F::instanceDestroys() && !F::deviceDestroys() && F::liveObjects() == 2,
				"operation retired or recreated the process Vulkan context");
		}
		auto opened = concurrent();
		require(opened == previous.lock(), "concurrent reopen replaced process device");
		for (bool driver : {false, true}) {
			auto wrong = F::identity(); (driver ? wrong.driver : wrong.device)[0] ^= 1;
			bool rejected = false;
			try { Device::open(wrong); } catch (const Failure& f) { rejected = f.code == Error::UnsupportedDevice; }
			require(rejected, "process cache accepted another UUID pair");
		}
		F::failAt(0);
		auto source = Allocation::create(opened, binding, image, 1000000);
		const auto operations = F::operations(); source.reset();
		F::waitError(VK_ERROR_DEVICE_LOST); F::failAt(operations);
		try { Allocation::create(opened, binding, image, 1000000); } catch (const Failure&) {}
		bool rejected = false;
		try { Device::open(F::identity()); } catch (const Failure& f) { rejected = f.detail == VK_ERROR_DEVICE_LOST; }
		require(rejected && F::instanceCreates() == 1 && F::deviceCreates() == 1,
			"lost process device was silently recreated");
		std::cout << "32 production export/import cycles and concurrent reuse passed; fake Vulkan only\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
