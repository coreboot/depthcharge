// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>
#include <vboot_api.h>

#include "vboot/ui.h"

static vb2_error_t ui_error_exit_action(struct ui_context *ui)
{
	/*
	 * If an error message is currently shown on the screen, any
	 * key press clears that error.  Unset the key so that it is
	 * not processed by other action functions.
	 */
	if (ui->key && ui->state->error_code) {
		ui->state->error_code = UI_ERROR_NONE;
		ui->key = 0;
	}
	return VB2_SUCCESS;
}

static vb2_error_t ui_menu_navigation_action(struct ui_context *ui)
{
	uint32_t key = ui->key;

	/* Map detachable button presses for simplicity. */
	if (CONFIG(DETACHABLE)) {
		if (key == UI_BUTTON_VOL_UP_SHORT_PRESS)
			key = UI_KEY_UP;
		else if (key == UI_BUTTON_VOL_DOWN_SHORT_PRESS)
			key = UI_KEY_DOWN;
		else if (key == UI_BUTTON_POWER_SHORT_PRESS)
			key = UI_KEY_ENTER;
	}

	switch (key) {
	case UI_KEY_UP:
		return ui_menu_prev(ui);
	case UI_KEY_DOWN:
		return ui_menu_next(ui);
	case UI_KEY_ENTER:
		return ui_menu_select(ui);
	case UI_KEY_ESC:
		return ui_screen_back(ui);
	default:
		if (key != 0)
			UI_INFO("Pressed key %#x, trusted? %d\n", ui->key,
				ui->key_trusted);
	}

	return VB2_SUCCESS;
}

static vb2_error_t check_shutdown_request(struct ui_context *ui)
{
	int shutdown_power;

	/* If desired, ignore shutdown request due to lid closure. */
	if (!(vb2api_gbb_get_flags(ui->ctx) &
	      VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN) &&
	    !ui_is_lid_open())
		return VB2_REQUEST_SHUTDOWN;

	/*
	 * In detachables, power button is used for menu selection.
	 * Therefore, return early to skip processing power button states.
	 */
	if (CONFIG(DETACHABLE))
		return VB2_SUCCESS;

	shutdown_power = ui_is_power_pressed();

	/*
	 * Ignore power button push until after we have seen it released.
	 * This avoids shutting down immediately if the power button is still
	 * being held on startup. After we've recognized a valid power button
	 * push then don't report the event until after the button is released.
	 */
	if (shutdown_power) {
		shutdown_power = 0;
		if (ui->power_button == UI_POWER_BUTTON_RELEASED)
			ui->power_button = UI_POWER_BUTTON_PRESSED;
	} else {
		if (ui->power_button == UI_POWER_BUTTON_PRESSED)
			shutdown_power = 1;
		ui->power_button = UI_POWER_BUTTON_RELEASED;
	}

	if (ui->key == UI_BUTTON_POWER_SHORT_PRESS)
		shutdown_power = 1;

	return shutdown_power ? VB2_REQUEST_SHUTDOWN : VB2_SUCCESS;
}

vb2_error_t ui_init_context(struct ui_context *ui, struct vb2_context *ctx,
			    enum ui_screen screen_id)
{
	const struct ui_screen_info *screen_info;
	const struct ui_locale *locale;
	const static struct ui_locale en_locale = {
		.id = 0,
		.code = "en",
		.rtl = 0,
	};
	vb2_error_t rv;

	screen_info = ui_get_screen_info(screen_id);
	if (!screen_info) {
		UI_ERROR("Screen entry %#x not found\n", screen_id);
		return VB2_ERROR_UI_INVALID_SCREEN;
	}

	uint32_t locale_id = vb2api_get_locale_id(ctx);
	rv = ui_get_locale_info(locale_id, &locale);
	if (rv == VB2_ERROR_UI_INVALID_LOCALE && locale_id != 0) {
		UI_WARN("Locale %u not found, falling back to locale 0",
			locale_id);
		locale_id = 0;
		rv = ui_get_locale_info(locale_id, &locale);
	}
	if (rv) {
		UI_WARN("Fallback locale %u still not found, assuming \"en\"\n",
			locale_id);
		locale = &en_locale;
	}

