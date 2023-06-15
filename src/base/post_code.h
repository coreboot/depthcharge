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

#ifndef __BASE_POST_CODE_H__
#define __BASE_POST_CODE_H__

#include <stdint.h>

/* Legacy POST codes */
#define POST_CODE_START_DEPTHCHARGE	0xaa
#define POST_CODE_START_KERNEL		0xab

/* POST codes 0xd000-0xd0ff are reserved for depthcharge */
#define POST_CODE_KERNEL_PHASE_1	0xd010
#define POST_CODE_KERNEL_PHASE_2	0xd020
#define POST_CODE_KERNEL_LOAD		0xd030
#define POST_CODE_KERNEL_FINALIZE	0xd040
#define POST_CODE_CROSSYSTEM_SETUP	0xd050


static inline void post_code(uint16_t val);

#endif /* __BASE_POST_CODE_H__ */
