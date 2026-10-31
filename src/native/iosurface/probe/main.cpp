/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probe.hpp"
#include <sys/socket.h>
#include <sys/random.h>
#include <sys/wait.h>
#include <spawn.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <fcntl.h>

extern char** environ;
static volatile sig_atomic_t stopRequested = 0;
static void stop(int) { stopRequested = 1; }
namespace SurfaceProbe {
enum class Kind : uint32_t { Configure = 1, Imported, Proceed, Finished, Release, Result };
struct Control {
	uint32_t version = 1;
	Kind kind = Kind::Configure;
	uint32_t run = 0, status = 0;
	uint32_t producerPid = 0, consumerPid = 0;
	uint64_t hash = 0;
	Counters counters;
	uint64_t memoryFeatures = 0, compatibleMemory = 0, semaphoreFeatures = 0, compatibleSemaphore = 0;
	uint64_t maxWidth = 0, maxHeight = 0, maxResource = 0;
	WireDescriptor descriptor{};
};
static_assert(sizeof(Control) == 368);
static pid_t activeChild = -1;
static bool childPending()
{
	if (activeChild < 0) return false;
	int status = 0;
	const auto result = waitpid(activeChild, &status, WNOHANG);
	if (result == activeChild || (result < 0 && errno == ECHILD)) { activeChild = -1; return false; }
	return true;
}

static void channelOptions(int channel, uint64_t timeout)
{
	const int enabled = 1;
	require(!setsockopt(channel, SOL_SOCKET, SO_PASSCRED, &enabled, sizeof(enabled)), "probe SO_PASSCRED failed");
	const auto bounded = std::max<uint64_t>(timeout, 1000000);
	timeval time{time_t(bounded / 1000000000), suseconds_t((bounded % 1000000000 + 999) / 1000)};
	if (time.tv_usec == 1000000) { ++time.tv_sec; time.tv_usec = 0; }
	require(!setsockopt(channel, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) &&
		!setsockopt(channel, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(time)), "probe finite channel timeout failed");
}
static void sendControl(int channel, const Control& message)
{
	require(send(channel, &message, sizeof(message), MSG_NOSIGNAL) == ssize_t(sizeof(message)), "probe control send failed");
}
static Control receiveControl(int channel, Kind kind, uint32_t run)
{
	Control message{};
	const auto size = recv(channel, &message, sizeof(message), MSG_TRUNC);
	require(size == sizeof(message) && message.version == 1 && message.kind == kind && message.run == run && !message.status,
		"probe missing/malformed/failed control message");
	return message;
}
static Control control(Kind kind, uint32_t run) { Control c; c.kind = kind; c.run = run; return c; }
static void capabilityReport(Control& report, const Capabilities& caps)
{
	report.memoryFeatures = caps.memory.externalMemoryFeatures; report.compatibleMemory = caps.memory.compatibleHandleTypes;
	report.semaphoreFeatures = caps.semaphore.externalSemaphoreFeatures; report.compatibleSemaphore = caps.semaphore.compatibleHandleTypes;
	report.maxWidth = caps.image.maxExtent.width; report.maxHeight = caps.image.maxExtent.height; report.maxResource = caps.image.maxResourceSize;
}

struct Child {
	NativeFd channel;
	pid_t pid = -1;
	void wait(uint64_t timeoutNs) {
		if (pid < 0) return;
		const auto end = std::chrono::steady_clock::now() + std::chrono::nanoseconds(timeoutNs);
		for (;;) {
			int status = 0;
			const auto result = waitpid(pid, &status, WNOHANG);
			if (result == pid) { pid = -1; activeChild = -1; require(WIFEXITED(status) && !WEXITSTATUS(status), "probe child failed"); return; }
			require(result >= 0 || errno == EINTR, "probe waitpid failed");
			require(std::chrono::steady_clock::now() < end, "probe child exit deadline exceeded; not signaled");
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
};
static Child startChild(const Options& o, uint32_t run)
{
	require(activeChild < 0, "probe already owns an unreaped child");
	int sockets[2]; require(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets), "probe socketpair failed");
	NativeFd parent(sockets[0]), child(sockets[1]);
	channelOptions(parent.get(), o.waitTimeoutNs);
	channelOptions(child.get(), o.waitTimeoutNs);
	const int target = child.get() == 3 ? 4 : 3;
	std::vector<std::string> args = {"surface-vulkan-hardware-probe", "--child", std::to_string(target), "--require-hardware",
		"--device-uuid", uuidString(o.identity.device), "--driver-uuid", uuidString(o.identity.driver),
		"--case", o.testCase == Case::FoundationImport ? "foundation-import" : "payload-roundtrip",
		"--width", std::to_string(o.width), "--height", std::to_string(o.height), "--run-index", std::to_string(run),
		"--initialization-timeout-ns", std::to_string(o.initializationTimeoutNs), "--wait-timeout-ns", std::to_string(o.waitTimeoutNs)};
	if (o.reverse) args.emplace_back("--producer-child");
	if (o.dropCreator) args.emplace_back("--drop-creator");
	if (o.fdAudit) args.emplace_back("--fd-audit");
	std::vector<char*> argv; for (auto& arg : args) argv.push_back(arg.data()); argv.push_back(nullptr);
	posix_spawn_file_actions_t actions;
	require(!posix_spawn_file_actions_init(&actions), "probe spawn actions init failed");
	const auto configured = posix_spawn_file_actions_addclose(&actions, parent.get()) ||
		posix_spawn_file_actions_adddup2(&actions, child.get(), target) ||
		posix_spawn_file_actions_addclose(&actions, child.get());
	if (configured) { posix_spawn_file_actions_destroy(&actions); throw std::runtime_error("probe spawn actions failed"); }
	pid_t pid = -1;
	const auto result = posix_spawn(&pid, "/proc/self/exe", &actions, nullptr, argv.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	require(!result && pid > 0, "probe posix_spawn failed");
	activeChild = pid;
	auditFds("child_spawned", run);
	return Child{std::move(parent), pid};
}

static BrokerBinding binding(uint32_t run, pid_t recipient)
{
	BrokerBinding b;
	require(getrandom(b.prefix.data(), b.prefix.size(), 0) == ssize_t(b.prefix.size()) &&
		getrandom(b.tag.data(), b.tag.size(), 0) == ssize_t(b.tag.size()), "probe fixture randomness failed");
	b.object = run + 1; b.generation = 1; b.processGeneration = uint32_t(recipient); b.lease = run + 1;
	return b;
}

static Control consume(int channel, pid_t producer, const Options& o, uint32_t run)
{
	const auto configure = receiveControl(channel, Kind::Configure, run);
	const auto expected = decode(configure.descriptor);
	require(expected.identity.device == o.identity.device && expected.identity.driver == o.identity.driver &&
		expected.image.width == o.width && expected.image.height == o.height && expected.binding.processGeneration == uint32_t(getpid()) &&
		configure.producerPid == uint32_t(producer) && configure.consumerPid == uint32_t(getpid()),
		"probe fixture configuration mismatch");
	ExpectedTransfer grant{expected, uint32_t(producer), uint32_t(getuid()), uint32_t(getgid())};
	auto transfer = receiveTransfer(channel, grant);
	auditFds("consumer_transfer_received", run);
	Counters counters;
	if (o.testCase == Case::FoundationImport) {
		auto context = Device::open(o.identity);
		auditFds("consumer_device_open", run);
		auto imported = Allocation::import(context, std::move(transfer));
		auditFds("consumer_foundation_imported", run);
		sendControl(channel, control(Kind::Imported, run)); receiveControl(channel, Kind::Proceed, run);
		imported.reset(); context.reset();
		auditFds("consumer_foundation_retired", run);
	} else {
		auto image = Image::import(Context::open(o.identity), transfer.descriptor,
			std::move(transfer.memory), std::move(transfer.ready), std::move(transfer.released));
		auditFds("consumer_payload_imported", run);
		sendControl(channel, control(Kind::Imported, run)); receiveControl(channel, Kind::Proceed, run);
		consumer(image, run, o, counters); image.reset();
		auditFds("consumer_payload_retired", run);
	}
	auto finished = control(Kind::Finished, run); finished.counters = counters;
	finished.producerPid = uint32_t(producer); finished.consumerPid = uint32_t(getpid());
	sendControl(channel, finished); receiveControl(channel, Kind::Release, run); return finished;
}

static Control produce(int channel, pid_t recipient, const Options& o, uint32_t run, const std::function<void()>& consumerExit)
{
	auto context = Device::open(o.identity);
	auditFds("producer_device_open", run);
	ImageSpec spec; spec.width = o.width; spec.height = o.height;
	const auto caps = context->query(spec);
	auto source = Allocation::create(context, binding(run, recipient), spec, o.initializationTimeoutNs);
	auditFds("producer_allocation_created", run);
	const auto descriptor = source->descriptor();
	auto transfer = source->exportTransfer(descriptor.binding);
	if (o.fdAudit) std::cerr << "{\"fd_bundle\":true,\"pid\":" << getpid() << ",\"run\":" << run << ",\"memory\":" << transfer.memory.get()
		<< ",\"ready\":" << transfer.ready.get() << ",\"released\":" << transfer.released.get() << "}\n";
	auditFds("producer_exported", run);
	NativeFd returnMemory, returnReleased;
	if (o.testCase == Case::PayloadRoundtrip) { returnMemory = transfer.memory.duplicate(); returnReleased = transfer.released.duplicate(); }
	auto configure = control(Kind::Configure, run); configure.descriptor = encode(descriptor);
	configure.producerPid = uint32_t(getpid()); configure.consumerPid = uint32_t(recipient);
	sendControl(channel, configure); sendTransfer(channel, std::move(transfer));
	auditFds("producer_transfer_sent", run);
	receiveControl(channel, Kind::Imported, run);
	auditFds("producer_import_acknowledged", run);
	if (o.dropCreator) { source.reset(); context.reset(); }
	auditFds("producer_after_creator_drop", run);
	sendControl(channel, control(Kind::Proceed, run));
	auto result = receiveControl(channel, Kind::Finished, run);
	require(result.producerPid == uint32_t(getpid()) && result.consumerPid == uint32_t(recipient), "probe process-role report mismatch");
	sendControl(channel, control(Kind::Release, run));
	if (consumerExit) consumerExit();
	auditFds("producer_consumer_retired", run);
	if (o.testCase == Case::PayloadRoundtrip) {
		auto returned = Image::import(Context::open(o.identity), descriptor, std::move(returnMemory), NativeFd(), std::move(returnReleased));
		auditFds("producer_return_imported", run);
		result.hash = returnRead(returned, run, o, result.counters);
		require(result.counters.readyWaits == 1 && result.counters.releasedSignals == 1 && result.counters.releasedWaits == 1,
			"probe directional binary semaphore count mismatch");
	}
	auditFds("producer_before_return", run);
	result.descriptor = encode(descriptor); capabilityReport(result, caps); result.kind = Kind::Result; return result;
}

static Control sameProcess(const Options& o, uint32_t run)
{
	auto context = Device::open(o.identity);
	ImageSpec spec; spec.width = o.width; spec.height = o.height;
	const auto caps = context->query(spec);
	auto source = Allocation::create(context, binding(run, getpid()), spec, o.initializationTimeoutNs);
	auto transfer = source->exportTransfer(source->descriptor().binding);
	const auto descriptor = transfer.descriptor;
	auto result = control(Kind::Result, run); result.descriptor = encode(descriptor); capabilityReport(result, caps);
	result.producerPid = result.consumerPid = uint32_t(getpid());
	if (o.testCase == Case::FoundationImport) {
		auto imported = Allocation::import(Device::open(o.identity), std::move(transfer));
	} else {
		auto memory = transfer.memory.duplicate(), released = transfer.released.duplicate();
		{
			auto image = Image::import(Context::open(o.identity), descriptor, std::move(transfer.memory), std::move(transfer.ready), std::move(transfer.released));
			consumer(image, run, o, result.counters);
		}
		auto returned = Image::import(Context::open(o.identity), descriptor, std::move(memory), NativeFd(), std::move(released));
		result.hash = returnRead(returned, run, o, result.counters);
		require(result.counters.readyWaits == 1 && result.counters.releasedSignals == 1 && result.counters.releasedWaits == 1, "probe same-process wait count mismatch");
	}
	return result;
}

int run(const Options& o)
{
	require(std::string(SURFACE_PROBE_SOURCE_ID) != "unrecorded", "configure a reviewed probe source ID before GPU execution");
	setFdAudit(o.fdAudit);
	if (o.child) {
		if (o.childProducer) { auto result = produce(o.channel, getppid(), o, o.runIndex, {}); sendControl(o.channel, result); }
		else consume(o.channel, getppid(), o, o.runIndex);
		return 0;
	}
	NativeFd outputFile;
	if (!o.output.empty()) { outputFile = NativeFd(open(o.output.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600)); require(outputFile.get() >= 0, "probe JSON output open failed"); }
	std::ostringstream output;
	auto emit = [&] {
		const auto text = output.str();
		if (outputFile.get() >= 0) {
			size_t offset = 0;
			while (offset < text.size()) {
				const auto sent = write(outputFile.get(), text.data() + offset, text.size() - offset);
				if (sent < 0 && errno == EINTR) continue;
				require(sent > 0, "probe JSON file write failed"); offset += size_t(sent);
			}
		} else std::cout << text << std::flush;
		output.str(""); output.clear();
	};
	if (o.list) {
		for (const auto& d : enumerateDevices()) {
			output << "{\"source_id\":" << jsonString(SURFACE_PROBE_SOURCE_ID) << ",\"device_uuid\":" << jsonString(uuidString(d.identity.device)) << ",\"driver_uuid\":" << jsonString(uuidString(d.identity.driver))
				<< ",\"name\":" << jsonString(d.properties.deviceName) << ",\"type\":" << d.properties.deviceType << ",\"api_version\":" << d.properties.apiVersion
				<< ",\"driver_version\":" << d.properties.driverVersion << ",\"extensions\":[";
			for (size_t i = 0; i < d.extensions.size(); ++i) output << (i ? "," : "") << jsonString(d.extensions[i]);
			output << "]}\n";
		}
		emit();
		return 0;
	}
	if (o.enumerationOnly) {
		std::vector<FdIdentity> baseline;
		for (uint32_t i = 0; i < o.runs; ++i) {
			const auto before = snapshotFds().size(); auditFds("enumeration_only_begin", i);
			const auto observed = enumerateDevices();
			auditFds("enumeration_only_retired", i); const auto after = snapshotFds();
			if (!i) baseline = after;
			output << "{\"source_id\":" << jsonString(SURFACE_PROBE_SOURCE_ID) << ",\"diagnostic\":\"enumeration-only\",\"run\":" << i
				<< ",\"physical_devices\":" << observed.size() << ",\"fd_before\":" << before << ",\"fd_after\":" << after.size()
				<< ",\"count_stable\":" << (after.size() == baseline.size() ? "true" : "false")
				<< ",\"fd_identities_stable\":" << (after == baseline ? "true" : "false") << ",\"gpu_storage_qualified\":false}\n";
			emit(); requireSameFds(baseline, after);
		}
		return 0;
	}
	std::vector<FdIdentity> stableFds;
	for (uint32_t i = 0; i < o.runs; ++i) {
		const auto before = snapshotFds().size();
		auditFds("run_begin", i);
		Control result;
		if (o.crossExec) {
			auto child = startChild(o, i);
			if (o.reverse) { consume(child.channel.get(), child.pid, o, i); result = receiveControl(child.channel.get(), Kind::Result, i); child.wait(o.waitTimeoutNs); }
			else result = produce(child.channel.get(), child.pid, o, i, [&] { child.wait(o.waitTimeoutNs); });
		} else result = sameProcess(o, i);
		auditFds("case_retired", i);
		require(!Submission::pending() && !pendingRetirements(), "probe retained unexpected submissions on success");
		const auto d = decode(result.descriptor);
		const auto observed = enumerateDevices();
		auditFds("metadata_enumerated", i);
		const auto selected = std::find_if(observed.begin(), observed.end(), [&](const auto& info) {
			return info.identity.device == d.identity.device && info.identity.driver == d.identity.driver;
		});
		require(selected != observed.end(), "probe selected hardware disappeared after payload");
		validateHardwarePair(d.identity, selected->identity, selected->properties.deviceType);
		const auto after = snapshotFds();
		if (!i) stableFds = after;
		requireSameFds(stableFds, after);
		Pixels expected(o.width, o.height); expected.pattern(i);
		require(!stopRequested, "probe stopped after completed case");
		output << "{\"source_id\":" << jsonString(SURFACE_PROBE_SOURCE_ID) << ",\"case\":" << jsonString(o.testCase == Case::FoundationImport ? "foundation-import" : "payload-roundtrip")
			<< ",\"cross_exec\":" << (o.crossExec ? "true" : "false") << ",\"reverse\":" << (o.reverse ? "true" : "false")
			<< ",\"creator_dropped\":" << (o.dropCreator ? "true" : "false") << ",\"run\":" << i
			<< ",\"producer_pid\":" << result.producerPid << ",\"consumer_pid\":" << result.consumerPid
			<< ",\"device_uuid\":" << jsonString(uuidString(d.identity.device)) << ",\"driver_uuid\":" << jsonString(uuidString(d.identity.driver))
			<< ",\"device_name\":" << jsonString(selected->properties.deviceName) << ",\"device_type\":" << selected->properties.deviceType
			<< ",\"api_version\":" << selected->properties.apiVersion << ",\"driver_version\":" << selected->properties.driverVersion
			<< ",\"object\":" << d.binding.object << ",\"generation\":" << d.binding.generation << ",\"allocation_size\":" << d.allocationSize
			<< ",\"alignment\":" << d.alignment << ",\"memory_type\":" << d.memoryType << ",\"dedicated\":" << d.dedicated
			<< ",\"width\":" << d.image.width << ",\"height\":" << d.image.height << ",\"format\":" << d.image.format << ",\"usage\":" << d.image.usage
			<< ",\"image_flags\":" << d.image.flags << ",\"tiling\":" << d.image.tiling << ",\"memory_handle\":" << d.memoryHandle << ",\"semaphore_handle\":" << d.semaphoreHandle
			<< ",\"memory_features\":" << result.memoryFeatures << ",\"compatible_memory\":" << result.compatibleMemory
			<< ",\"semaphore_features\":" << result.semaphoreFeatures << ",\"compatible_semaphore\":" << result.compatibleSemaphore
			<< ",\"max_width\":" << result.maxWidth << ",\"max_height\":" << result.maxHeight << ",\"max_resource_size\":" << result.maxResource
			<< ",\"ready_waits\":" << result.counters.readyWaits << ",\"released_signals\":" << result.counters.releasedSignals
			<< ",\"released_waits\":" << result.counters.releasedWaits << ",\"uploads\":" << result.counters.uploads << ",\"readbacks\":" << result.counters.readbacks
			<< ",\"initial_hash\":" << result.counters.initialHash << ",\"initial_expected_hash\":" << result.counters.initialExpectedHash
			<< ",\"pixel_hash\":" << result.hash << ",\"expected_hash\":" << (o.testCase == Case::PayloadRoundtrip ? expected.hash(expected.bytes.data()) : 0)
			<< ",\"fd_before\":" << before << ",\"fd_after\":" << after.size() << ",\"fd_identities_stable\":true"
			<< ",\"pixel_verified\":" << (o.testCase == Case::PayloadRoundtrip ? "true" : "false") << ",\"passed\":true}\n";
		emit();
	}
	return 0;
}
}

int main(int argc, char** argv)
{
	using namespace SurfaceProbe;
	signal(SIGINT, stop); signal(SIGTERM, stop);
	try { return run(parseOptions(argc, argv)); }
	catch (const std::exception& e) {
		const auto* failure = dynamic_cast<const Failure*>(&e);
		std::cerr << "{\"passed\":false,\"error\":" << jsonString(e.what()) << ",\"vk_result\":" << (failure ? failure->detail : 0)
			<< ",\"pid\":" << getpid() << ",\"owned_child\":" << activeChild << ",\"retirement_hold\":" << (Submission::pending() + pendingRetirements()) << "}\n";
		while (Submission::pending() || pendingRetirements() || childPending()) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			Submission::pollAll(); pollRetirements();
			if (stopRequested) { std::cerr << "termination deferred: pending owned GPU work retained\n"; stopRequested = 0; }
		}
		return 1;
	}
}
