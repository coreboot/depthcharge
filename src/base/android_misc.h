/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2025 Google LLC. */

#ifndef __BASE_ANDROID_MISC_H__
#define __BASE_ANDROID_MISC_H__

#include <commonlib/helpers.h>

#include "android_misc_core.h"
#include "base/gpt.h"

/* Possible values of BCB command */
#define BCB_CMD_BOOTONCE_BOOTLOADER "bootonce-bootloader"
#define BCB_CMD_BOOT_RECOVERY "boot-recovery"

enum android_misc_bcb_command {
	MISC_BCB_NORMAL_BOOT,
	MISC_BCB_RECOVERY_BOOT,
	MISC_BCB_BOOTLOADER_BOOT,
	MISC_BCB_UNKNOWN,
	MISC_BCB_DISK_ERROR,
};

/*
 * Reserve space for android oem cmdline on misc partition. Use for that vendor
 * space which is at 2K - 16K range in misc. Skip 2K - 4K range as it may be
 * optionally used as bootloader_message_ab struct.
 */
#define MISC_VENDOR_SPACE_OEM_CMDLINE_OFFSET (1024 * 4)
#define MISC_VENDOR_SPACE_OEM_CMDLINE_SIZE (1024 * 2)

/*
 * Magic value for android_misc_oem_cmdline that identifies content of
 * the structure. Magic value is four ASCII characters in little-endian order.
 * These are hex values for ASCII "FCML".
 */
#define ANDROID_MISC_OEM_CMDLINE_MAGIC 0x4c4d4346

struct android_misc_oem_cmdline {
	uint32_t magic;
	uint8_t version;
	/* Reserved for future use */
	uint8_t reserved;
	/*
	 * ipchksum is calculated for struct android_misc_oem_cmdline
	 * up to the last byte of bootconfig
	 */
	uint16_t chksum;
	/* Length of cmdline, including the \0 at the end */
	uint16_t cmdline_len;
	/* Length of bootconfig, including the \0 at the end */
	uint16_t bootconfig_len;
	/* Data contains cmdline starting at offset 0 followed by \0 and bootconfig */
	char data[2036];
} __attribute__((packed));
_Static_assert(sizeof(struct android_misc_oem_cmdline) ==
	       MISC_VENDOR_SPACE_OEM_CMDLINE_SIZE,
	       "android_misc_oem_cmdline size is incorrect");

bool is_android_misc_cmd_separator(const char c, bool bootconfig);
bool is_android_misc_oem_cmdline_valid(struct android_misc_oem_cmdline *cmd);
int android_misc_oem_cmdline_update_checksum(struct android_misc_oem_cmdline *cmd);
int android_misc_oem_cmdline_write(BlockDev *disk, GptData *gpt,
				   struct android_misc_oem_cmdline *cmd);
int android_misc_oem_cmdline_read(BlockDev *disk, GptData *gpt,
				  struct android_misc_oem_cmdline *cmd);
char *android_misc_get_oem_cmd(struct android_misc_oem_cmdline *cmd, bool bootconfig);
int android_misc_append_oem_cmd(struct android_misc_oem_cmdline *cmd, const char *buf,
				bool bootconfig);
int android_misc_del_oem_cmd(struct android_misc_oem_cmdline *cmd, const size_t idx,
			     const size_t len, bool bootconfig);
int android_misc_set_oem_cmd(struct android_misc_oem_cmdline *cmd, const char *buf,
			     bool bootconfig);
void android_misc_reset_oem_cmd(struct android_misc_oem_cmdline *cmd);
int android_misc_bcb_write(BlockDev *disk, GptData *gpt, struct bootloader_message *bcb);
int android_misc_bcb_read(BlockDev *disk, GptData *gpt, struct bootloader_message *bcb);
enum android_misc_bcb_command android_misc_get_bcb_command(BlockDev *disk, GptData *gpt);
int android_misc_system_space_write(BlockDev *disk, GptData *gpt,
				    struct misc_system_space *space);
int android_misc_system_space_read(BlockDev *disk, GptData *gpt,
				   struct misc_system_space *space);

#endif /* __BASE_ANDROID_MISC_H__ */
