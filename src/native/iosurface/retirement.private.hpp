/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "vulkan.hpp"

namespace Darling::Surface {
void registerRetirement(const std::shared_ptr<RetirementTicket>& ticket);
void unregisterRetirement(const std::shared_ptr<RetirementTicket>& ticket);
}
