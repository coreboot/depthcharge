// SPDX-License-Identifier: GPL-2.0

#include <inttypes.h>
#include <libpayload.h>
#include <stdint.h>
#include <vb2_api.h>
#include <vboot_api.h>

#include "vboot/load_kernel.h"

static int is_valid_disk(const struct vb2_disk_info *info, uint32_t disk_flags)
{
	return info->bytes_per_lba >= 512 &&
	       IS_POWER_OF_2(info->bytes_per_lba) && info->lba_count >= 16 &&
	       (info->flags & disk_flags & VB2_DISK_FLAG_SELECT_MASK) &&
	       IS_POWER_OF_2(info->flags & VB2_DISK_FLAG_SELECT_MASK);
}

static vb2_error_t vboot_load_kernel_impl(struct vb2_context *ctx,
					  uint32_t disk_flags, int minios,
					  uint32_t minios_flags,
					  struct vb2_kernel_params *kparams)
{
	vb2_error_t rv = VB2_ERROR_LK_NO_DISK_FOUND;
	struct vb2_disk_info *disk_info = NULL;
	uint32_t disk_count = 0;
	uint32_t i;
	vb2_error_t new_rv;

	die_if(!kparams, "kparams is NULL");

	/* Find disks. */
	if (VbExDiskGetInfo(&disk_info, &disk_count, disk_flags))
		disk_count = 0;

	/* Loop over disks. */
	for (i = 0; i < disk_count; i++) {
		printf("trying disk %d\n", (int)i);

		if (!is_valid_disk(&disk_info[i], disk_flags)) {
			printf("  skipping: bytes_per_lba=%" PRIu64
			       " lba_count=%" PRIu64 " flags=%#x\n",
			       disk_info[i].bytes_per_lba,
			       disk_info[i].lba_count, disk_info[i].flags);
			continue;
		}

		if (minios) {
			new_rv = vb2api_load_minios_kernel(
				ctx, kparams, &disk_info[i], minios_flags);
			printf("vb2api_load_minios_kernel() = %#x\n", new_rv);
		} else {
			new_rv = vb2api_load_kernel(
				ctx, kparams, &disk_info[i]);
			printf("vb2api_load_kernel() = %#x\n", new_rv);
		}

		/* Stop now if we found a kernel. */
		if (new_rv == VB2_SUCCESS) {
			VbExDiskFreeInfo(disk_info, disk_info[i].handle);
			return VB2_SUCCESS;
		}

		/* Don't update error if we already have a more specific one. */
		if (rv != VB2_ERROR_LK_INVALID_KERNEL_FOUND)
			rv = new_rv;
	}

	/* If we drop out of the loop, we didn't find any usable kernel. */
	if (ctx->boot_mode == VB2_BOOT_MODE_NORMAL) {
		switch (rv) {
		case VB2_ERROR_LK_INVALID_KERNEL_FOUND:
			vb2api_fail(ctx, VB2_RECOVERY_RW_INVALID_OS, rv);
			break;
		case VB2_ERROR_LK_NO_KERNEL_FOUND:
			vb2api_fail(ctx, VB2_RECOVERY_RW_NO_KERNEL, rv);
			break;
		case VB2_ERROR_LK_NO_DISK_FOUND:
			vb2api_fail(ctx, VB2_RECOVERY_RW_NO_DISK, rv);
			break;
		default:
			vb2api_fail(ctx, VB2_RECOVERY_LK_UNSPECIFIED, rv);
			break;
		}
	}

	/* If we didn't find any good kernels, don't return a disk handle. */
	kparams->disk_handle = NULL;
	VbExDiskFreeInfo(disk_info, NULL);

	return rv;
}

vb2_error_t vboot_load_kernel(struct vb2_context *ctx, uint32_t disk_flags,
			      struct vb2_kernel_params *kparams)
{
	ctx->flags &= ~VB2_CONTEXT_DISABLE_TPM;
	return vboot_load_kernel_impl(ctx, disk_flags, 0, 0, kparams);
}

vb2_error_t vboot_load_minios_kernel(struct vb2_context *ctx,
				     uint32_t minios_flags,
				     struct vb2_kernel_params *kparams)
{
	VB2_TRY(vboot_load_kernel_impl(ctx, VB2_DISK_FLAG_FIXED, 1,
				       minios_flags, kparams));
	ctx->flags |= VB2_CONTEXT_DISABLE_TPM;
	return VB2_SUCCESS;
}
