/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __VBOOT_LOAD_KERNEL_H__
#define __VBOOT_LOAD_KERNEL_H__

#include <vb2_api.h>

/*
 * Load a kernel from the specified type(s) of disks.
 *
 * @param ctx		Vboot2 context.
 * @param disk_flags	Flags to pass to VbExDiskGetInfo().
 * @param kparams	Params specific to loading the kernel.
 *
 * @return VB2_SUCCESS or the most specific VB2_ERROR_LK error.
 */
vb2_error_t vboot_load_kernel(struct vb2_context *ctx, uint32_t disk_flags,
			      struct vb2_kernel_params *kparams);

/*
 * Load a miniOS kernel from internal disk.
 *
 * Scans sectors at the start and end of the disk, and looks for miniOS kernels
 * starting at the beginning of the sector.  Attempts loading any miniOS
 * kernels found.
 *
 * If successful, sets kparams->disk_handle to the disk for the kernel and
 * returns VB2_SUCCESS.
 *
 * @param ctx			Vboot2 context.
 * @param minios_flags		Flags for miniOS.
 * @param kparams               Params specific to loading the kernel.
 *
 * @return VB2_SUCCESS or the most specific VB2_ERROR_LK error.
 */
vb2_error_t vboot_load_minios_kernel(struct vb2_context *ctx,
				     uint32_t minios_flags,
				     struct vb2_kernel_params *kparams);

#endif /* __VBOOT_LOAD_KERNEL_H__ */
