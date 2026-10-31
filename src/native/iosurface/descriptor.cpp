/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "surface.hpp"
#include <algorithm>
#include <limits>

namespace Darling::Surface {

Failure::Failure(Error code, const char* message, int detail, std::shared_ptr<RetirementTicket> retirement) :
	std::runtime_error(message), code(code), detail(detail), retirement(std::move(retirement)) {}

template<size_t N> static bool empty(const std::array<uint8_t, N>& bytes)
{
	return std::all_of(bytes.begin(), bytes.end(), [](uint8_t b) { return b == 0; });
}

void validateBinding(const BrokerBinding& b)
{
	if (empty(b.prefix) || empty(b.tag) || !b.object || !b.generation || !b.processGeneration || !b.lease)
		throw Failure(Error::InvalidDescriptor, "incomplete broker binding");
}

void validateImageSpec(const ImageSpec& s)
{
	const ImageSpec subset;
	if (!s.width || !s.height || s.width > UINT64_MAX / s.height / 4 ||
		s.format != subset.format || s.usage != subset.usage || s.flags != subset.flags ||
		s.tiling != subset.tiling || s.type != subset.type || s.mipLevels != 1 ||
		s.arrayLayers != 1 || s.samples != subset.samples)
		throw Failure(Error::UnsupportedFormat, "outside dedicated BGRA8 2D subset");
}

void validateDescriptor(const Descriptor& d)
{
	validateBinding(d.binding);
	validateImageSpec(d.image);
	if (empty(d.identity.device) || empty(d.identity.driver) || !d.allocationSize ||
		!d.alignment || (d.alignment & (d.alignment - 1)) || d.offset || d.dedicated != 1 ||
		d.memoryType >= VK_MAX_MEMORY_TYPES ||
		d.allocationSize < uint64_t(d.image.width) * d.image.height * 4 ||
		d.memoryHandle != VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT ||
		d.semaphoreHandle != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT ||
		d.epoch != 1 || d.layout != VK_IMAGE_LAYOUT_GENERAL || d.owner != VK_QUEUE_FAMILY_EXTERNAL)
		throw Failure(Error::InvalidDescriptor, "invalid opaque-FD allocation metadata");
}

void validateHardwarePair(const DeviceIdentity& expected, const DeviceIdentity& actual, VkPhysicalDeviceType type)
{
	if (empty(expected.device) || empty(expected.driver) || expected.device != actual.device ||
		expected.driver != actual.driver ||
		(type != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && type != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU))
		throw Failure(Error::UnsupportedDevice, "hardware device/driver UUID pair required");
}

namespace {
struct Writer {
	WireDescriptor bytes{};
	size_t offset = 0;
	void u32(uint32_t v) { for (unsigned i = 0; i < 4; ++i) bytes.at(offset++) = uint8_t(v >> (8 * i)); }
	void u64(uint64_t v) { for (unsigned i = 0; i < 8; ++i) bytes.at(offset++) = uint8_t(v >> (8 * i)); }
	template<size_t N> void array(const std::array<uint8_t, N>& b) { for (auto v : b) bytes.at(offset++) = v; }
};
struct Reader {
	const WireDescriptor& bytes;
	size_t offset = 0;
	uint32_t u32() { uint32_t v = 0; for (unsigned i = 0; i < 4; ++i) v |= uint32_t(bytes.at(offset++)) << (8 * i); return v; }
	uint64_t u64() { uint64_t v = 0; for (unsigned i = 0; i < 8; ++i) v |= uint64_t(bytes.at(offset++)) << (8 * i); return v; }
	template<size_t N> void array(std::array<uint8_t, N>& b) { for (auto& v : b) v = bytes.at(offset++); }
};
}

WireDescriptor encode(const Descriptor& d)
{
	validateDescriptor(d);
	Writer w;
	w.u32(0x44534750); w.u32(1); w.u32(WireSize); w.u32(0);
	w.array(d.binding.prefix); w.array(d.binding.tag);
	w.u64(d.binding.object); w.u64(d.binding.generation); w.u64(d.binding.processGeneration); w.u64(d.binding.lease);
	w.array(d.identity.device); w.array(d.identity.driver);
	w.u32(d.image.width); w.u32(d.image.height); w.u32(d.image.format); w.u32(d.image.usage);
	w.u32(d.image.flags); w.u32(d.image.tiling); w.u32(d.image.type); w.u32(d.image.mipLevels);
	w.u32(d.image.arrayLayers); w.u32(d.image.samples);
	w.u64(d.allocationSize); w.u64(d.alignment); w.u64(d.offset);
	w.u32(d.memoryType); w.u32(d.dedicated); w.u32(d.memoryHandle); w.u32(d.semaphoreHandle);
	w.u64(d.epoch); w.u32(d.layout); w.u32(d.owner);
	if (w.offset != WireSize) throw Failure(Error::InvalidDescriptor, "wire encoder size mismatch");
	return w.bytes;
}

Descriptor decode(const WireDescriptor& wire)
{
	Reader r{wire};
	if (r.u32() != 0x44534750 || r.u32() != 1 || r.u32() != WireSize || r.u32())
		throw Failure(Error::InvalidDescriptor, "unknown descriptor wire version");
	Descriptor d;
	r.array(d.binding.prefix); r.array(d.binding.tag);
	d.binding.object = r.u64(); d.binding.generation = r.u64();
	d.binding.processGeneration = r.u64(); d.binding.lease = r.u64();
	r.array(d.identity.device); r.array(d.identity.driver);
	d.image.width = r.u32(); d.image.height = r.u32(); d.image.format = VkFormat(r.u32()); d.image.usage = r.u32();
	d.image.flags = r.u32(); d.image.tiling = VkImageTiling(r.u32()); d.image.type = VkImageType(r.u32());
	d.image.mipLevels = r.u32(); d.image.arrayLayers = r.u32(); d.image.samples = VkSampleCountFlagBits(r.u32());
	d.allocationSize = r.u64(); d.alignment = r.u64(); d.offset = r.u64();
	d.memoryType = r.u32(); d.dedicated = r.u32();
	d.memoryHandle = VkExternalMemoryHandleTypeFlagBits(r.u32());
	d.semaphoreHandle = VkExternalSemaphoreHandleTypeFlagBits(r.u32());
	d.epoch = r.u64(); d.layout = VkImageLayout(r.u32()); d.owner = r.u32();
	if (r.offset != WireSize) throw Failure(Error::InvalidDescriptor, "wire decoder size mismatch");
	validateDescriptor(d);
	return d;
}

}
