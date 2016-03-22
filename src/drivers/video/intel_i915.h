/*
 * Copyright 2015 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_VIDEO_INTEL_I915_H__
#define __DRIVERS_VIDEO_INTEL_I915_H__

#include <stdint.h>

#include "drivers/video/display.h"

DisplayOps *new_intel_i915_display(uintptr_t reg_addr);

#endif /* __DRIVERS_VIDEO_INTEL_I915_H__ */
