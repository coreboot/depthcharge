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

#ifndef __BOOT_BOOT_H__
#define __BOOT_BOOT_H__

#include <vboot_api.h>

struct boot_info {
	void *kernel;
	char *cmd_line;
	void *params;
	void *loader;
	void *ramdisk_addr;
	size_t ramdisk_size;
	VbSelectAndLoadKernelParams *kparams;
};

// To be implemented by each boot method.
int boot(struct boot_info *bi);

// Alternative boot method, to try is the main method failed.
int legacy_boot(void *kernel, const char *cmd_line_buf);

#endif /* __BOOT_BOOT_H__ */
