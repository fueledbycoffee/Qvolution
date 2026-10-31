/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fake-vulkan.hpp"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <map>
#include <type_traits>

namespace {
unsigned operation = 0, failure = 0, initialized = 0;
uint64_t nextHandle = 100;
std::map<uint64_t, int> live;
uint32_t typeBits = 1;
VkPhysicalDeviceType physicalType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
bool hasExtensions = true;
VkResult waitFailure = VK_ERROR_OUT_OF_HOST_MEMORY;
VkResult fenceResult = VK_SUCCESS;
VkResult submissionFailure = VK_ERROR_OUT_OF_DEVICE_MEMORY;
unsigned submissionOperation = 0;
std::map<uint64_t, uint64_t> commandPools, fencePools;
std::map<uint64_t, bool> pendingPools;
VkExternalMemoryFeatureFlags memoryFlags = VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT;
VkExternalSemaphoreFeatureFlags semaphoreFlags = VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT;
bool failed() { return ++operation == failure; }
template<class T> uint64_t key(T value) { if constexpr (std::is_pointer_v<T>) return reinterpret_cast<uintptr_t>(value); else return value; }
template<class T> T handle(uint64_t value) { if constexpr (std::is_pointer_v<T>) return reinterpret_cast<T>(value); else return T(value); }
template<class T> VkResult create(T* result) {
	if (failed()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	const auto value = ++nextHandle; live.emplace(value, -1); *result = handle<T>(value); return VK_SUCCESS;
}
template<class T> void destroy(T value) {
	auto found = live.find(key(value));
	if (found == live.end()) std::terminate();
	if (found->second >= 0) close(found->second);
	live.erase(found);
}
int newFd() { return memfd_create("fake-vulkan-control-only", MFD_CLOEXEC); }
}

namespace FakeVulkan {
void reset() {
	if (!live.empty()) std::terminate();
	operation = failure = initialized = 0; typeBits = 1; hasExtensions = true;
	waitFailure = VK_ERROR_OUT_OF_HOST_MEMORY; fenceResult = VK_SUCCESS;
	submissionFailure = VK_ERROR_OUT_OF_DEVICE_MEMORY; submissionOperation = 0;
	commandPools.clear(); fencePools.clear(); pendingPools.clear();
	physicalType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
	memoryFlags = VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT;
	semaphoreFlags = VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT;
}
void failAt(unsigned index) { operation = 0; failure = index; }
unsigned operations() { return operation; }
unsigned liveObjects() { return live.size(); }
unsigned initializationCount() { return initialized; }
void memoryTypeBits(uint32_t bits) { typeBits = bits; }
void imageFeatures(VkExternalMemoryFeatureFlags flags) { memoryFlags = flags; }
void semaphoreFeatures(VkExternalSemaphoreFeatureFlags flags) { semaphoreFlags = flags; }
void deviceType(VkPhysicalDeviceType type) { physicalType = type; }
void extensions(bool enabled) { hasExtensions = enabled; }
void waitError(VkResult error) { waitFailure = error; }
void fenceStatus(VkResult status) { fenceResult = status; }
void submitError(VkResult error) { submissionFailure = error; }
unsigned submitOperation() { return submissionOperation; }
Darling::Surface::DeviceIdentity identity() {
	Darling::Surface::DeviceIdentity id; id.device.fill(7); id.driver.fill(8); return id;
}
}

extern "C" {
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance* out) { return create(out); }
VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance value, const VkAllocationCallbacks*) { destroy(value); }
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance, uint32_t* count, VkPhysicalDevice* out) {
	if (failed()) return VK_ERROR_OUT_OF_HOST_MEMORY;
	*count = 1; if (out) *out = handle<VkPhysicalDevice>(1); return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2(VkPhysicalDevice, VkPhysicalDeviceProperties2* out) {
	out->properties.apiVersion = VK_API_VERSION_1_1; out->properties.deviceType = physicalType;
	auto ids = static_cast<VkPhysicalDeviceIDProperties*>(out->pNext);
	memset(ids->deviceUUID, 7, VK_UUID_SIZE); memset(ids->driverUUID, 8, VK_UUID_SIZE);
}
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice, const char*, uint32_t* count, VkExtensionProperties* out) {
	if (failed()) return VK_ERROR_OUT_OF_HOST_MEMORY;
	*count = hasExtensions ? 2 : 0;
	if (out && hasExtensions) {
		strcpy(out[0].extensionName, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
		strcpy(out[1].extensionName, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
	}
	return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice, uint32_t* count, VkQueueFamilyProperties* out) {
	*count = 1; if (out) { out->queueCount = 1; out->queueFlags = VK_QUEUE_GRAPHICS_BIT; }
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice* out) { return create(out); }
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice value, const VkAllocationCallbacks*) { destroy(value); }
VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(VkDevice, uint32_t, uint32_t, VkQueue* out) { *out = handle<VkQueue>(2); }
static VkResult VKAPI_CALL exportMemory(VkDevice, const VkMemoryGetFdInfoKHR*, int* out) {
	if (failed()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	*out = newFd(); return *out < 0 ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS;
}
static VkResult VKAPI_CALL exportSemaphore(VkDevice, const VkSemaphoreGetFdInfoKHR*, int* out) {
	if (failed()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	*out = newFd(); return *out < 0 ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS;
}
static VkResult VKAPI_CALL importSemaphore(VkDevice, const VkImportSemaphoreFdInfoKHR* info) {
	if (failed()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	live.at(key(info->semaphore)) = info->fd; return VK_SUCCESS;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice, const char* name) {
	if (failed()) return nullptr;
	if (!strcmp(name, "vkGetMemoryFdKHR")) return reinterpret_cast<PFN_vkVoidFunction>(exportMemory);
	if (!strcmp(name, "vkGetSemaphoreFdKHR")) return reinterpret_cast<PFN_vkVoidFunction>(exportSemaphore);
	if (!strcmp(name, "vkImportSemaphoreFdKHR")) return reinterpret_cast<PFN_vkVoidFunction>(importSemaphore);
	return nullptr;
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice, const VkPhysicalDeviceImageFormatInfo2*, VkImageFormatProperties2* out) {
	if (failed()) return VK_ERROR_FORMAT_NOT_SUPPORTED;
	out->imageFormatProperties = {{4096, 4096, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, 1ULL << 30};
	auto external = static_cast<VkExternalImageFormatProperties*>(out->pNext);
	external->externalMemoryProperties = {memoryFlags, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT};
	return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo*, VkExternalSemaphoreProperties* out) {
	out->externalSemaphoreFeatures = semaphoreFlags;
	out->compatibleHandleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage* out) { return create(out); }
VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice, VkImage value, const VkAllocationCallbacks*) { destroy(value); }
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(VkDevice, VkImage, VkMemoryRequirements* out) { *out = {4096, 4096, typeBits}; }
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2* info, VkMemoryRequirements2* out) {
	vkGetImageMemoryRequirements(device, info->image, &out->memoryRequirements);
	if (out->pNext) static_cast<VkMemoryDedicatedRequirements*>(out->pNext)->requiresDedicatedAllocation = VK_TRUE;
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* out) {
	*out = {}; out->memoryTypeCount = 1; out->memoryTypes[0] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0};
	out->memoryHeapCount = 1; out->memoryHeaps[0] = {1ULL << 30, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
}
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks*, VkDeviceMemory* out) {
	const auto result = create(out); if (result != VK_SUCCESS) return result;
	auto next = static_cast<const VkBaseInStructure*>(info->pNext);
	for (; next; next = next->pNext) if (next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR)
		live.at(key(*out)) = reinterpret_cast<const VkImportMemoryFdInfoKHR*>(next)->fd;
	return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice, VkDeviceMemory value, const VkAllocationCallbacks*) { destroy(value); }
VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize) { return failed() ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore* out) { return create(out); }
VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(VkDevice, VkSemaphore value, const VkAllocationCallbacks*) { destroy(value); }
VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool* out) { return create(out); }
VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice, VkCommandPool value, const VkAllocationCallbacks*) {
	if (pendingPools[key(value)]) std::terminate();
	pendingPools.erase(key(value));
	destroy(value);
}
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* out) {
	if (failed()) return VK_ERROR_OUT_OF_HOST_MEMORY;
	*out = handle<VkCommandBuffer>(++nextHandle); commandPools[key(*out)] = key(info->commandPool); return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return failed() ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer) { return failed() ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*, uint32_t, const VkImageMemoryBarrier*) {}
