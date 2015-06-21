/*
 * Copyright 2015 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __VBOOT_BOOT_POLICY_H__
#define __VBOOT_BOOT_POLICY_H__

#include <vboot_api.h>

#include "vboot/boot.h"

/*
 * Flags field in vboot kernel preamble is defined as:
 * [31:2] - Reserved (for future use)
 * [1:0]  - Image type (0x0 = CrOS, 0x1 = Bootimg)
 */
#define KERNEL_IMG_TYPE_MASK	(0x3)
#define KERNEL_IMG_TYPE_SHIFT	(0x0)

#define GET_KERNEL_IMG_TYPE(x)					\
	(((x) >> KERNEL_IMG_TYPE_SHIFT) & KERNEL_IMG_TYPE_MASK)

typedef enum {
	KERNEL_IMAGE_CROS = 0 << KERNEL_IMG_TYPE_SHIFT,
	KERNEL_IMAGE_BOOTIMG = 1 << KERNEL_IMG_TYPE_SHIFT,
} kernel_img_type_t;

typedef enum {
	CMD_LINE_SIGNER = 1 << 0,
	CMD_LINE_BOOTIMG_HDR = 1 << 1,
	CMD_LINE_DTB = 1 << 2,
} cmd_line_loc_t;

enum {
	CmdLineSize = 4096,
	CrosParamSize = 4096,
};

struct boot_policy {
	kernel_img_type_t img_type;
	cmd_line_loc_t cmd_line_loc;
};

/*
 * Allows the board to pass in an array of boot_policy structures and count of
 * the number of policies. Boot policy mechanism is used to select a particular
 * policy for booting a board i.e. select a kernel image type and identify the
 * location of command line. Default policy is to use CrOS image type and cmd
 * line passed in by the signer. Thus, for all boards added before this change
 * and new boards who do not set any policy, default policy would be used.
 * In cases where the boards provide more than one boot policy, policy with
 * lower index is considered to have higher priority. Thus, the first match for
 * policy is chosen as the booting policy.
 */
int set_boot_policy(const struct boot_policy *policy, size_t count);

/*
 * Depending upon the boot policy set by the board for supported image type,
 * this function selects the appropriate function pointer to fill up the
 * boot_info structure.
 *
 * Returns 0 on success, -1 on error. Error can occur if the image type handler
 * is not able to setup the boot info structure for any reason.
 *
 * This function assumes that the board provides a prioritized list of image
 * types that it wants to boot. In the default case, we let only CrOS images
 * with cmd line filled by signer boot.
 */
int fill_boot_info(struct boot_info *bi, VbSelectAndLoadKernelParams *kparams);

/*
 * Given a bootimg, this routine returns pointer to the start of the kernel
 * image. If the bootimg does not have the right magic value in the header, it
 * returns NULL. It also returns NULL if image_size is not big enough to hold
 * kernel as per the details in bootimg hdr.
 */
void *bootimg_get_kernel_ptr(void *img, size_t image_size);

#endif /* __VBOOT_BOOT_POLICY_H__ */
