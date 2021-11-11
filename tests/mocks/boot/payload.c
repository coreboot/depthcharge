// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <mocks/payload.h>
#include <tests/test.h>
#include <vb2_api.h>

#include "base/list.h"
#include "boot/payload.h"

struct ListNode payload_altfw_head;
int payload_altfw_head_initialized;

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

	if (payload_altfw_head_initialized)
		return &payload_altfw_head;

	payload_altfw_head.next = NULL;
	payload_altfw_head.prev = NULL;
	for (seqnum = count; seqnum >= 1; seqnum--) {
		info = malloc(sizeof(*info));
		if (!info)
			fail_msg("Cannot allocate memory");
		*info = altfw_info;
		info->seqnum = seqnum;
		list_insert_after(&info->list_node, &payload_altfw_head);
	}

	payload_altfw_head_initialized = 1;
	return &payload_altfw_head;
}
