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
	if (ui->key && ui->error_code) {
		ui->error_code = VB2_UI_ERROR_NONE;
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
	uint32_t shutdown_request = VbExIsShutdownRequested();

	/*
	 * Ignore power button push until after we have seen it released.
	 * This avoids shutting down immediately if the power button is still
	 * being held on startup. After we've recognized a valid power button
	 * push then don't report the event until after the button is released.
	 */
	if (shutdown_request & VB_SHUTDOWN_REQUEST_POWER_BUTTON) {
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		if (ui->power_button == UI_POWER_BUTTON_RELEASED)
			ui->power_button = UI_POWER_BUTTON_PRESSED;
	} else {
		if (ui->power_button == UI_POWER_BUTTON_PRESSED)
			shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		ui->power_button = UI_POWER_BUTTON_RELEASED;
	}

	if (ui->key == UI_BUTTON_POWER_SHORT_PRESS)
		shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;

	/* If desired, ignore shutdown request due to lid closure. */
	if (vb2api_gbb_get_flags(ui->ctx) & VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN)
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_LID_CLOSED;

	/*
	 * In detachables, disable shutdown due to power button.
	 * It is used for menu selection instead.
	 */
	if (CONFIG(DETACHABLE))
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_POWER_BUTTON;

	if (shutdown_request)
		return VB2_REQUEST_SHUTDOWN;

	return VB2_SUCCESS;
}

static vb2_error_t ui_loop_impl(
	struct vb2_context *ctx, enum vb2_screen root_screen_id,
	vb2_error_t (*global_action)(struct ui_context *ui))
{
	struct ui_context ui;
	struct ui_state prev_state;
	int prev_disable_timer;
	enum vb2_ui_error prev_error_code;
	const struct ui_menu *menu;
	const struct ui_screen_info *root_info;
	uint32_t key_flags;
	uint32_t start_time_ms, elapsed_ms;
	vb2_error_t rv;

	memset(&ui, 0, sizeof(ui));
	ui.ctx = ctx;
	root_info = ui_get_screen_info(root_screen_id);
	if (root_info == NULL)
		die("Root screen not found.\n");
	ui.locale_id = vb2api_get_locale_id(ctx);

	rv = ui_screen_change(&ui, root_screen_id);
	if (rv && rv != VB2_REQUEST_UI_CONTINUE)
		return rv;

	memset(&prev_state, 0, sizeof(prev_state));
	prev_disable_timer = 0;
	prev_error_code = VB2_UI_ERROR_NONE;

	while (1) {
		start_time_ms = vb2ex_mtime();

		/* Draw if there are state changes. */
		if (memcmp(&prev_state, ui.state, sizeof(*ui.state)) ||
		    /* Redraw when timer is disabled. */
		    prev_disable_timer != ui.disable_timer ||
		    /* Redraw/beep on a transition. */
		    prev_error_code != ui.error_code ||
		    /* Beep. */
		    ui.error_beep != 0 ||
		    /* Redraw on a screen request to refresh. */
		    ui.force_display) {

			menu = ui_get_menu(&ui);
			UI_INFO("<%s> menu item <%s>\n", ui.state->screen->name,
				menu->num_items
					? menu->items[ui.state->selected_item]
						  .name
					: "null");
			vb2ex_display_ui(ui.state->screen->id, ui.locale_id,
					 ui.state->selected_item,
					 ui.state->disabled_item_mask,
					 ui.state->hidden_item_mask,
					 ui.disable_timer,
					 ui.state->current_page,
					 ui.error_code);
			if (ui.error_beep ||
			    (ui.error_code &&
			     prev_error_code != ui.error_code)) {
				vb2ex_beep(250, 400);
				ui.error_beep = 0;
			}

			/* Reset refresh flag. */
			ui.force_display = 0;

			/* Update prev variables. */
			memcpy(&prev_state, ui.state, sizeof(*ui.state));
			prev_disable_timer = ui.disable_timer;
			prev_error_code = ui.error_code;
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

vb2_error_t ui_loop(struct vb2_context *ctx, enum vb2_screen root_screen_id,
		    vb2_error_t (*global_action)(struct ui_context *ui))
{
	vb2_error_t rv = ui_loop_impl(ctx, root_screen_id, global_action);
	if (rv == VB2_REQUEST_UI_EXIT)
		return VB2_SUCCESS;
	return rv;
}
