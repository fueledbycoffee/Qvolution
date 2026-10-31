/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vulkan.hpp"
#include "retirement.private.hpp"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <vector>
#include <utility>

namespace Darling::Surface {

static void checked(VkResult result, const char* operation)
{
	if (result != VK_SUCCESS) throw Failure(Error::Vulkan, operation, result);
}

std::shared_ptr<Device> Device::open(const DeviceIdentity& required)
{
	auto result = std::shared_ptr<Device>(new Device);
	VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
	application.pApplicationName = "Darling native surface";
	application.apiVersion = VK_API_VERSION_1_1;
	VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	instance.pApplicationInfo = &application;
	checked(vkCreateInstance(&instance, nullptr, &result->_instance), "create native Vulkan instance");
	uint32_t count = 0;
	checked(vkEnumeratePhysicalDevices(result->_instance, &count, nullptr), "count Vulkan devices");
	std::vector<VkPhysicalDevice> physicals(count);
	checked(vkEnumeratePhysicalDevices(result->_instance, &count, physicals.data()), "enumerate Vulkan devices");
	for (uint32_t i = 0; i < count; ++i) {
		VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
		VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		properties.pNext = &ids;
		vkGetPhysicalDeviceProperties2(physicals[i], &properties);
		DeviceIdentity actual;
		std::copy_n(ids.deviceUUID, VK_UUID_SIZE, actual.device.begin());
		std::copy_n(ids.driverUUID, VK_UUID_SIZE, actual.driver.begin());
		if (actual.device != required.device || actual.driver != required.driver) continue;
		validateHardwarePair(required, actual, properties.properties.deviceType);
		if (properties.properties.apiVersion < VK_API_VERSION_1_1)
			throw Failure(Error::MissingExtension, "Vulkan 1.1 external-object contracts required");
		result->_physical = physicals[i]; result->_identity = actual; result->_type = properties.properties.deviceType;
		break;
	}
	if (!result->_physical) throw Failure(Error::UnsupportedDevice, "required Vulkan device/driver UUID pair unavailable");
	checked(vkEnumerateDeviceExtensionProperties(result->_physical, nullptr, &count, nullptr), "count device extensions");
	std::vector<VkExtensionProperties> extensions(count);
	checked(vkEnumerateDeviceExtensionProperties(result->_physical, nullptr, &count, extensions.data()), "enumerate device extensions");
	const char* requiredExtensions[] = {VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME};
	for (const auto name : requiredExtensions) {
		if (std::none_of(extensions.begin(), extensions.begin() + count,
			[name](const auto& extension) { return strcmp(extension.extensionName, name) == 0; }))
			throw Failure(Error::MissingExtension, "native opaque-FD extension unavailable");
	}
	vkGetPhysicalDeviceQueueFamilyProperties(result->_physical, &count, nullptr);
	std::vector<VkQueueFamilyProperties> families(count);
	vkGetPhysicalDeviceQueueFamilyProperties(result->_physical, &count, families.data());
	const auto family = std::find_if(families.begin(), families.begin() + count, [](const auto& f) {
		return f.queueCount && (f.queueFlags & VK_QUEUE_GRAPHICS_BIT);
	});
	if (family == families.begin() + count) throw Failure(Error::UnsupportedDevice, "graphics queue unavailable");
	result->_family = uint32_t(family - families.begin());
	const float priority = 1;
	VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	queue.queueFamilyIndex = result->_family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
	VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	device.queueCreateInfoCount = 1; device.pQueueCreateInfos = &queue;
	device.enabledExtensionCount = 2; device.ppEnabledExtensionNames = requiredExtensions;
	checked(vkCreateDevice(result->_physical, &device, nullptr, &result->_device), "create native Vulkan device");
	vkGetDeviceQueue(result->_device, result->_family, 0, &result->_queue);
	result->_getMemoryFd = reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(result->_device, "vkGetMemoryFdKHR"));
	result->_getSemaphoreFd = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(result->_device, "vkGetSemaphoreFdKHR"));
	result->_importSemaphoreFd = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(vkGetDeviceProcAddr(result->_device, "vkImportSemaphoreFdKHR"));
	if (!result->_getMemoryFd || !result->_getSemaphoreFd || !result->_importSemaphoreFd)
		throw Failure(Error::MissingExtension, "native external-object entry point unresolved");
	return result;
}

