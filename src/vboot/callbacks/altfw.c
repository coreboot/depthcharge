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

uint32_t vb2ex_get_altfw_count(void)
{
	struct altfw_info *node;
	uint32_t count = 0;
	ListNode *head;

	head = payload_get_altfw_list();
	if (head) {
		list_for_each(node, *head, list_node) {
			/* Discount default seqnum=0, since it is duplicated. */
			if (node->seqnum)
				count += 1;
		}
	}

	return count;
}

vb2_error_t vb2ex_run_altfw(uint32_t altfw_id)
{
	ListNode *head;

	/* If we don't have a particular one to boot, use 0. */
	head = payload_get_altfw_list();
	if (head) {
		struct altfw_info *node;

		list_for_each(node, *head, list_node) {
			if (node->seqnum == altfw_id) {
				printf("Running bootloader '%s: %s'\n",
				       node->name, node->desc);
				return payload_run(node->filename, 0) ?
					VB2_ERROR_UNKNOWN : VB2_SUCCESS;
			}
		}
	}

	/*
	 * For now, try the default payload
	 * TODO(sjg@chromium.org): Drop this once everything is migrated to
	 * altfw.
	 */
	if (payload_run("payload", 0))
		printf("%s: Could not find default altfw payload\n", __func__);
	printf("%s: Could not find bootloader #%u\n", __func__, altfw_id);

	return VB2_ERROR_UNKNOWN;
}
