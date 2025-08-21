/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __BASE_ANDROID_MISC_CORE_H__
#define __BASE_ANDROID_MISC_CORE_H__

#include <commonlib/helpers.h>

/*
 * Structures in the file are imported from
 * https://googleplex-android.googlesource.com/platform/bootable/recovery/+/refs/heads/main/bootloader_message/include/bootloader_message/bootloader_message_core.h
 */
struct bootloader_message {
	char command[32];
	char status[32];
	char recovery[768];
	char stage[32];
	char reserved[1184];
};
_Static_assert(sizeof(struct bootloader_message) == 2048,
	       "bootloader_message size is incorrect");

/* 32K - 64K System space, used for miscellaneous AOSP features. */
#define MISC_SYSTEM_SPACE_OFFSET	(32 * KiB)
#define MISC_SYSTEM_SPACE_SIZE		(32 * KiB)

struct misc_virtual_ab_message {
	uint8_t version;
	uint32_t magic;
	uint8_t merge_status;
	uint8_t source_slot;
	uint8_t reserved[57];
} __attribute__((packed));
_Static_assert(sizeof(struct misc_virtual_ab_message) == 64,
	       "struct misc_virtual_ab_message has wrong size");

#define ANDROID_MISC_MEMTAG_MESSAGE_VERSION		1
#define ANDROID_MISC_MEMTAG_MAGIC			0x5afefe5a
#define ANDROID_MISC_MEMTAG_MODE_MEMTAG			0x1
#define ANDROID_MISC_MEMTAG_MODE_MEMTAG_ONCE		0x2
#define ANDROID_MISC_MEMTAG_MODE_MEMTAG_KERNEL		0x4
#define ANDROID_MISC_MEMTAG_MODE_MEMTAG_KERNEL_ONCE	0x8
#define ANDROID_MISC_MEMTAG_MODE_MEMTAG_OFF		0x10
#define ANDROID_MISC_MEMTAG_MODE_FORCED			0x20

struct misc_memtag_message {
	uint8_t version;
	uint32_t magic;
	uint32_t memtag_mode;
	uint8_t reserved[55];
} __attribute__((packed));
_Static_assert(sizeof(struct misc_memtag_message) == 64,
	       "struct misc_memtag_message has wrong size");

struct misc_kcmdline_message {
	uint8_t version;
	uint32_t magic;
	uint64_t kcmdline_flags;
	uint8_t reserved[51];
} __attribute__((packed));
_Static_assert(sizeof(struct misc_kcmdline_message) == 64,
	       "struct misc_kcmdline_message has wrong size");

struct misc_control_message {
	uint8_t version;
	uint32_t magic;
	uint64_t misctrl_flags;
	uint8_t reserved[51];
} __attribute__((packed));
_Static_assert(sizeof(struct misc_control_message) == 64,
	       "struct misc_control_message has wrong size");

struct misc_system_space {
	struct misc_virtual_ab_message virtual_ab_message;
	struct misc_memtag_message memtag_message;
	struct misc_kcmdline_message kcmdline_message;
	struct misc_control_message control_message;
	/* To align the entire `struct misc_system_space` with
	   the common sector size of 512 bytes. */
	uint8_t reserved[256];
} __attribute__((packed));
_Static_assert(sizeof(struct misc_system_space) % 512 == 0,
	       "prefer to extend by 512 byte chunks, for efficient");

#endif /* __BASE_ANDROID_MISC_CORE_H__ */