Device::~Device()
{
	if (_device) vkDestroyDevice(_device, nullptr);
	if (_instance) vkDestroyInstance(_instance, nullptr);
}

Capabilities Device::query(const ImageSpec& image) const
{
	if (_lost.load()) throw Failure(Error::Vulkan, "native device context lost", VK_ERROR_DEVICE_LOST);
	validateImageSpec(image);
	VkPhysicalDeviceExternalImageFormatInfo external{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO};
	external.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
	VkPhysicalDeviceImageFormatInfo2 info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
	info.pNext = &external; info.format = image.format; info.type = image.type;
	info.tiling = image.tiling; info.usage = image.usage; info.flags = image.flags;
	VkExternalImageFormatProperties externalProperties{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};
	VkImageFormatProperties2 properties{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};
	properties.pNext = &externalProperties;
	const auto result = vkGetPhysicalDeviceImageFormatProperties2(_physical, &info, &properties);
	if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) throw Failure(Error::UnsupportedFormat, "opaque-FD image combination unsupported", result);
	checked(result, "query external image combination");
	Capabilities caps;
	caps.memory = externalProperties.externalMemoryProperties; caps.image = properties.imageFormatProperties;
	const VkExternalMemoryFeatureFlags features = VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
	if ((caps.memory.externalMemoryFeatures & features) != features ||
		!(caps.memory.compatibleHandleTypes & external.handleType) ||
		image.width > caps.image.maxExtent.width || image.height > caps.image.maxExtent.height ||
		caps.image.maxExtent.depth < 1 || caps.image.maxMipLevels < image.mipLevels ||
		caps.image.maxArrayLayers < image.arrayLayers || !(caps.image.sampleCounts & image.samples))
		throw Failure(Error::UnsupportedFormat, "image external features or dimensions unavailable");
	VkPhysicalDeviceExternalSemaphoreInfo semaphore{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO};
	semaphore.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
	vkGetPhysicalDeviceExternalSemaphoreProperties(_physical, &semaphore, &caps.semaphore);
	const VkExternalSemaphoreFeatureFlags semaphoreFeatures = VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;
	if ((caps.semaphore.externalSemaphoreFeatures & semaphoreFeatures) != semaphoreFeatures ||
		!(caps.semaphore.compatibleHandleTypes & semaphore.handleType))
		throw Failure(Error::MissingExtension, "binary opaque-FD semaphore combination unsupported");
	return caps;
}

void Allocation::createImage()
{
	const auto& s = _descriptor.image;
	VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
	external.handleTypes = _descriptor.memoryHandle;
	VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	image.pNext = &external; image.flags = s.flags; image.imageType = s.type; image.format = s.format;
	image.extent = {s.width, s.height, 1}; image.mipLevels = s.mipLevels; image.arrayLayers = s.arrayLayers;
	image.samples = s.samples; image.tiling = s.tiling; image.usage = s.usage;
	image.sharingMode = VK_SHARING_MODE_EXCLUSIVE; image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	_device->check(vkCreateImage(_device->_device, &image, nullptr, &_image), "create external image");
}

