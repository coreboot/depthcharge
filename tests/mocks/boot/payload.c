// SPDX-License-Identifier: GPL-2.0

#include <commonlib/list.h>
#include <libpayload.h>
#include <mocks/payload.h>
#include <tests/test.h>
#include <vb2_api.h>

#include "boot/payload.h"

struct list_node payload_altfw_head;
int payload_altfw_head_initialized;

size_t payload_get_altfw_count(void)
{
	return mock_type(size_t);
}

struct list_node *payload_get_altfw_list(void)
{
	int seqnum;
	const uint32_t count = payload_get_altfw_count();
	struct altfw_info *info;
	const struct altfw_info altfw_info = {
		.filename = "altfw_file",
		.name = "altfw_name",
		.desc = "altfw_desc",
	};

	if (payload_altfw_head_initialized)
		return &payload_altfw_head;

	memset(&payload_altfw_head, 0, sizeof(payload_altfw_head));
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

int payload_run_altfw(int altfw_id)
{
	int ret;
	check_expected(altfw_id);
	ret = mock();
	if (ret == 0) {
		mock_assert(0, __func__, __FILE__, __LINE__);
		return 0;
	}

	return ret;
}
