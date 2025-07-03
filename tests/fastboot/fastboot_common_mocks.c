// SPDX-License-Identifier: GPL-2.0

#include "fastboot/fastboot.h"
#include "tests/fastboot/fastboot_common_mocks.h"
#include "tests/test.h"

/* Mock data */
struct FastbootOps test_fb;
BlockDev test_disk;
GptData test_gpt;

/* Mocked functions */
static int fb_mock_send_packet(struct FastbootOps *fb, void *buf, size_t len)
{
	check_expected_ptr(fb);
	check_expected(buf);
	check_expected(len);

	return 0;
}

void setup_test_fb(void)
{
	memset(&test_fb, 0, sizeof(test_fb));
	test_fb.send_packet = fb_mock_send_packet;
	test_fb.state = COMMAND;
}

/* Setup fb, so only functions that explicitly call this can use disk and gpt */
int fastboot_disk_gpt_init(struct FastbootOps *fb)
{
	fb->disk = &test_disk;
	fb->gpt = &test_gpt;

	return 0;
}
