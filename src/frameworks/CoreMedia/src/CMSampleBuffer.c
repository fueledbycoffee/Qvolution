/*
 This file is part of Darling.

 Copyright (C) 2025 Darling Developers

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <CoreMedia/CMSampleBuffer.h>

const CFStringRef kCMSampleAttachmentKey_DisplayImmediately = CFSTR("DisplayImmediately");

// Public API specifies a CFString key with CFBoolean values and no default.
// GUESS: bare-key spelling follows the existing NotSync/DisplayImmediately
// convention. Native constant bytes and sample/codec interoperability are not
// established by this project data policy; absent metadata stays unknown.
const CFStringRef kCMSampleAttachmentKey_IsDependedOnByOthers = CFSTR("IsDependedOnByOthers");
