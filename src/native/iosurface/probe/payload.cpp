/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "probe.hpp"
#include <cstring>
#include <iostream>

namespace SurfaceProbe {
static VkBufferImageCopy copy(const Pixels& pixels)
{
	VkBufferImageCopy region{}; region.bufferOffset = pixels.prefix; region.bufferRowLength = pixels.rowTexels;
	region.bufferImageHeight = pixels.height; region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	region.imageExtent = {pixels.width, pixels.height, 1}; return region;
}
static void imageBarrier(VkCommandBuffer command, const Image& image, bool acquire, bool initial)
{
	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.image = image.image; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	barrier.oldLayout = initial ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex = acquire ? VK_QUEUE_FAMILY_EXTERNAL : image.context->family;
	barrier.dstQueueFamilyIndex = acquire ? image.context->family : VK_QUEUE_FAMILY_EXTERNAL;
	barrier.srcAccessMask = acquire ? 0 : VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = acquire ? VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT : 0;
	vkCmdPipelineBarrier(command, acquire ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT,
		acquire ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
static void hostBarrier(VkCommandBuffer command, const Buffer& buffer, bool toHost)
{
	VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER}; barrier.buffer = buffer.buffer; barrier.size = VK_WHOLE_SIZE;
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.srcAccessMask = toHost ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = toHost ? VK_ACCESS_HOST_READ_BIT : VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(command, toHost ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_HOST_BIT,
		toHost ? VK_PIPELINE_STAGE_HOST_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void consumer(std::shared_ptr<Image> image, uint32_t run, const Options& options, Counters& counters)
{
	require(image->ready && image->released, "consumer needs both directional semaphores");
	Pixels pixels(image->descriptor.image.width, image->descriptor.image.height);
	auto readback = Buffer::create(image->context, pixels.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	memset(readback->mapped, 0xa5, pixels.size); readback->flush();
	auto read = Submission::begin(image->context); read->images = {image}; read->buffers = {readback};
	imageBarrier(read->command, *image, true, true); hostBarrier(read->command, *readback, false);
	auto region = copy(pixels);
	vkCmdCopyImageToBuffer(read->command, image->image, VK_IMAGE_LAYOUT_GENERAL, readback->buffer, 1, &region);
	hostBarrier(read->command, *readback, true);
	++counters.readyWaits; ++counters.readbacks;
	read->submit(image->ready, VK_NULL_HANDLE, options.waitTimeoutNs);
	readback->invalidate();
	pixels.clearPixels();
	counters.initialHash = pixels.hash(readback->mapped); counters.initialExpectedHash = pixels.hash(pixels.bytes.data());
	const auto initialMismatches = pixels.compare(readback->mapped, true);
	if (initialMismatches) std::cerr << "initial mismatch bytes=" << initialMismatches << " actual_hash=" << counters.initialHash << " expected_hash=" << counters.initialExpectedHash << '\n';
	require(!initialMismatches, "initial shared clear/canary mismatch");
	pixels.pattern(run);
	auto upload = Buffer::create(image->context, pixels.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	memcpy(upload->mapped, pixels.bytes.data(), pixels.size); upload->flush();
	auto write = Submission::begin(image->context); write->images = {image}; write->buffers = {upload};
	VkImageMemoryBarrier local{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; local.image = image->image;
	local.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}; local.oldLayout = local.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	local.srcQueueFamilyIndex = local.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	local.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT; local.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(write->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &local);
	hostBarrier(write->command, *upload, false);
	vkCmdCopyBufferToImage(write->command, upload->buffer, image->image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
	imageBarrier(write->command, *image, false, false);
	++counters.uploads; ++counters.releasedSignals;
	write->submit(VK_NULL_HANDLE, image->released, options.waitTimeoutNs);
}

uint64_t returnRead(std::shared_ptr<Image> image, uint32_t run, const Options& options, Counters& counters)
{
	require(image->released && !image->ready, "return reader owns only released wait");
	Pixels pixels(image->descriptor.image.width, image->descriptor.image.height); pixels.pattern(run);
	auto buffer = Buffer::create(image->context, pixels.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	memset(buffer->mapped, 0xa5, pixels.size); buffer->flush();
	auto read = Submission::begin(image->context); read->images = {image}; read->buffers = {buffer};
	imageBarrier(read->command, *image, true, false); hostBarrier(read->command, *buffer, false);
	auto region = copy(pixels);
	vkCmdCopyImageToBuffer(read->command, image->image, VK_IMAGE_LAYOUT_GENERAL, buffer->buffer, 1, &region);
	hostBarrier(read->command, *buffer, true);
	++counters.releasedWaits; ++counters.readbacks;
	read->submit(image->released, VK_NULL_HANDLE, options.waitTimeoutNs);
	buffer->invalidate();
	const auto mismatches = pixels.compare(buffer->mapped, false);
	if (mismatches) std::cerr << "return mismatch bytes=" << mismatches << " actual_hash=" << pixels.hash(buffer->mapped) << " expected_hash=" << pixels.hash(pixels.bytes.data()) << '\n';
	require(!mismatches, "cross-endpoint pixel/canary mismatch");
	return pixels.hash(buffer->mapped);
}
}
