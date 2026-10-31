/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "surface.hpp"
#include <atomic>
#include <mutex>

namespace Darling::Surface {

struct Capabilities {
	VkExternalMemoryProperties memory{};
	VkExternalSemaphoreProperties semaphore{VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES};
	VkImageFormatProperties image{};
};

enum class RetirementState { Unsubmitted, Pending, Complete, DeviceLost };
struct RetirementResult {
	RetirementState state;
	VkResult result;
};

constexpr size_t RetirementCapacity = 32;
size_t pendingRetirements();
void pollRetirements();

class Device : public std::enable_shared_from_this<Device> {
public:
	static std::shared_ptr<Device> open(const DeviceIdentity& required);
	~Device();
	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;
	const DeviceIdentity& identity() const { return _identity; }
	Capabilities query(const ImageSpec& image) const;
private:
	Device() = default;
	friend class Allocation;
	friend class RetirementTicket;
	void check(VkResult result, const char* operation);
	VkInstance _instance = VK_NULL_HANDLE;
	VkPhysicalDevice _physical = VK_NULL_HANDLE;
	VkDevice _device = VK_NULL_HANDLE;
	DeviceIdentity _identity;
	VkPhysicalDeviceType _type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
	uint32_t _family = 0;
	VkQueue _queue = VK_NULL_HANDLE;
	PFN_vkGetMemoryFdKHR _getMemoryFd = nullptr;
	PFN_vkGetSemaphoreFdKHR _getSemaphoreFd = nullptr;
	PFN_vkImportSemaphoreFdKHR _importSemaphoreFd = nullptr;
	std::mutex _queueMutex;
	std::atomic_bool _lost{false};
};

// Preallocated and registered before submission. The explicit bounded registry
// retains pending tickets even when an error/caller reference is dropped.
class RetirementTicket : public std::enable_shared_from_this<RetirementTicket> {
public:
	~RetirementTicket();
	RetirementResult retry(); // one nonblocking vkGetFenceStatus, never waits
	VkResult originalFailure() const { return _initialFailure; }
private:
	friend class Allocation;
	explicit RetirementTicket(std::shared_ptr<Device> device) : _device(std::move(device)) {}
	void cleanup();
	std::shared_ptr<Device> _device;
	VkCommandPool _pool = VK_NULL_HANDLE;
	VkFence _fence = VK_NULL_HANDLE;
	VkImage _image = VK_NULL_HANDLE;
	VkDeviceMemory _memory = VK_NULL_HANDLE;
	VkSemaphore _ready = VK_NULL_HANDLE, _released = VK_NULL_HANDLE;
	RetirementState _state = RetirementState::Unsubmitted;
	VkResult _result = VK_NOT_READY;
	VkResult _initialFailure = VK_SUCCESS;
	std::mutex _mutex;
};

// Storage only. Import never transitions/clears populated image data. No public
// rendering access is exposed until submission/ownership tracking is integrated.
class Allocation {
public:
	static std::unique_ptr<Allocation> create(std::shared_ptr<Device>, const BrokerBinding&, const ImageSpec&, uint64_t initializationTimeoutNs);
	static std::unique_ptr<Allocation> import(std::shared_ptr<Device>, Transfer transfer);
	~Allocation();
	Allocation(const Allocation&) = delete;
	Allocation& operator=(const Allocation&) = delete;
	const Descriptor& descriptor() const { return _descriptor; }
	Transfer exportTransfer(const BrokerBinding& recipient);
private:
	explicit Allocation(std::shared_ptr<Device> device) : _device(std::move(device)) {}
	void createImage();
	void createSemaphores();
	void initialize(uint64_t timeoutNs);
	std::shared_ptr<Device> _device;
	Descriptor _descriptor;
	VkImage _image = VK_NULL_HANDLE;
	VkDeviceMemory _memory = VK_NULL_HANDLE;
	VkSemaphore _ready = VK_NULL_HANDLE, _released = VK_NULL_HANDLE;
	bool _exportable = false;
	bool _transferIssued = false;
	std::mutex _exportMutex;
};

}
