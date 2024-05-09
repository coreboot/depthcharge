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

#ifndef __VBOOT_STAGES_H__
#define __VBOOT_STAGES_H__

#include <stdint.h>
#include <vb2_api.h>

int vboot_check_wipe_memory(void);
int vboot_check_enable_usb(void);
int vboot_in_recovery(void);
int vboot_in_manual_recovery(void);
int vboot_in_developer(void);

/*
 * Select, load, and boot the kernel.
 *
 * If selecting and loading the kernel succeed, boot the kernel, and never
 * return (or return 1 if booting fails). Otherwise, reboot the device and this
 * function will never return.
 */
int vboot_select_and_boot_kernel(void);

/*
 * Select and load the kernel.
 *
 * @param ctx		Vboot2 context.
 * @param kparams	Params specific to loading the kernel.
 *
 * @return VB2_SUCCESS on success, non-zero on error. on error, caller
 * should reboot.
 */
vb2_error_t vboot_select_and_load_kernel(struct vb2_context *ctx,
					 struct vb2_kernel_params *kparams);

/*
 * Boot the kernel.
 *
 * @param kparams               Params containing the disk information.
 */
void vboot_boot_kernel(struct vb2_kernel_params *kparams);

#endif /* __VBOOT_STAGES_H__ */