	/*
	 * We cannot use ui_screen_change() to initialize ui->state, because
	 * ui_screen_change() will call the screen's init(), which may need
	 * ui->state->locale (for example the debug info screen).
	 */
	memset(ui, 0, sizeof(*ui));
	ui->ctx = ctx;
	ui->state = xzalloc(sizeof(*ui->state));
	ui->state->screen = screen_info;
	ui->state->locale = locale;
	return ui_screen_init(ui);
}

static vb2_error_t ui_loop_impl(
	struct vb2_context *ctx, enum ui_screen root_screen_id,
	vb2_error_t (*global_action)(struct ui_context *ui),
	struct vb2_kernel_params *kparams)
{
	struct ui_context ui;
	struct ui_state prev_state;
	int need_redraw;
	const struct ui_menu *menu;
	uint32_t key_flags;
	uint32_t start_time_ms, elapsed_ms;
	vb2_error_t rv;

	if (ui_init_context(&ui, ctx, root_screen_id))
		die("Cannot init UI context for root screen %#x\n",
		    root_screen_id);
	ui.kparams = kparams;

	memset(&prev_state, 0, sizeof(prev_state));
	need_redraw = 1;

	while (1) {
		start_time_ms = vb2ex_mtime();

		/* Draw if there are state changes. */
		if (memcmp(&prev_state, ui.state, sizeof(*ui.state)) ||
		    /* Beep. */
		    ui.error_beep != 0 ||
		    /* Redraw on a screen request to refresh. */
		    ui.force_display) {

			menu = ui_get_menu(&ui);
			UI_INFO("<%s> menu item <%s>\n",
				ui.state->screen->name ? ui.state->screen->name
					: "null",
				menu->num_items
					? menu->items[ui.state->selected_item]
						  .name
					: "null");
			rv = ui_display(&ui, need_redraw ? NULL : &prev_state);
			/* If the drawing failed, set the flag so that NULL will
			   be passed to ui_display() in the next iteration. */
			need_redraw = !!rv;

			if (ui.error_beep ||
			    (ui.state->error_code &&
			     prev_state.error_code != ui.state->error_code)) {
				ui_beep(250, 400);
				ui.error_beep = 0;
			}

			/* Reset refresh flag. */
			ui.force_display = 0;

			/* Update prev variables. */
			memcpy(&prev_state, ui.state, sizeof(*ui.state));
		}

		/* Grab new keyboard input. */
		ui.key = ui_keyboard_read(&key_flags);
		ui.key_trusted = !!(key_flags & UI_KEY_FLAG_TRUSTED_KEYBOARD);

		/* Check for shutdown request. */
		rv = check_shutdown_request(&ui);
		if (rv && rv != VB2_REQUEST_UI_CONTINUE) {
			UI_INFO("Shutdown requested!\n");
			return rv;
		}

		/* Check if we need to exit an error box. */
		rv = ui_error_exit_action(&ui);
		if (rv && rv != VB2_REQUEST_UI_CONTINUE)
			return rv;

		/*
		 * Run menu navigation action. This has to be run before
		 * functions that may change the screen (such as screen actions
		 * and global actions). Otherwise menu navigation action would
		 * run on the new screen instead of the one shown to the user.
		 */
		rv = ui_menu_navigation_action(&ui);
		if (rv && rv != VB2_REQUEST_UI_CONTINUE)
			return rv;

		/* Run screen action. */
		if (ui.state->screen->action) {
			rv = ui.state->screen->action(&ui);
			if (rv && rv != VB2_REQUEST_UI_CONTINUE)
				return rv;
		}

		/* Run global action function if available. */
		if (global_action) {
			rv = global_action(&ui);
			if (rv && rv != VB2_REQUEST_UI_CONTINUE)
				return rv;
		}

		/* Delay. */
		elapsed_ms = vb2ex_mtime() - start_time_ms;
		if (elapsed_ms < UI_KEY_DELAY_MS)
			vb2ex_msleep(UI_KEY_DELAY_MS - elapsed_ms);
	}

	return VB2_SUCCESS;
}

vb2_error_t ui_loop(struct vb2_context *ctx, enum ui_screen root_screen_id,
		    vb2_error_t (*global_action)(struct ui_context *ui),
		    struct vb2_kernel_params *kparams)
{
	vb2_error_t rv = ui_loop_impl(ctx, root_screen_id, global_action,
				      kparams);
	if (rv == VB2_REQUEST_UI_EXIT)
		return VB2_SUCCESS;
	return rv;
}
