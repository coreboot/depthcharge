/*
 * Copyright 2012 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __VBOOT_UTIL_ACPI_H__
#define __VBOOT_UTIL_ACPI_H__

#include <stdint.h>

enum {
	BOOT_REASON_OTHER = 0,
	BOOT_REASON_S3DIAG = 9
};

enum {
	CHSW_RECOVERY_X86 =     (1 << 1),
	CHSW_RECOVERY_EC =      (1 << 2),
	CHSW_DEVELOPER_SWITCH = (1 << 5),
	CHSW_FIRMWARE_WP_DIS =  (1 << 9)
};

enum {
	ACTIVE_MAINFW_RECOVERY = 0,
	ACTIVE_MAINFW_RW_A =     1,
	ACTIVE_MAINFW_RW_B =     2
};

enum {
	RECOVERY_REASON_NONE = 0,
	RECOVERY_REASON_ME = 1
};

enum {
	ACTIVE_ECFW_RO = 0,
	ACTIVE_ECFW_RW = 1
};

enum {
	FIRMWARE_TYPE_RECOVERY = 0,
	FIRMWARE_TYPE_NORMAL = 1,
	FIRMWARE_TYPE_DEVELOPER = 2
};

enum {
	BINF_RECOVERY = 0,
	BINF_RW_A = 1,
	BINF_RW_B = 2
};

#define ACPI_FWID_SIZE 64

/*
 * This structure is also used in coreboot. Any changes to this version have
 * to be made to that version as well.
 */
typedef struct {
	uint32_t boot_reason;          // 0x00
	uint32_t main_fw;              // 0x04
	uint32_t ec_fw;                // 0x08
	uint16_t chsw;                 // 0x0C
	uint8_t  hwid[256];            // 0x0e
	uint8_t  fwid[ACPI_FWID_SIZE]; // 0x10e
	uint8_t  frid[ACPI_FWID_SIZE]; // 0x14e
	uint32_t main_fw_type;         // 0x18e
	uint32_t recovery_reason;      // 0x192
	uint32_t fmap_base;            // 0x196
	uint8_t  vdat[3072];           // 0x19a
	uint32_t fwid_ptr;             // 0xd9a
	uint32_t me_hash[8];           // 0xd9e
	                               // 0xdbe
} __attribute__((packed)) chromeos_acpi_t;

#endif /* __VBOOT_UTIL_ACPI_H__ */
