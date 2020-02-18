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

#include <vb2_api.h>
#include <vboot_api.h>  /* for VbScreenData */

#include "vboot/ui.h"

_Static_assert(CONFIG(MENU_UI), "MENU_UI must be set");
_Static_assert(!CONFIG(LEGACY_MENU_UI), "LEGACY_MENU_UI not allowed");
_Static_assert(!CONFIG(LEGACY_CLAMSHELL_UI), "LEGACY_CLAMSHELL_UI not allowed");

vb2_error_t vb2ex_display_ui(enum vb2_screen screen, uint32_t locale)
{
	vb2_error_t rv;
	printf("%s: screen=%#x, locale=%u\n", __func__, screen, locale);

	struct ui_state state = {
		.screen = screen,
		.locale = locale,
	};
	static struct ui_state prev_state;
	static int has_prev_state = 0;

	rv = ui_display_screen(&state, has_prev_state ? &prev_state : NULL);

	if (rv) {
		has_prev_state = 0;
		/*
		 * TODO(yupingso): Add fallback display when drawing
		 * fails.
		 */
		return rv;
	}

	memcpy(&prev_state, &state, sizeof(struct ui_state));
	has_prev_state = 1;

	return VB2_SUCCESS;
}

/*
 * TODO(chromium:1055125): Before decoupling EC/AUXFW sync and UI, keep this
 * dummy function here as a workaround.
 */
vb2_error_t VbExDisplayScreen(uint32_t screen_type, uint32_t locale,
			      const VbScreenData *data)
{
	return VB2_SUCCESS;
}
