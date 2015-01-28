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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __FASTBOOT_FASTBOOT_H__
#define __FASTBOOT_FASTBOOT_H__

/* Fastboot getvar variables */
typedef enum {
	FB_VERSION,
	FB_BOOTLOADER_VERSION,
	FB_BASEBAND_VERSION,
	FB_PRODUCT,
	FB_SERIAL_NO,
	FB_SECURE,
	FB_DWNLD_SIZE,
	FB_PART_TYPE,
	FB_PART_SIZE,
} fb_getvar_t;

typedef enum fb_ret {
	FB_SUCCESS,
	FB_NORMAL_BOOT,
	FB_BOOT_MEMORY,
	FB_REBOOT,
	FB_REBOOT_BOOTLOADER,
	FB_POWEROFF,
	FB_RECV_DATA,
	FB_CONTINUE_RECOVERY,
}fb_ret_type;

fb_ret_type device_mode_enter(void);
int get_board_var(fb_getvar_t var, const char *input, size_t input_len,
		  char *str, size_t str_len);

int board_should_enter_device_mode(void);

#endif /* __FASTBOOT_FASTBOOT_H__ */
