/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __VBOOT_UTIL_VBOOT_HANDOFF_H__
#define __VBOOT_UTIL_VBOOT_HANDOFF_H__

#include <vboot_api.h>

/*
 * The vboot_handoff structure contains the data to be consumed by downstream
 * firmware after firmware selection has been completed. Namely it provides
 * vboot shared data as well as the flags from VbInit. As noted above a finite
 * number of components are parsed from the verfieid firmare region.
 */
struct vboot_handoff {
	VbInitParams init_params;
	uint32_t selected_firmware;
	char shared_data[VB_SHARED_DATA_MIN_SIZE];
} __attribute__((packed));


#endif /* __VBOOT_UTIL_VBOOT_HANDOFF_H__ */
