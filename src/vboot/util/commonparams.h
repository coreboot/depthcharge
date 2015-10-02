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

#ifndef __VBOOT_UTIL_COMMONPARAMS_H__
#define __VBOOT_UTIL_COMMONPARAMS_H__

#include <stddef.h>
#include <stdint.h>
#include <vboot_api.h>

extern VbCommonParams cparams;
extern uint8_t shared_data_blob[VB_SHARED_DATA_REC_SIZE];

int gbb_copy_in_bmp_block();
int common_params_init(int clear_shared_data);
int is_cparams_initialized(void);
// Implemented by each arch.
int find_common_params(void **blob, int *size);

#endif /* __VBOOT_UTIL_COMMONPARAMS_H__ */
