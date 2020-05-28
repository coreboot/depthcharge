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

#include "vboot/ui.h"

_Static_assert(CONFIG(MENU_UI), "MENU_UI must be set");
_Static_assert(!CONFIG(LEGACY_MENU_UI), "LEGACY_MENU_UI not allowed");
_Static_assert(!CONFIG(LEGACY_CLAMSHELL_UI), "LEGACY_CLAMSHELL_UI not allowed");

uint32_t vb2ex_get_locale_count(void)
{
	return ui_get_locale_count();
}

vb2_error_t vb2ex_display_ui(enum vb2_screen screen,
			     uint32_t locale_id,
			     uint32_t selected_item,
			     uint32_t disabled_item_mask)
{
	vb2_error_t rv;
	const struct ui_locale *locale = NULL;
	const struct ui_screen_info *screen_info;
	printf("%s: screen=%#x, locale=%u, selected_item=%u, "
	       "disabled_item_mask=%#x\n", __func__,
	       screen, locale_id, selected_item, disabled_item_mask);

	rv = ui_get_locale_info(locale_id, &locale);
	if (rv == VB2_ERROR_UI_INVALID_LOCALE) {
		printf("Locale %u not found, falling back to locale 0",
		       locale_id);
		rv = ui_get_locale_info(0, &locale);
	}
	if (rv)
		goto fail;

	screen_info = ui_get_screen_info(screen);
	if (!screen_info) {
		printf("%s: Not a valid screen: %#x\n", __func__, screen);
		rv = VB2_ERROR_UI_INVALID_SCREEN;
		goto fail;
	}

	struct ui_state state = {
		.screen = screen_info,
		.locale = locale,
		.selected_item = selected_item,
		.disabled_item_mask = disabled_item_mask,
	};
	static struct ui_state prev_state;
	static int has_prev_state = 0;

	rv = ui_display_screen(&state, has_prev_state ? &prev_state : NULL);
	if (rv)
		goto fail;

	memcpy(&prev_state, &state, sizeof(struct ui_state));
	has_prev_state = 1;

	return VB2_SUCCESS;

 fail:
	has_prev_state = 0;
	/* TODO(yupingso): Add fallback display when drawing fails. */
	return rv;
}
