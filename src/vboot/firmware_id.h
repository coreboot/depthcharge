/*
 * Copyright 2015 Google Inc.
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
 */

#ifndef __VBOOT_FIRMWARE_ID_H__
#define __VBOOT_FIRMWARE_ID_H__

enum {
	VBSD_RW_A = 0x0,
	VBSD_RW_B = 0x1,
	VBSD_RO = 0xFF,
	VBSD_RECOVERY = 0xFF,
	VBSD_UNKNOWN = 0x100,
};

/* Get firmware details by VbSharedDataHeader index */
const char *get_fw_id(int index);
int get_fw_size(int index);

/* Get firmware id for a particular type - ro, rwa, rwb */
const char *get_ro_fw_id(void);
const char *get_rwa_fw_id(void);
const char *get_rwb_fw_id(void);

/* Get firmware size for a particular type - ro, rwa, rwb */
int get_ro_fw_size(void);
int get_rwa_fw_size(void);
int get_rwb_fw_size(void);

/*
 * Get firmware details for currently active fw type. It looks up
 * VbSharedDataHeader from vboot_handoff, identifies fw_index, and
 * returns appropriate id and size for that index.
 */
const char *get_active_fw_id(void);
int get_active_fw_size(void);

#endif /* __VBOOT_FIRMWARE_ID_H__ */
