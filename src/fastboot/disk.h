/*
 * Copyright 2021 Google LLC
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

#ifndef __FASTBOOT_DISK_H__
#define __FASTBOOT_DISK_H__

#include <stdbool.h>

#include "drivers/storage/blockdev.h"
#include "fastboot/fastboot.h"
#include "gpt_misc.h"

int fastboot_disk_init(struct FastbootOps *fb);
int fastboot_disk_gpt_init(struct FastbootOps *fb);
int fastboot_save_gpt(struct FastbootOps *fb);
void fastboot_write(struct FastbootOps *fb, const char *partition_name,
		    const uint64_t blocks_offset, void *data, size_t data_len);
void fastboot_erase(struct FastbootOps *fb, const char *partition_name);
int fastboot_get_slot_count(GptData *gpt);
char get_slot_for_partition_name(GptEntry *e, char *partition_name);
GptEntry *fastboot_get_kernel_for_slot(GptData *gpt, char slot);
void fastboot_slots_disable_all(GptData *gpt);

#endif // __FASTBOOT_DISK_H__
