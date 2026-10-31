/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "vulkan.hpp"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace SurfaceProbe {
using namespace Darling::Surface;

enum class Case { FoundationImport, PayloadRoundtrip };
struct Options {
	bool list = false, crossExec = false, dropCreator = false, child = false;
	bool requireHardware = false;
	bool reverse = false, childProducer = false;
	bool fdAudit = false, enumerationOnly = false;
	Case testCase = Case::PayloadRoundtrip;
	DeviceIdentity identity;
	uint32_t width = 61, height = 47, runs = 1;
	uint32_t runIndex = 0;
	uint64_t initializationTimeoutNs = 1000000000, waitTimeoutNs = 1000000000;
	int channel = -1;
	std::string output;
};

Options parseOptions(int argc, char** argv);
std::string uuidString(const UUID& uuid);
std::string jsonString(const std::string& text);
void require(bool value, const char* message);
void checked(VkResult result, const char* operation);

struct FdIdentity {
	int number = -1, descriptorFlags = -1, statusFlags = -1;
	uint64_t device = 0, inode = 0, specialDevice = 0;
	uint32_t mode = 0;
	bool stable = false;
	std::string target;
	bool operator==(const FdIdentity& other) const;
};
std::vector<FdIdentity> snapshotFds();
void requireSameFds(const std::vector<FdIdentity>& baseline, const std::vector<FdIdentity>& current);
void setFdAudit(bool enabled);
void auditFds(const char* phase, uint32_t run);

struct Pixels {
	uint32_t width, height, rowTexels;
	size_t prefix = 64, size;
	std::vector<uint8_t> bytes;
	Pixels(uint32_t width, uint32_t height);
	void pattern(uint32_t run);
	void clearPixels();
	uint64_t compare(const void* actual, bool initialClear) const;
	uint64_t hash(const void* actual) const;
};

struct Counters {
	uint64_t readyWaits = 0, releasedSignals = 0, releasedWaits = 0;
	uint64_t uploads = 0, readbacks = 0;
	uint64_t initialHash = 0, initialExpectedHash = 0;
};

struct DeviceInfo {
	DeviceIdentity identity;
	VkPhysicalDeviceProperties properties{};
	std::vector<std::string> extensions;
};
std::vector<DeviceInfo> enumerateDevices();

class Context : public std::enable_shared_from_this<Context> {
public:
	static std::shared_ptr<Context> open(const DeviceIdentity& identity);
	~Context();
	Context(const Context&) = delete;
	Context& operator=(const Context&) = delete;
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physical = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t family = 0;
	DeviceInfo info;
	VkPhysicalDeviceMemoryProperties memory{};
	PFN_vkImportSemaphoreFdKHR importSemaphore = nullptr;
	bool lost = false;
private:
	Context() = default;
};

struct Image {
	Image() = default;
	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;
	std::shared_ptr<Context> context;
	Descriptor descriptor;
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkSemaphore ready = VK_NULL_HANDLE, released = VK_NULL_HANDLE;
	~Image();
	static std::shared_ptr<Image> import(std::shared_ptr<Context>, const Descriptor&, NativeFd memory,
		NativeFd ready, NativeFd released);
};

struct Buffer {
	Buffer() = default;
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;
	std::shared_ptr<Context> context;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void* mapped = nullptr;
	VkDeviceSize allocationSize = 0;
	bool coherent = false;
	~Buffer();
	static std::shared_ptr<Buffer> create(std::shared_ptr<Context>, size_t size, VkBufferUsageFlags usage);
	void flush();
	void invalidate();
};

class Submission : public std::enable_shared_from_this<Submission> {
public:
	static std::shared_ptr<Submission> begin(std::shared_ptr<Context>);
	~Submission();
	Submission(const Submission&) = delete;
	Submission& operator=(const Submission&) = delete;
	VkCommandBuffer command = VK_NULL_HANDLE;
	std::vector<std::shared_ptr<Image>> images;
	std::vector<std::shared_ptr<Buffer>> buffers;
	void submit(VkSemaphore wait, VkSemaphore signal, uint64_t timeoutNs);
	bool poll();
	static size_t pending();
	static void pollAll();
private:
	explicit Submission(std::shared_ptr<Context> context) : _context(std::move(context)) {}
	void cleanup();
	std::shared_ptr<Context> _context;
	VkCommandPool _pool = VK_NULL_HANDLE;
	VkFence _fence = VK_NULL_HANDLE;
	bool _pending = false;
};

void consumer(std::shared_ptr<Image>, uint32_t run, const Options&, Counters&);
uint64_t returnRead(std::shared_ptr<Image>, uint32_t run, const Options&, Counters&);
int run(const Options& options);
}
