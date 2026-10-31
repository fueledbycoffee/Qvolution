/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probe.hpp"
#include <algorithm>
#include <cstring>
#include <fcntl.h>

namespace SurfaceProbe {
static DeviceInfo deviceInfo(VkPhysicalDevice physical)
{
	DeviceInfo out;
	VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
	VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; properties.pNext = &ids;
	vkGetPhysicalDeviceProperties2(physical, &properties); out.properties = properties.properties;
	std::copy_n(ids.deviceUUID, VK_UUID_SIZE, out.identity.device.begin());
	std::copy_n(ids.driverUUID, VK_UUID_SIZE, out.identity.driver.begin());
	uint32_t count = 0;
	checked(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr), "probe count extensions");
	std::vector<VkExtensionProperties> extensions(count);
	checked(vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, extensions.data()), "probe extensions");
	for (uint32_t i = 0; i < count; ++i) out.extensions.emplace_back(extensions[i].extensionName);
	return out;
}

std::vector<DeviceInfo> enumerateDevices(uint32_t run, uint32_t passes)
{
	auditFds("enumeration_before_instance", run);
	const auto instance = Instance::open();
	auditFds("enumeration_instance_reused", run);
	std::vector<DeviceInfo> result;
	for (uint32_t pass = 0; pass < passes; ++pass) {
		auditFds("enumeration_pass_begin", pass);
		const auto physicals = instance->physicalDevices();
		auditFds("enumeration_physicals_observed", pass);
		result.clear();
		for (const auto physical : physicals) result.push_back(deviceInfo(physical));
		auditFds("enumeration_pass_end", pass);
	}
	return result;
}

std::shared_ptr<Context> Context::open(const DeviceIdentity& identity)
{
	auto c = std::shared_ptr<Context>(new Context);
	c->owner = Device::open(identity);
	c->physical = c->owner->_physical;
	c->device = c->owner->_device;
	c->queue = c->owner->_queue;
	c->family = c->owner->_family;
	c->info = deviceInfo(c->physical);
	vkGetPhysicalDeviceMemoryProperties(c->physical, &c->memory);
	c->importSemaphore = c->owner->_importSemaphoreFd;
	return c;
}

bool Context::lost() const { return owner->_lost.load(); }
void Context::markLost() { owner->_lost.store(true); }
VkResult Context::submit(const VkSubmitInfo& info, VkFence fence)
{
	std::lock_guard<std::mutex> lock(owner->_queueMutex);
	if (lost()) return VK_ERROR_DEVICE_LOST;
	const auto result = vkQueueSubmit(queue, 1, &info, fence);
	if (result == VK_ERROR_DEVICE_LOST) markLost();
	return result;
}