VKAPI_ATTR void VKAPI_CALL vkCmdClearColorImage(VkCommandBuffer, VkImage, VkImageLayout, const VkClearColorValue*, uint32_t, const VkImageSubresourceRange*) { ++initialized; }
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo* info, VkFence fence) {
	const bool fail = failed(); submissionOperation = operation;
	if (fail && submissionFailure != VK_ERROR_UNKNOWN) return submissionFailure;
	const auto pool = commandPools.at(key(info->pCommandBuffers[0]));
	fencePools[key(fence)] = pool; pendingPools[pool] = true;
	return fail ? submissionFailure : VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* out) { return create(out); }
VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice, VkFence value, const VkAllocationCallbacks*) { fencePools.erase(key(value)); destroy(value); }
VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t, const VkFence* fences, VkBool32, uint64_t) {
	const auto result = failed() ? waitFailure : VK_SUCCESS;
	if (result == VK_SUCCESS) pendingPools[fencePools.at(key(*fences))] = false;
	if (result == VK_ERROR_DEVICE_LOST) for (auto& entry : pendingPools) entry.second = false;
	return result;
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice, VkFence fence) {
	if (fenceResult == VK_SUCCESS) pendingPools[fencePools.at(key(fence))] = false;
	if (fenceResult == VK_ERROR_DEVICE_LOST) for (auto& entry : pendingPools) entry.second = false;
	return fenceResult;
}
}
