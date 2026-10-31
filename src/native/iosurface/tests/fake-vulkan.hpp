/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "vulkan.hpp"

// Deterministic native failure controls; never linked into the production library.
namespace FakeVulkan {
void reset();
void failAt(unsigned operation);
unsigned operations();
unsigned liveObjects();
unsigned initializationCount();
void memoryTypeBits(uint32_t bits);
void imageFeatures(VkExternalMemoryFeatureFlags flags);
void semaphoreFeatures(VkExternalSemaphoreFeatureFlags flags);
void deviceType(VkPhysicalDeviceType type);
void extensions(bool enabled);
void waitError(VkResult error);
void fenceStatus(VkResult status);
void submitError(VkResult error);
unsigned submitOperation();
Darling::Surface::DeviceIdentity identity();
}
