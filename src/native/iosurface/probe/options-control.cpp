/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probe.hpp"
#include <iostream>

using namespace SurfaceProbe;
static Options parse(std::vector<std::string> arguments)
{
	std::vector<char*> pointers;
	for (auto& argument : arguments) pointers.push_back(argument.data());
	return parseOptions(int(pointers.size()), pointers.data());
}
template<class Function> static void rejects(Function f)
{
	try { f(); } catch (const std::exception&) { return; }
	throw std::runtime_error("probe accepted invalid CPU control");
}
int main()
{
	try {
		require(jsonString(std::string("\n\t\0\x7f\x80\xff", 6)) == "\"\\u000a\\u0009\\u0000\\u007f\\u0080\\u00ff\"",
			"JSON diagnostic lost native string bytes");
		require(jsonString("?\n") != jsonString("??"), "JSON diagnostic collapsed distinct native names");
		std::vector<std::string> base{"probe", "--require-hardware", "--device-uuid", std::string(32, '1'), "--driver-uuid", std::string(32, '2')};
		require(parse(base).width == 61, "probe argument defaults mismatch");
		for (auto pair : {std::pair{"--runs", "0"}, {"--runs", "257"}, {"--width", "0"}, {"--width", "4097"},
			{"--height", "-1"}, {"--wait-timeout-ns", "18446744073709551615"}, {"--case", "fake"}, {"--runs", "1junk"}}) {
			auto args = base; args.push_back(pair.first); args.push_back(pair.second); rejects([&] { parse(args); });
		}
		rejects([&] { parse({"probe"}); });
		rejects([&] { parse({"probe", "--list-hardware", "--unknown"}); });
		require(parse({"probe", "--list-hardware", "--json"}).list, "CPU list-option parse failed");
		Pixels expected(61, 47);
		auto untouched = expected.bytes;
		require(expected.compare(untouched.data(), true) != 0, "untouched sentinel falsely passed initial clear");
		std::vector<uint8_t> zeros(expected.size);
		require(expected.compare(zeros.data(), true) != 0, "zero canaries falsely passed initial clear");
		expected.clearPixels(); require(expected.compare(expected.bytes.data(), true) == 0, "initial zero detector failed");
		expected.pattern(13); require(expected.compare(expected.bytes.data(), false) == 0, "pattern self comparison failed");
		for (auto offset : {size_t(0), expected.prefix, expected.prefix + size_t(expected.width) * 4, expected.size - 1}) {
			auto changed = expected.bytes; changed[offset] ^= 1;
			require(expected.compare(changed.data(), false) == 1, "pixel/padding/canary detector missed a byte");
		}
		const auto hash = expected.hash(expected.bytes.data()); expected.pattern(14);
		require(hash != expected.hash(expected.bytes.data()), "pattern run index did not change full hash");
		require(uuidString(parse(base).identity.device) == std::string(32, '1'), "UUID CPU roundtrip failed");
		std::cout << "probe CPU options/sentinel/pattern controls passed; no Vulkan calls\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
