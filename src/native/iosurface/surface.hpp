/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

namespace Darling::Surface {

class RetirementTicket;

enum class Error {
	InvalidDescriptor, UnsupportedDevice, UnsupportedFormat, MissingExtension,
	Vulkan, System, Transport, PeerMismatch, BindingMismatch, InvalidState,
	Timeout, RetirementCapacity
};

class Failure : public std::runtime_error {
public:
	Failure(Error code, const char* message, int detail = 0, std::shared_ptr<RetirementTicket> retirement = {});
	const Error code;
	const int detail;
	const std::shared_ptr<RetirementTicket> retirement;
};

using UUID = std::array<uint8_t, VK_UUID_SIZE>;
struct DeviceIdentity {
	UUID device{}, driver{};
};

// Supplied and checked against genuine broker rights outside this native layer.
// These correlation values, including the opaque tag, are not capabilities.
struct BrokerBinding {
	std::array<uint8_t, 16> prefix{};
	std::array<uint8_t, 32> tag{};
	uint64_t object = 0, generation = 0, processGeneration = 0, lease = 0;
};

struct ImageSpec {
	uint32_t width = 0, height = 0;
	VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
	VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	VkImageCreateFlags flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	VkImageType type = VK_IMAGE_TYPE_2D;
	uint32_t mipLevels = 1, arrayLayers = 1;
	VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
};

// Version 1 is one dedicated Vulkan opaque-FD image and two binary semaphores.
// It deliberately does not describe DMA-BUF memory planes or Apple ABI.
struct Descriptor {
	BrokerBinding binding;
	DeviceIdentity identity;
	ImageSpec image;
	uint64_t allocationSize = 0, alignment = 0, offset = 0;
	uint32_t memoryType = 0, dedicated = 1;
	VkExternalMemoryHandleTypeFlagBits memoryHandle = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
	VkExternalSemaphoreHandleTypeFlagBits semaphoreHandle = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
	uint64_t epoch = 1;
	VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
	uint32_t owner = VK_QUEUE_FAMILY_EXTERNAL;
};

constexpr size_t WireSize = 224;
using WireDescriptor = std::array<uint8_t, WireSize>;
void validateBinding(const BrokerBinding& binding);
void validateImageSpec(const ImageSpec& image);
void validateDescriptor(const Descriptor& descriptor);
void validateHardwarePair(const DeviceIdentity& expected, const DeviceIdentity& actual, VkPhysicalDeviceType type);
WireDescriptor encode(const Descriptor& descriptor);
Descriptor decode(const WireDescriptor& wire);

class NativeFd {
public:
	NativeFd() = default;
	explicit NativeFd(int fd) noexcept : _fd(fd) {}
	~NativeFd();
	NativeFd(NativeFd&& other) noexcept;
	NativeFd& operator=(NativeFd&& other) noexcept;
	NativeFd(const NativeFd&) = delete;
	NativeFd& operator=(const NativeFd&) = delete;
	int get() const noexcept { return _fd; }
	int release() noexcept;
	NativeFd duplicate() const;
private:
	int _fd = -1;
};

struct Transfer {
	Transfer() = default;
	Transfer(Descriptor descriptor, NativeFd memory, NativeFd ready, NativeFd released) noexcept :
		descriptor(std::move(descriptor)), memory(std::move(memory)), ready(std::move(ready)), released(std::move(released)) {}
	Transfer(Transfer&&) noexcept = default;
	Transfer& operator=(Transfer&&) noexcept = default;
	Transfer(const Transfer&) = delete;
	Transfer& operator=(const Transfer&) = delete;
	Descriptor descriptor;
	NativeFd memory, ready, released;
};

struct ExpectedTransfer {
	Descriptor descriptor;
	uint32_t senderPid = 0, senderUid = 0, senderGid = 0;
};

// Connected native AF_UNIX/SOCK_SEQPACKET only; receiver needs SO_PASSCRED.
// Caller must independently obtain expected metadata from an audited broker grant.
// These APIs neither authenticate Mach rights nor translate guest descriptor tables.
// Sending consumes the bundle at entry on every outcome. No transport retry is
// permitted; a failed/ambiguous delivery requires broker retirement/recovery.
void sendTransfer(int nativeSocket, Transfer transfer);
Transfer receiveTransfer(int nativeSocket, const ExpectedTransfer& expected);

}
