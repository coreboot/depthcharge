/*
 * Copyright 2012 Google LLC
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

#ifndef __VBOOT_UTIL_MEMORY_H__
#define __VBOOT_UTIL_MEMORY_H__

#include <stdint.h>
#include "base/ranges.h"

int memory_range_init_and_get_unused(Ranges *ranges);
int memory_wipe_unused(void);
void memory_mark_used(uint64_t start, uint64_t end);

#endif /* __VBOOT_UTIL_MEMORY_H__ */
