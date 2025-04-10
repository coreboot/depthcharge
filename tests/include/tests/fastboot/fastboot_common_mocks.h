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

/* Setup for get_slot_for_partition_name mock */
#define WILL_GET_SLOT_FOR_PARTITION_NAME(n, ret) do { \
	expect_string(fastboot_get_slot_for_partition_name, partition_name, n); \
	will_return(fastboot_get_slot_for_partition_name, ret); \
} while (0)

/* Setup for fastboot_get_kernel_for_slot mock */
#define WILL_GET_KERNEL_FOR_SLOT(slot_arg, entry) do { \
	expect_value(fastboot_get_kernel_for_slot, slot, slot_arg); \
	will_return(fastboot_get_kernel_for_slot, entry); \
} while (0)

/* Setup for IsAndroid mock */
#define WILL_CHECK_ANDROID(entry, ret) do { \
	expect_value(IsAndroid, e, entry); \
	will_return(IsAndroid, ret); \
} while (0)

/* Setup for GetEntryPriority mock */
#define WILL_GET_PRIORITY(entry, priority) do { \
	expect_value(GetEntryPriority, e, entry); \
	will_return(GetEntryPriority, priority); \
} while (0)

/* Setup for IsBootableEntry mock */
#define WILL_CHECK_BOOTABLE_ENTRY(entry, ret) do { \
	expect_value(IsBootableEntry, e, entry); \
	will_return(IsBootableEntry, ret); \
} while (0)

/* Setup for gpt_find_partition mock */
#define WILL_FIND_PARTITION(name, ret) do { \
	expect_string(gpt_find_partition, partition_name, name); \
	will_return(gpt_find_partition, ret); \
} while (0)

#endif /* _FASTBOOT_COMMON_MOCKS_H */
