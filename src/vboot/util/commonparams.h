/*
 * Copyright 2012 Google Inc.
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

#ifndef __VBOOT_UTIL_COMMONPARAMS_H__
#define __VBOOT_UTIL_COMMONPARAMS_H__

#include <stddef.h>
#include <stdint.h>
#include <vboot_api.h>

/*
 * The vboot_handoff structure contains the data to be consumed by downstream
 * firmware after firmware selection has been completed. Namely it provides
 * vboot shared data as well as the flags from VbInit.
 */
struct vboot_handoff {
	uint32_t reserved0;  /* originally from VbInitParams */
	uint32_t out_flags;
	uint32_t selected_firmware;
	char shared_data[VB_SHARED_DATA_MIN_SIZE];
} __attribute__((packed));

int gbb_clear_flags(void);

int common_params_init(void);
int find_common_params(void **blob, int *size);

struct vb2_context *vboot_get_context(void);

#endif /* __VBOOT_UTIL_COMMONPARAMS_H__ */
