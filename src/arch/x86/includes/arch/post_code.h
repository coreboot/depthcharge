/*
 * Copyright 2013 Google LLC
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

#ifndef __ARCH_X86_INCLUDES_ARCH_POST_CODE_H__
#define __ARCH_X86_INCLUDES_ARCH_POST_CODE_H__

#include <arch/io.h>

#include "base/post_code.h"

#define DEBUG_PORT 0x80

static inline void post_code(uint16_t val)
{
	// Send a post code.
	outw(val, DEBUG_PORT);
}

#endif /* __ARCH_X86_INCLUDES_ARCH_POST_CODE_H__ */
