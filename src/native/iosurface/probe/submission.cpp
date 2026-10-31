/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probe.hpp"

namespace SurfaceProbe {
namespace {
// The probe runs one command controller per process; signals only set flags.
using Slots = std::array<std::shared_ptr<Submission>, RetirementCapacity>;
Slots& slots() { static auto* owner = new Slots; return *owner; }
void remove(const std::shared_ptr<Submission>& job) {
	for (auto& entry : slots()) if (entry == job) { entry.reset(); return; }
}
}

std::shared_ptr<Submission> Submission::begin(std::shared_ptr<Context> context)
{
	require(!context->lost(), "probe context lost before recording");
	auto job = std::shared_ptr<Submission>(new Submission(std::move(context)));
	VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pool.queueFamilyIndex = job->_context->family; pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	checked(vkCreateCommandPool(job->_context->device, &pool, nullptr, &job->_pool), "probe command pool");
	VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; checked(vkCreateFence(job->_context->device, &fence, nullptr, &job->_fence), "probe retirement fence");
	VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; allocate.commandPool = job->_pool;
	allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount = 1;
	checked(vkAllocateCommandBuffers(job->_context->device, &allocate, &job->command), "probe command buffer");
	VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	checked(vkBeginCommandBuffer(job->command, &begin), "probe begin command buffer"); return job;
}

void Submission::cleanup()
{
	require(!_pending, "probe attempted cleanup of pending work");
	if (_pool) vkDestroyCommandPool(_context->device, _pool, nullptr);
	if (_fence) vkDestroyFence(_context->device, _fence, nullptr);
	_pool = VK_NULL_HANDLE; _fence = VK_NULL_HANDLE; command = VK_NULL_HANDLE;
	images.clear(); buffers.clear();
}
Submission::~Submission() { if (_pending) std::terminate(); cleanup(); }

void Submission::submit(VkSemaphore wait, VkSemaphore signal, uint64_t timeoutNs)
{
	require(!_pending && timeoutNs <= 10000000000ULL, "invalid probe submission state/timeout");
	checked(vkEndCommandBuffer(command), "probe end command buffer");
	auto self = shared_from_this();
	bool reserved = false;
	for (auto& entry : slots()) if (!entry) { entry = self; reserved = true; break; }
	require(reserved, "probe bounded retirement slots exhausted before submit");
	VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO}; info.commandBufferCount = 1; info.pCommandBuffers = &command;
	if (wait) { info.waitSemaphoreCount = 1; info.pWaitSemaphores = &wait; info.pWaitDstStageMask = &stage; }
	if (signal) { info.signalSemaphoreCount = 1; info.pSignalSemaphores = &signal; }
	const auto submitted = _context->submit(info, _fence);
	if (submitted == VK_ERROR_OUT_OF_HOST_MEMORY || submitted == VK_ERROR_OUT_OF_DEVICE_MEMORY || submitted == VK_ERROR_DEVICE_LOST) {
		if (submitted == VK_ERROR_DEVICE_LOST) _context->markLost();
		remove(self); checked(submitted, "probe queue submit");
	}
	_pending = true;
	if (submitted != VK_SUCCESS) throw Failure(Error::Vulkan, "probe unexpected submit result retained", submitted);
	const auto completed = vkWaitForFences(_context->device, 1, &_fence, VK_TRUE, timeoutNs);
	if (completed != VK_SUCCESS && completed != VK_ERROR_DEVICE_LOST)
		throw Failure(completed == VK_TIMEOUT ? Error::Timeout : Error::Vulkan, "probe submission pending in retirement registry", completed);
	if (completed == VK_ERROR_DEVICE_LOST) _context->markLost();
	_pending = false; cleanup(); remove(self); checked(completed, "probe finite fence wait");
}
bool Submission::poll()
{
	auto self = shared_from_this();
	if (!_pending) return true;
	const auto result = _context->lost() ? VK_ERROR_DEVICE_LOST : vkGetFenceStatus(_context->device, _fence);
	if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) return false;
	if (result == VK_ERROR_DEVICE_LOST) _context->markLost();
	_pending = false; cleanup(); remove(self); return true;
}
size_t Submission::pending() { size_t n = 0; for (const auto& entry : slots()) if (entry) ++n; return n; }
void Submission::pollAll() { auto snapshot = slots(); for (const auto& entry : snapshot) if (entry) entry->poll(); }
}
