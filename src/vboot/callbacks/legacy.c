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

#include <libpayload.h>
#include <cbfs.h>
#include <vboot_api.h>

#include "base/list.h"
#include "boot/payload.h"
#include "drivers/flash/cbfs.h"

int VbExLegacy(int altfw_num)
{
	ListNode *head = NULL;
	struct altfw_info *node;
	struct cbfs_media media;

	if (cbfs_media_from_fmap("RW_LEGACY", &media)) {
		printf("%s: Cannot set up CBFS\n", __func__);
		return -1;
	}

	/*
	 * If we don't have a particular one to boot, use 0.
	 *
	 * TODO(sjg@chromium.org): Add a menu to vboot for this situation so
	 * that the user can select which to boot.
	 */
	if (altfw_num)
		head = payload_get_altfw_list(&media);

	if (head) {
		list_for_each(node, *head, list_node) {
			if (node->seqnum == altfw_num) {
				printf("Running bootloader '%s: %s'\n",
				       node->name, node->desc);
				return payload_run(&media, node->filename);
			}
		}
	}

	/*
	 * For now, try the default payload
	 * TODO(sjg@chromium.org): Drop this once everything is migrated to
	 * altfw.
	 */
	if (payload_run(&media, "payload"))
		printf("%s: Could not find default legacy payload\n", __func__);
	printf("%s: Could not find bootloader #%d\n", __func__, altfw_num);

	return -1;
}
