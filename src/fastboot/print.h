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

#ifndef __FASTBOOT_PRINT_H__
#define __FASTBOOT_PRINT_H__

typedef enum {
	PRINT_INFO,
	PRINT_WARN,
	PRINT_ERR,
} fb_msg_t;

void fb_print_text_on_screen(fb_msg_t type, const char *msg, size_t len);

#endif /* __FASTBOOT_PRINT_H__ */
