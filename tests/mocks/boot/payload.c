// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <tests/test.h>
#include <vb2_api.h>

#include "base/list.h"
#include "boot/payload.h"

/*
 * NOTE: To avoid infinte recursion, this function must be linked with a mocking
 * version of vb2ex_get_altfw_count().
 */
struct ListNode *payload_get_altfw_list(void)
{
	int seqnum;
	const uint32_t count = vb2ex_get_altfw_count();
	struct altfw_info *info;
	const struct altfw_info altfw_info = {
		.list_node = {
			.next = NULL,
			.prev = NULL,
		},
		.filename = "altfw_file",
		.name = "altfw_name",
		.desc = "altfw_desc",
	};
	static struct ListNode head = {
		.next = NULL,
		.prev = NULL,
	};
	for (seqnum = 1; seqnum <= count; seqnum++) {
		info = malloc(sizeof(*info));
		if (!info)
			fail_msg("Cannot allocate memory");
		*info = altfw_info;
		info->seqnum = seqnum;
		list_insert_after(&info->list_node, &head);
	}
	return &head;
}
