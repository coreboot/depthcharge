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

#ifndef __VBOOT_STAGES_H__
#define __VBOOT_STAGES_H__

#include <stdint.h>
#include <vboot_api.h>

int vboot_init(void);
int vboot_select_firmware(void);
int vboot_select_and_load_kernel(void);
int vboot_do_init_out_flags(uint32_t out_flags);
int vboot_in_recovery(void);
int vboot_in_developer(void);
void vboot_update_recovery(uint32_t request);
void vboot_boot_kernel(VbSelectAndLoadKernelParams *kparams);

#endif /* __VBOOT_STAGES_H__ */
