/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "retirement.private.hpp"

namespace Darling::Surface {
namespace {
struct Registry {
	std::mutex mutex;
	std::array<std::shared_ptr<RetirementTicket>, RetirementCapacity> tickets;
};
Registry& registry()
{
	// Pending Vulkan work must not be freed by static destructor ordering.
	// This bounded, explicitly pollable owner lasts until process teardown.
	static auto* value = new Registry;
	return *value;
}
}

void registerRetirement(const std::shared_ptr<RetirementTicket>& ticket)
{
	auto& r = registry();
	std::lock_guard<std::mutex> lock(r.mutex);
	for (auto& entry : r.tickets) if (!entry) { entry = ticket; return; }
	throw Failure(Error::RetirementCapacity, "native retirement registry full before submission");
}

void unregisterRetirement(const std::shared_ptr<RetirementTicket>& ticket)
{
	auto& r = registry();
	std::lock_guard<std::mutex> lock(r.mutex);
	for (auto& entry : r.tickets) if (entry == ticket) { entry.reset(); return; }
}

size_t pendingRetirements()
{
	auto& r = registry();
	std::lock_guard<std::mutex> lock(r.mutex);
	size_t count = 0;
	for (const auto& entry : r.tickets) if (entry) ++count;
	return count;
}

void pollRetirements()
{
	std::array<std::shared_ptr<RetirementTicket>, RetirementCapacity> tickets;
	{
		auto& r = registry();
		std::lock_guard<std::mutex> lock(r.mutex);
		tickets = r.tickets;
	}
	for (const auto& ticket : tickets) if (ticket) ticket->retry();
}

void Device::check(VkResult result, const char* operation)
{
	if (result == VK_ERROR_DEVICE_LOST) _lost.store(true);
	if (_lost.load()) throw Failure(Error::Vulkan, operation, VK_ERROR_DEVICE_LOST);
	if (result != VK_SUCCESS) throw Failure(Error::Vulkan, operation, result);
}

void RetirementTicket::cleanup()
{
	if (!_device) return;
	const auto device = _device->_device;
	if (_pool) vkDestroyCommandPool(device, _pool, nullptr);
	if (_fence) vkDestroyFence(device, _fence, nullptr);
	if (_ready) vkDestroySemaphore(device, _ready, nullptr);
	if (_released) vkDestroySemaphore(device, _released, nullptr);
	if (_image) vkDestroyImage(device, _image, nullptr);
	if (_memory) vkFreeMemory(device, _memory, nullptr);
	_pool = VK_NULL_HANDLE; _fence = VK_NULL_HANDLE;
	_image = VK_NULL_HANDLE; _memory = VK_NULL_HANDLE;
	_ready = _released = VK_NULL_HANDLE;
	_device.reset();
}

RetirementTicket::~RetirementTicket()
{
	if (_state == RetirementState::Pending) std::terminate();
	cleanup();
}

RetirementResult RetirementTicket::retry()
{
	auto keepAlive = shared_from_this();
	std::unique_lock<std::mutex> lock(_mutex, std::try_to_lock);
	if (!lock.owns_lock()) return {RetirementState::Pending, VK_NOT_READY};
	if (_state != RetirementState::Pending) return {_state, _result};
	_result = _device->_lost.load() ? VK_ERROR_DEVICE_LOST : vkGetFenceStatus(_device->_device, _fence);
	if (_result == VK_SUCCESS || _result == VK_ERROR_DEVICE_LOST) {
		_state = _result == VK_SUCCESS ? RetirementState::Complete : RetirementState::DeviceLost;
		if (_result == VK_ERROR_DEVICE_LOST) _device->_lost.store(true);
		cleanup();
		unregisterRetirement(keepAlive);
	}
	return {_state, _result};
}

}