Image::~Image()
{
	if (ready) vkDestroySemaphore(context->device, ready, nullptr);
	if (released) vkDestroySemaphore(context->device, released, nullptr);
	if (image) vkDestroyImage(context->device, image, nullptr);
	if (memory) vkFreeMemory(context->device, memory, nullptr);
}
std::shared_ptr<Image> Image::import(std::shared_ptr<Context> c, const Descriptor& d, NativeFd memoryFd, NativeFd readyFd, NativeFd releasedFd)
{
	validateDescriptor(d); validateHardwarePair(d.identity, c->info.identity, c->info.properties.deviceType);
	VkPhysicalDeviceExternalImageFormatInfo externalQuery{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO}; externalQuery.handleType = d.memoryHandle;
	VkPhysicalDeviceImageFormatInfo2 query{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2}; query.pNext = &externalQuery;
	query.format = d.image.format; query.type = d.image.type; query.tiling = d.image.tiling; query.usage = d.image.usage; query.flags = d.image.flags;
	VkExternalImageFormatProperties externalProperties{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};
	VkImageFormatProperties2 properties{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2}; properties.pNext = &externalProperties;
	checked(vkGetPhysicalDeviceImageFormatProperties2(c->physical, &query, &properties), "probe exact external image query");
	require((externalProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) &&
		(externalProperties.externalMemoryProperties.compatibleHandleTypes & d.memoryHandle), "probe exact memory import unsupported");
	require(d.image.width <= properties.imageFormatProperties.maxExtent.width && d.image.height <= properties.imageFormatProperties.maxExtent.height &&
		properties.imageFormatProperties.maxExtent.depth >= 1 && properties.imageFormatProperties.maxMipLevels >= d.image.mipLevels &&
		properties.imageFormatProperties.maxArrayLayers >= d.image.arrayLayers && (properties.imageFormatProperties.sampleCounts & d.image.samples),
		"probe image dimensions/sample count unsupported");
	VkPhysicalDeviceExternalSemaphoreInfo semaphoreQuery{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO}; semaphoreQuery.handleType = d.semaphoreHandle;
	VkExternalSemaphoreProperties semaphoreProperties{VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES};
	vkGetPhysicalDeviceExternalSemaphoreProperties(c->physical, &semaphoreQuery, &semaphoreProperties);
	require((semaphoreProperties.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT) &&
		(semaphoreProperties.compatibleHandleTypes & d.semaphoreHandle), "probe exact semaphore import unsupported");
	auto result = std::make_shared<Image>(); result->context = std::move(c); result->descriptor = d;
	VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO}; external.handleTypes = d.memoryHandle;
	VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; info.pNext = &external; info.flags = d.image.flags;
	info.imageType = d.image.type; info.format = d.image.format; info.extent = {d.image.width, d.image.height, 1};
	info.mipLevels = d.image.mipLevels; info.arrayLayers = d.image.arrayLayers; info.samples = d.image.samples;
	info.tiling = d.image.tiling; info.usage = d.image.usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	const auto device = result->context->device;
	checked(vkCreateImage(device, &info, nullptr, &result->image), "probe identical imported image");
	VkMemoryRequirements requirements; vkGetImageMemoryRequirements(device, result->image, &requirements);
	const auto& memory = result->context->memory;
	require(requirements.size == d.allocationSize && requirements.alignment == d.alignment && d.memoryType < memory.memoryTypeCount &&
		(requirements.memoryTypeBits & (uint32_t(1) << d.memoryType)) &&
		d.allocationSize <= memory.memoryHeaps[memory.memoryTypes[d.memoryType].heapIndex].size, "probe import requirements mismatch");
	const auto memoryFlags = fcntl(memoryFd.get(), F_GETFD);
	require(memoryFd.get() >= 0 && memoryFlags >= 0 && (memoryFlags & FD_CLOEXEC), "probe memory FD ownership invalid");
	VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO}; dedicated.image = result->image;
	VkImportMemoryFdInfoKHR imported{VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR}; imported.pNext = &dedicated; imported.handleType = d.memoryHandle; imported.fd = memoryFd.get();
	VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocate.pNext = &imported; allocate.allocationSize = d.allocationSize; allocate.memoryTypeIndex = d.memoryType;
	const auto allocated = vkAllocateMemory(device, &allocate, nullptr, &result->memory);
	if (allocated == VK_SUCCESS) memoryFd.release();
	checked(allocated, "probe real opaque memory import");
	checked(vkBindImageMemory(device, result->image, result->memory, d.offset), "probe bind imported image");
	auto semaphore = [&](NativeFd fd, VkSemaphore& out) {
		if (fd.get() < 0) return;
		const auto flags = fcntl(fd.get(), F_GETFD);
		require(flags >= 0 && (flags & FD_CLOEXEC), "probe semaphore FD ownership invalid");
		VkSemaphoreCreateInfo create{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; checked(vkCreateSemaphore(device, &create, nullptr, &out), "probe create imported semaphore");
		VkImportSemaphoreFdInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR}; import.semaphore = out; import.handleType = d.semaphoreHandle; import.fd = fd.get();
		const auto status = result->context->importSemaphore(device, &import); if (status == VK_SUCCESS) fd.release(); checked(status, "probe real opaque semaphore import");
	};
	semaphore(std::move(readyFd), result->ready); semaphore(std::move(releasedFd), result->released); return result;
}

Buffer::~Buffer()
{
	if (mapped) vkUnmapMemory(context->device, memory);
	if (buffer) vkDestroyBuffer(context->device, buffer, nullptr);
	if (memory) vkFreeMemory(context->device, memory, nullptr);
}
std::shared_ptr<Buffer> Buffer::create(std::shared_ptr<Context> c, size_t size, VkBufferUsageFlags usage)
{
	auto b = std::make_shared<Buffer>(); b->context = std::move(c);
	VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; info.size = size; info.usage = usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	checked(vkCreateBuffer(b->context->device, &info, nullptr, &b->buffer), "probe staging buffer");
	VkMemoryRequirements r; vkGetBufferMemoryRequirements(b->context->device, b->buffer, &r);
	uint32_t type = b->context->memory.memoryTypeCount;
	for (bool coherent : {true, false}) {
		for (uint32_t i = 0; i < b->context->memory.memoryTypeCount; ++i) {
			const auto flags = b->context->memory.memoryTypes[i].propertyFlags;
			if ((r.memoryTypeBits & (uint32_t(1) << i)) && (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
				!(flags & (VK_MEMORY_PROPERTY_PROTECTED_BIT | VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)) &&
				(!coherent || (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) { type = i; break; }
		}
		if (type < b->context->memory.memoryTypeCount) break;
	}
	require(type < b->context->memory.memoryTypeCount, "probe staging memory unavailable");
	b->coherent = b->context->memory.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	b->allocationSize = r.size;
	VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocate.allocationSize = r.size; allocate.memoryTypeIndex = type;
	checked(vkAllocateMemory(b->context->device, &allocate, nullptr, &b->memory), "probe staging memory");
	checked(vkBindBufferMemory(b->context->device, b->buffer, b->memory, 0), "probe bind staging");
	checked(vkMapMemory(b->context->device, b->memory, 0, VK_WHOLE_SIZE, 0, &b->mapped), "probe map staging"); return b;
}
void Buffer::flush() {
	if (coherent) return;
	VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory = memory; range.size = VK_WHOLE_SIZE;
	checked(vkFlushMappedMemoryRanges(context->device, 1, &range), "probe flush whole allocation");
}
void Buffer::invalidate() {
	if (coherent) return;
	VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory = memory; range.size = VK_WHOLE_SIZE;
	checked(vkInvalidateMappedMemoryRanges(context->device, 1, &range), "probe invalidate whole allocation");
}
}
