/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probe.hpp"
#include <charconv>
#include <cstring>
#include <limits>
#include <cstdio>
#include <unistd.h>

namespace SurfaceProbe {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void checked(VkResult result, const char* operation) {
	fprintf(stderr, "pid=%ld vk_call=%s result=%d\n", long(getpid()), operation, int(result));
	if (result != VK_SUCCESS) throw Failure(Error::Vulkan, operation, result);
}

static uint64_t number(const char* text)
{
	uint64_t result = 0;
	const auto end = text + strlen(text);
	const auto parsed = std::from_chars(text, end, result);
	require(parsed.ec == std::errc{} && parsed.ptr == end, "invalid unsigned argument");
	return result;
}

static UUID uuid(const char* text)
{
	UUID result;
	require(strlen(text) == result.size() * 2, "UUID needs 32 hex digits");
	for (size_t i = 0; i < result.size(); ++i) {
		unsigned value = 0;
		const auto parsed = std::from_chars(text + 2 * i, text + 2 * i + 2, value, 16);
		require(parsed.ec == std::errc{} && parsed.ptr == text + 2 * i + 2, "invalid UUID");
		result[i] = uint8_t(value);
	}
	return result;
}

Options parseOptions(int argc, char** argv)
{
	Options o;
	bool device = false, driver = false;
	for (int i = 1; i < argc; ++i) {
		const std::string option = argv[i];
		auto value = [&]() { require(i + 1 < argc, "missing argument value"); return argv[++i]; };
		if (option == "--list-hardware") o.list = true;
		else if (option == "--json") {}
		else if (option == "--require-hardware") o.requireHardware = true;
		else if (option == "--cross-exec") o.crossExec = true;
		else if (option == "--same-process") o.crossExec = false;
		else if (option == "--drop-creator") o.dropCreator = true;
		else if (option == "--reverse") o.reverse = true;
		else if (option == "--producer-child") o.childProducer = true;
		else if (option == "--child") { const auto n = number(value()); require(n > 2 && n <= INT32_MAX, "invalid child channel"); o.child = true; o.channel = int(n); }
		else if (option == "--device-uuid") { o.identity.device = uuid(value()); device = true; }
		else if (option == "--driver-uuid") { o.identity.driver = uuid(value()); driver = true; }
		else if (option == "--case") {
			const std::string name = value();
			require(name == "foundation-import" || name == "payload-roundtrip", "unsupported probe case");
			o.testCase = name == "foundation-import" ? Case::FoundationImport : Case::PayloadRoundtrip;
		} else if (option == "--width") { const auto n = number(value()); require(n && n <= 4096, "width outside probe bound"); o.width = uint32_t(n); }
		else if (option == "--height") { const auto n = number(value()); require(n && n <= 4096, "height outside probe bound"); o.height = uint32_t(n); }
		else if (option == "--runs") { const auto n = number(value()); require(n && n <= 256, "run count outside probe bound"); o.runs = uint32_t(n); }
		else if (option == "--run-index") { const auto n = number(value()); require(n <= 255, "run index outside probe bound"); o.runIndex = uint32_t(n); }
		else if (option == "--initialization-timeout-ns") o.initializationTimeoutNs = number(value());
		else if (option == "--wait-timeout-ns") o.waitTimeoutNs = number(value());
		else if (option == "--json-out") o.output = value();
		else throw std::runtime_error("unknown probe option: " + option);
	}
	if (!o.list) require(device && driver && o.requireHardware, "explicit UUID pair and --require-hardware required");
	require(o.initializationTimeoutNs <= 10000000000ULL && o.waitTimeoutNs <= 10000000000ULL,
		"probe waits must be finite and at most ten seconds");
	require(!o.dropCreator || o.crossExec || o.child, "creator-drop requires cross-exec");
	require(!o.reverse || o.crossExec, "reverse role requires cross-exec");
	require(!o.childProducer || o.child, "internal producer role requires child mode");
	return o;
}

std::string uuidString(const UUID& id)
{
	static const char hex[] = "0123456789abcdef";
	std::string result;
	for (auto byte : id) { result += hex[byte >> 4]; result += hex[byte & 15]; }
	return result;
}
std::string jsonString(const std::string& text)
{
	std::string result = "\"";
	for (unsigned char byte : text) {
		if (byte == '\\' || byte == '"') { result += '\\'; result += char(byte); }
		else if (byte >= 32 && byte < 127) result += char(byte);
		else result += '?';
	}
	return result + '"';
}

Pixels::Pixels(uint32_t width, uint32_t height) : width(width), height(height), rowTexels(width + 7),
	size(prefix + size_t(rowTexels) * height * 4 + 64), bytes(size, 0xa5) {}
void Pixels::pattern(uint32_t run)
{
	for (uint32_t y = 0; y < height; ++y) for (uint32_t x = 0; x < width; ++x) {
		auto p = &bytes[prefix + (size_t(y) * rowTexels + x) * 4];
		p[0] = uint8_t(13 * x + 3 * y + run * 17);
		p[1] = uint8_t(5 * x + 19 * y + run * 29 + 31);
		p[2] = uint8_t(23 * x + 7 * y + run * 11 + 71);
		p[3] = uint8_t(37 * x + 41 * y + run * 43 + 127);
	}
}
void Pixels::clearPixels()
{
	for (uint32_t y = 0; y < height; ++y) memset(&bytes[prefix + size_t(y) * rowTexels * 4], 0, size_t(width) * 4);
}
uint64_t Pixels::compare(const void* actual, bool initialClear) const
{
	const auto* data = static_cast<const uint8_t*>(actual);
	uint64_t mismatches = 0;
	for (size_t i = 0; i < size; ++i) {
		const bool pixel = i >= prefix && i < size - 64 && ((i - prefix) / 4) % rowTexels < width;
		const auto expected = pixel && initialClear ? 0 : bytes[i];
		if (data[i] != expected) ++mismatches;
	}
	return mismatches;
}
uint64_t Pixels::hash(const void* actual) const
{
	const auto* data = static_cast<const uint8_t*>(actual);
	uint64_t result = 14695981039346656037ULL;
	for (size_t i = 0; i < size; ++i) { result ^= data[i]; result *= 1099511628211ULL; }
	return result;
}
}
