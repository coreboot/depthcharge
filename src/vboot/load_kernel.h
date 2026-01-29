/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __VBOOT_LOAD_KERNEL_H__
#define __VBOOT_LOAD_KERNEL_H__

#include <vb2_api.h>

#include "drivers/storage/blockdev.h"

/**
 * Initialize a vboot disk information structure from a block device.
 *
 * @param di		Vboot disk information structure to initialize.
 * @param bdev		Block device to use for initialization.
 */
void vboot_create_disk_info(struct vb2_disk_info *di, BlockDev *bdev);

/*
 * Load a kernel from the specified type(s) of disks.
 *
 * @param ctx		Vboot2 context.
 * @param type		Block device type.
 * @param kparams	Params specific to loading the kernel.
 *
 * @return VB2_SUCCESS or the most specific VB2_ERROR_LK error.
 */
vb2_error_t vboot_load_kernel(struct vb2_context *ctx, blockdev_type_t type,
			      struct vb2_kernel_params *kparams);

/*
 * Load a NBR kernel from internal disk.
 *
 * Scans sectors at the start and end of the disk, and looks for network
 * based recovery (NBR) kernels starting at the beginning of the sector.
 *
 * If successful, sets kparams->disk_handle to the disk for the kernel and
 * returns VB2_SUCCESS.
 *
 * @param ctx			Vboot2 context.
 * @param nbr_flags		Flags for NBR.
 * @param kparams               Params specific to loading the kernel.
 *
 * @return VB2_SUCCESS or the most specific VB2_ERROR_LK error.
 */
vb2_error_t vboot_load_nbr_kernel(struct vb2_context *ctx,
				  uint32_t nbr_flags,
				  struct vb2_kernel_params *kparams);

#endif /* __VBOOT_LOAD_KERNEL_H__ */
