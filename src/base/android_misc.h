/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2025 Google LLC. */

#ifndef __BASE_ANDROID_MISC_H__
#define __BASE_ANDROID_MISC_H__

#include "base/gpt.h"

/* BCB structure from Android recovery bootloader_message.h */
struct bootloader_message {
	char command[32];
	char status[32];
	char recovery[768];
	char stage[32];
	char reserved[1184];
};
_Static_assert(sizeof(struct bootloader_message) == 2048,
	       "bootloader_message size is incorrect");

/* Possible values of BCB command */
#define BCB_CMD_BOOTONCE_BOOTLOADER "bootonce-bootloader"
#define BCB_CMD_BOOT_RECOVERY "boot-recovery"

int android_misc_bcb_write(BlockDev *disk, GptData *gpt, struct bootloader_message *bcb);
int android_misc_bcb_read(BlockDev *disk, GptData *gpt, struct bootloader_message *bcb);
enum android_misc_bcb_command android_misc_get_bcb_command(BlockDev *disk, GptData *gpt);

#endif /* __BASE_ANDROID_MISC_H__ */
