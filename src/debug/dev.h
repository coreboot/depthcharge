/*
 * Copyright 2014 Google Inc.
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

#ifndef __DEBUG_DEV_H__
#define __DEBUG_DEV_H__

void dc_dev_gdb_enter(void);
void dc_dev_gdb_exit(int exit_code);
void dc_dev_netboot(void);
void dc_dev_fastboot(void);

#endif /* __DEBUG_DEV_H__ */