void Allocation::createSemaphores()
{
	VkExportSemaphoreCreateInfo external{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
	external.handleTypes = _descriptor.semaphoreHandle;
	VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	semaphore.pNext = _exportable ? &external : nullptr;
	_device->check(vkCreateSemaphore(_device->_device, &semaphore, nullptr, &_ready), "create ready binary semaphore");
	_device->check(vkCreateSemaphore(_device->_device, &semaphore, nullptr, &_released), "create released binary semaphore");
}

std::unique_ptr<Allocation> Allocation::create(std::shared_ptr<Device> device, const BrokerBinding& binding, const ImageSpec& image, uint64_t timeoutNs)
{
	if (!device) throw Failure(Error::UnsupportedDevice, "native device required");
	if (timeoutNs == UINT64_MAX) throw Failure(Error::InvalidDescriptor, "finite initialization timeout required");
	validateBinding(binding);
	auto caps = device->query(image);
	auto allocation = std::unique_ptr<Allocation>(new Allocation(std::move(device)));
	allocation->_exportable = true;
	allocation->_descriptor.binding = binding; allocation->_descriptor.image = image;
	allocation->_descriptor.identity = allocation->_device->_identity;
	allocation->createImage();
	VkMemoryDedicatedRequirements dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
	VkMemoryRequirements2 requirements{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
	requirements.pNext = &dedicated;
	VkImageMemoryRequirementsInfo2 request{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
	request.image = allocation->_image;
	vkGetImageMemoryRequirements2(allocation->_device->_device, &request, &requirements);
	const auto& r = requirements.memoryRequirements;
	VkPhysicalDeviceMemoryProperties memory;
	vkGetPhysicalDeviceMemoryProperties(allocation->_device->_physical, &memory);
	uint32_t type = 0;
	for (; type < memory.memoryTypeCount; ++type) {
		const auto& candidate = memory.memoryTypes[type];
		if ((r.memoryTypeBits & (uint32_t(1) << type)) && (candidate.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) &&
			!(candidate.propertyFlags & (VK_MEMORY_PROPERTY_PROTECTED_BIT | VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)) &&
			r.size <= memory.memoryHeaps[candidate.heapIndex].size) break;
	}
	if (type == memory.memoryTypeCount || r.size > caps.image.maxResourceSize)
		throw Failure(Error::UnsupportedFormat, "compatible device-local memory unavailable");
	allocation->_descriptor.memoryType = type; allocation->_descriptor.allocationSize = r.size;
	allocation->_descriptor.alignment = r.alignment;
	validateDescriptor(allocation->_descriptor);
	VkMemoryDedicatedAllocateInfo dedicatedInfo{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
	dedicatedInfo.image = allocation->_image;
	VkExportMemoryAllocateInfo external{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
	external.pNext = &dedicatedInfo; external.handleTypes = allocation->_descriptor.memoryHandle;
	VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocate.pNext = &external; allocate.allocationSize = r.size; allocate.memoryTypeIndex = type;
	allocation->_device->check(vkAllocateMemory(allocation->_device->_device, &allocate, nullptr, &allocation->_memory), "allocate exportable dedicated memory");
	allocation->_device->check(vkBindImageMemory(allocation->_device->_device, allocation->_image, allocation->_memory, 0), "bind exportable image memory");
	allocation->createSemaphores();
	allocation->initialize(timeoutNs);
	return allocation;
}

void Allocation::initialize(uint64_t timeoutNs)
{
	const auto device = _device->_device;
	auto ticket = std::shared_ptr<RetirementTicket>(new RetirementTicket(_device));
	std::unique_lock<std::mutex> ticketLock(ticket->_mutex);
	VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	poolInfo.queueFamilyIndex = _device->_family; poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	_device->check(vkCreateCommandPool(device, &poolInfo, nullptr, &ticket->_pool), "create initialization command pool");
	VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	_device->check(vkCreateFence(device, &fence, nullptr, &ticket->_fence), "create initialization retirement fence");
	registerRetirement(ticket);
	try {
		VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		allocate.commandPool = ticket->_pool; allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount = 1;
		VkCommandBuffer command;
		_device->check(vkAllocateCommandBuffers(device, &allocate, &command), "allocate initialization command buffer");
		VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		_device->check(vkBeginCommandBuffer(command, &begin), "begin image initialization");
		VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = _image; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		const VkClearColorValue clear{};
		vkCmdClearColorImage(command, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &barrier.subresourceRange);
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = 0;
		barrier.srcQueueFamilyIndex = _device->_family; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
		vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		_device->check(vkEndCommandBuffer(command), "end image initialization");
		VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
		submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &_ready;
		std::lock_guard<std::mutex> lock(_device->_queueMutex);
		_device->check(VK_SUCCESS, "native context before submit");
		const auto submitted = vkQueueSubmit(_device->_queue, 1, &submit, ticket->_fence);
		if (submitted == VK_ERROR_OUT_OF_HOST_MEMORY || submitted == VK_ERROR_OUT_OF_DEVICE_MEMORY || submitted == VK_ERROR_DEVICE_LOST)
			_device->check(submitted, "submit initialized external image");
		ticket->_image = std::exchange(_image, VK_NULL_HANDLE);
		ticket->_memory = std::exchange(_memory, VK_NULL_HANDLE);
		ticket->_ready = std::exchange(_ready, VK_NULL_HANDLE);
		ticket->_released = std::exchange(_released, VK_NULL_HANDLE);
		ticket->_state = RetirementState::Pending;
		if (submitted != VK_SUCCESS) {
			ticket->_initialFailure = submitted;
			throw Failure(Error::Vulkan, "unexpected submission result leaves retirement unresolved", submitted, ticket);
		}
		const auto completed = vkWaitForFences(device, 1, &ticket->_fence, VK_TRUE, timeoutNs);
		ticket->_result = completed;
		if (completed != VK_SUCCESS && completed != VK_ERROR_DEVICE_LOST) {
			ticket->_initialFailure = completed;
			throw Failure(completed == VK_TIMEOUT ? Error::Timeout : Error::Vulkan,
				"external image initialization remains pending", completed, ticket);
		}
		ticket->_state = completed == VK_SUCCESS ? RetirementState::Complete : RetirementState::DeviceLost;
		if (completed == VK_SUCCESS) {
			_image = std::exchange(ticket->_image, VK_NULL_HANDLE);
			_memory = std::exchange(ticket->_memory, VK_NULL_HANDLE);
			_ready = std::exchange(ticket->_ready, VK_NULL_HANDLE);
			_released = std::exchange(ticket->_released, VK_NULL_HANDLE);
		}
		ticket->cleanup();
		unregisterRetirement(ticket);
		_device->check(completed, "finish initialized external image");
	} catch (...) {
		if (ticket->_state != RetirementState::Pending) unregisterRetirement(ticket);
		throw;
	}
}

std::unique_ptr<Allocation> Allocation::import(std::shared_ptr<Device> device, Transfer transfer)
{
	if (!device) throw Failure(Error::UnsupportedDevice, "native device required");
	validateDescriptor(transfer.descriptor);
	validateHardwarePair(device->_identity, transfer.descriptor.identity, device->_type);
	device->query(transfer.descriptor.image);
	for (int fd : {transfer.memory.get(), transfer.ready.get(), transfer.released.get()}) {
		const int flags = fcntl(fd, F_GETFD);
		if (flags < 0 || !(flags & FD_CLOEXEC)) throw Failure(Error::Transport, "owned CLOEXEC import descriptors required");
	}
	auto allocation = std::unique_ptr<Allocation>(new Allocation(std::move(device)));
	allocation->_descriptor = transfer.descriptor;
	allocation->createImage();
	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(allocation->_device->_device, allocation->_image, &requirements);
	VkPhysicalDeviceMemoryProperties properties;
	vkGetPhysicalDeviceMemoryProperties(allocation->_device->_physical, &properties);
	const auto& d = allocation->_descriptor;
	if (requirements.size != d.allocationSize || requirements.alignment != d.alignment ||
		d.memoryType >= properties.memoryTypeCount || !(requirements.memoryTypeBits & (uint32_t(1) << d.memoryType)) ||
		d.allocationSize > properties.memoryHeaps[properties.memoryTypes[d.memoryType].heapIndex].size)
		throw Failure(Error::InvalidDescriptor, "import metadata differs from image memory requirements");
	VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
	dedicated.image = allocation->_image;
	VkImportMemoryFdInfoKHR external{VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR};
	external.pNext = &dedicated; external.handleType = d.memoryHandle; external.fd = transfer.memory.get();
	VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	memory.pNext = &external; memory.allocationSize = d.allocationSize; memory.memoryTypeIndex = d.memoryType;
	const auto importedMemory = vkAllocateMemory(allocation->_device->_device, &memory, nullptr, &allocation->_memory);
	if (importedMemory == VK_SUCCESS) transfer.memory.release();
	allocation->_device->check(importedMemory, "import dedicated opaque-FD memory");
	allocation->_device->check(vkBindImageMemory(allocation->_device->_device, allocation->_image, allocation->_memory, 0), "bind imported memory");
	allocation->createSemaphores();
	auto importSemaphore = [&](VkSemaphore semaphore, NativeFd& fd) {
		VkImportSemaphoreFdInfoKHR info{VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR};
		info.semaphore = semaphore; info.handleType = d.semaphoreHandle; info.fd = fd.get();
		const auto result = allocation->_device->_importSemaphoreFd(allocation->_device->_device, &info);
		if (result == VK_SUCCESS) fd.release();
		allocation->_device->check(result, "import binary opaque-FD semaphore");
	};
	importSemaphore(allocation->_ready, transfer.ready);
	importSemaphore(allocation->_released, transfer.released);
	return allocation;
}

Transfer Allocation::exportTransfer(const BrokerBinding& recipient)
{
	std::lock_guard<std::mutex> lock(_exportMutex);
	validateBinding(recipient);
	if (!_exportable || _transferIssued) throw Failure(Error::InvalidState, "initial binary handoff already issued or not exportable");
	const auto& origin = _descriptor.binding;
	if (recipient.object != origin.object || recipient.generation != origin.generation || recipient.prefix != origin.prefix)
		throw Failure(Error::BindingMismatch, "recipient grant identifies another allocation");
	Transfer result;
	result.descriptor = _descriptor; result.descriptor.binding = recipient;
	auto owned = [](int fd) {
		NativeFd owner(fd);
		const int flags = fcntl(fd, F_GETFD);
		if (flags < 0 || !(flags & FD_CLOEXEC)) throw Failure(Error::Transport, "driver export lacks native CLOEXEC ownership");
		return owner;
	};
	VkMemoryGetFdInfoKHR memory{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
	memory.memory = _memory; memory.handleType = _descriptor.memoryHandle;
	int fd = -1;
	const auto exportedMemory = _device->_getMemoryFd(_device->_device, &memory, &fd);
	if (exportedMemory == VK_SUCCESS) result.memory = owned(fd);
	_device->check(exportedMemory, "export image memory FD");
	auto exportSemaphore = [&](VkSemaphore semaphore) {
		VkSemaphoreGetFdInfoKHR info{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
		info.semaphore = semaphore; info.handleType = _descriptor.semaphoreHandle;
		int handle = -1;
		const auto exported = _device->_getSemaphoreFd(_device->_device, &info, &handle);
		NativeFd owner;
		if (exported == VK_SUCCESS) owner = owned(handle);
		_device->check(exported, "export binary semaphore FD");
		return owner;
	};
	result.ready = exportSemaphore(_ready); result.released = exportSemaphore(_released);
	_transferIssued = true;
	return result;
}

Allocation::~Allocation()
{
	const auto device = _device->_device;
	if (_ready) vkDestroySemaphore(device, _ready, nullptr);
	if (_released) vkDestroySemaphore(device, _released, nullptr);
	if (_image) vkDestroyImage(device, _image, nullptr);
	if (_memory) vkFreeMemory(device, _memory, nullptr);
}

}
