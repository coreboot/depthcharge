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

#include "drivers/ec/cros/ec.h"  /* TODO(b/144969088): VbExDisplayDebugInfo */
#include "drivers/tpm/tpm.h"  /* TODO(b/144969088): VbExDisplayDebugInfo */
#include "vboot/firmware_id.h"  /* TODO(b/144969088): VbExDisplayDebugInfo */
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
			     uint32_t disabled_item_mask,
			     int timer_disabled,
			     enum vb2_ui_error error_code)
{
	vb2_error_t rv;
	const struct ui_locale *locale = NULL;
	const struct ui_screen_info *screen_info;
	printf("%s: screen=%#x, locale=%u, selected_item=%u, "
	       "disabled_item_mask=%#x, timer_disabled=%d, error=%#x\n",
	       __func__,
	       screen, locale_id, selected_item, disabled_item_mask,
	       timer_disabled, error_code);

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
		.timer_disabled = timer_disabled,
		.error_code = error_code,
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

/* TODO(b/144969088): Rewrite for proper debug screen implementation. */
vb2_error_t VbExDisplayDebugInfo(const char *info_str, int full_info)
{
	char buf[1024];

	if (!full_info) {
		strncpy(buf, info_str, sizeof(buf) - 2);
		buf[sizeof(buf) - 1] = '\0';
	} else {
		char *tpm_str = NULL;
		char batt_pct_str[16];

		if (CONFIG(MOCK_TPM))
			tpm_str = "MOCK TPM";
		else if (CONFIG(DRIVER_TPM))
			tpm_str = tpm_report_state();

		if (!tpm_str)
			tpm_str = "(unsupported)";

		if (!CONFIG(DRIVER_EC_CROS))
			strcpy(batt_pct_str, "(unsupported)");
		else {
			uint32_t batt_pct;
			if (cros_ec_read_batt_state_of_charge(&batt_pct))
				strcpy(batt_pct_str, "(read failure)");
			else
				snprintf(batt_pct_str, sizeof(batt_pct_str),
					 "%u%%", batt_pct);
		}
		snprintf(buf, sizeof(buf),
			 "%s"	// vboot output includes newline
			 "read-only firmware id: %s\n"
			 "active firmware id: %s\n"
			 "battery level: %s\n"
			 "TPM state: %s",
			 info_str, get_ro_fw_id(), get_active_fw_id(),
			 batt_pct_str, tpm_str);
	}
	return print_fallback_message((const char *)buf);
}
