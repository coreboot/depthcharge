/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _FASTBOOT_COMMON_MOCKS_H
#define _FASTBOOT_COMMON_MOCKS_H

#include "fastboot/fastboot.h"

/* Mock data */
extern struct FastbootOps test_fb;
extern BlockDev test_disk;
extern GptData test_gpt;

/* Setup test_fb with mocked send function and in command state */
void setup_test_fb(void);

/* Setup for send_packet mock */
#define WILL_SEND_EXACT(fb_ptr, data) do { \
	expect_value(fb_mock_send_packet, fb, fb_ptr); \
	expect_string(fb_mock_send_packet, buf, data); \
	expect_value(fb_mock_send_packet, len, strlen(data) + 1); \
} while (0)

#define WILL_SEND_PREFIX(fb_ptr, data) do { \
	expect_value(fb_mock_send_packet, fb, fb_ptr); \
	expect_memory(fb_mock_send_packet, buf, data, strlen(data)); \
	expect_in_range(fb_mock_send_packet, len, strlen(data) + 1, 256); \
} while (0)

#endif /* _FASTBOOT_COMMON_MOCKS_H */
