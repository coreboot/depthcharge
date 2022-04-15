// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <vb2_api.h>

#include "base/list.h"
#include "boot/payload.h"

struct ListNode *payload_get_altfw_list(void)
{
	static struct ListNode payload_altfw_head;
	static int payload_altfw_head_initialized;
	int i;
	struct altfw_info *info;
	const struct altfw_info altfw_infos[] = {
		[0] = {
			.filename = "altfw_file0",
			.name = "altfw 0",
			.desc = "altfw_desc0",
		},
		[1] = {
			.filename = "altfw_file1",
			.name = "altfw 1",
			.desc = "altfw_desc1",
		},
	};

	if (payload_altfw_head_initialized)
		return &payload_altfw_head;

	for (i = ARRAY_SIZE(altfw_infos) - 1; i >= 0; i--) {
		info = xzalloc(sizeof(*info));
		*info = altfw_infos[i];
		info->seqnum = i + 1;  /* Must be positive */
		list_insert_after(&info->list_node, &payload_altfw_head);
	}

	payload_altfw_head_initialized = 1;
	return &payload_altfw_head;
}

int payload_run_altfw(int altfw_id)
{
	return 0;
}
