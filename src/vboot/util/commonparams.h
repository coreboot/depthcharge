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
 */

#ifndef __VBOOT_UTIL_COMMONPARAMS_H__
#define __VBOOT_UTIL_COMMONPARAMS_H__

#include <stddef.h>
#include <stdint.h>
#include <vb2_api.h>

int gbb_clear_flags(void);

int vboot_create_vbsd(void);
int find_common_params(void **blob, int *size);

struct vb2_context *vboot_get_context(void);

#endif /* __VBOOT_UTIL_COMMONPARAMS_H__ */
