// SPDX-License-Identifier: GPL-2.0

#include <commonlib/list.h>
#include <inttypes.h>
#include <libpayload.h>
#include <stdint.h>
#include <vb2_api.h>

#include "base/timestamp.h"
#include "drivers/storage/blockdev.h"
#include "vboot/load_kernel.h"

static inline int is_valid_disk(BlockDev *bdev, blockdev_type_t type)
{
	return bdev->block_size >= 512 && IS_POWER_OF_2(bdev->block_size) &&
	       bdev->block_count >= 16 &&
	       type == (bdev->removable ? BLOCKDEV_REMOVABLE : BLOCKDEV_FIXED);
}

static vb2_error_t vboot_load_kernel_impl(struct vb2_context *ctx,
					  blockdev_type_t type, int minios,
					  uint32_t minios_flags,
					  struct vb2_kernel_params *kparams)
{
	vb2_error_t rv = VB2_ERROR_LK_NO_DISK_FOUND;
	vb2_error_t new_rv;
	struct list_node *devs;
	BlockDev *bdev;

	die_if(!kparams, "kparams is NULL");

	/* Find disks. */
	get_all_bdevs(type, &devs);

	/* Only log for fixed disks to avoid spamming timestamps in recovery. */
	if (type == BLOCKDEV_FIXED)
		timestamp_add_now(TS_VB_STORAGE_INIT_DONE);

	/* Loop over disks. */
	list_for_each(bdev, *devs, list_node) {
		printf("Trying disk: %s\n", bdev->name ?: "NULL");

		if (!is_valid_disk(bdev, type)) {
			printf("  skipping: block_size=%u"
			       " block_count=%" PRIu64 " removable=%d"
			       " external_gpt=%d\n",
			       bdev->block_size, bdev->block_count,
			       bdev->removable, bdev->external_gpt);
			continue;
		}

		struct vb2_disk_info disk_info = {
			.name = bdev->name,
			.handle = (vb2ex_disk_handle_t)bdev,
			.bytes_per_lba = bdev->block_size,
			.lba_count = bdev->block_count,
			.streaming_lba_count = bdev->stream_block_count,
			.flags = bdev->external_gpt ? VB2_DISK_FLAG_EXTERNAL_GPT
						    : 0,
		};

		if (minios) {
			new_rv = vb2api_load_minios_kernel(
				ctx, kparams, &disk_info, minios_flags);
			printf("vb2api_load_minios_kernel() = %#x\n", new_rv);
		} else {
			new_rv = vb2api_load_kernel(ctx, kparams, &disk_info);
			printf("vb2api_load_kernel() = %#x\n", new_rv);
		}

		/* Stop now if we found a kernel. */
		if (new_rv == VB2_SUCCESS)
			return VB2_SUCCESS;

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
	return rv;
}

vb2_error_t vboot_load_kernel(struct vb2_context *ctx, blockdev_type_t type,
			      struct vb2_kernel_params *kparams)
{
	ctx->flags &= ~VB2_CONTEXT_DISABLE_TPM;
	return vboot_load_kernel_impl(ctx, type, 0, 0, kparams);
}

vb2_error_t vboot_load_minios_kernel(struct vb2_context *ctx,
				     uint32_t minios_flags,
				     struct vb2_kernel_params *kparams)
{
	VB2_TRY(vboot_load_kernel_impl(ctx, BLOCKDEV_FIXED, 1,
				       minios_flags, kparams));
	ctx->flags |= VB2_CONTEXT_DISABLE_TPM;
	return VB2_SUCCESS;
}
