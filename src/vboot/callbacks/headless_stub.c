// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google Inc.
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

#include <libpayload.h>
#include <vb2_api.h>

#include "vboot/ui.h"

uint32_t vb2ex_get_locale_count(void)
{
	return ui_get_locale_count();
}

vb2_error_t vb2ex_display_ui(enum vb2_screen screen, uint32_t locale_id,
			     uint32_t selected_item,
			     uint32_t disabled_item_mask,
			     enum vb2_error error_code)
{
	/* TODO(b/151200757): Support headless devices */
	return VB2_SUCCESS;
}

/* TODO(b/144969088): Rewrite for proper debug screen implementation. */
vb2_error_t VbExDisplayDebugInfo(const char *info_str, int full_info)
{
	return VB2_SUCCESS;
}
