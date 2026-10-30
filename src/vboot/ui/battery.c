// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/ec/cros/ec.h"
#include "vboot/ui.h"

/*
 * The battery icons and the mapping from the battery status to the icons follow
 * the OTA recovery UI, so that the battery icon looks the same when the device
 * enters the OTA recovery UI from the firmware UI.
 */

/* Battery icon when the battery is charging or fully charged on AC power. */
static const struct ui_battery_icon charging_icon = {
	"ic_battery_android_bolt.bmp", &ui_color_fg,
};

/* Battery icons for the battery levels when not charging, from low to high. */
static const struct {
	/* Maximum battery level of the icon. */
	uint32_t max_level;
	struct ui_battery_icon icon;
} level_icons[] = {
	{ 18, { "ic_battery_android_alert.bmp", &ui_color_battery_alert } },
	{ 40, { "ic_battery_android_3.bmp", &ui_color_fg } },
	{ 60, { "ic_battery_android_4.bmp", &ui_color_fg } },
	{ 80, { "ic_battery_android_5.bmp", &ui_color_fg } },
	{ 100, { "ic_battery_android_full.bmp", &ui_color_fg } },
};

const struct ui_battery_icon *ui_battery_get_icon(
		const struct ui_battery *battery)
{
	if (!battery->present)
		return NULL;

	if (battery->charging)
		return &charging_icon;

	for (size_t i = 0; i < ARRAY_SIZE(level_icons); i++) {
		if (battery->level <= level_icons[i].max_level)
			return &level_icons[i].icon;
	}

	return &level_icons[ARRAY_SIZE(level_icons) - 1].icon;
}

/*
 * Read the battery status from the EC.
 *
 * @param battery	Battery status to be read. If the battery status is not
 *			available, battery->present will be false.
 */
static void read_battery(struct ui_battery *battery)
{
	uint32_t flags;
	uint32_t level;

	memset(battery, 0, sizeof(*battery));

	if (!CONFIG(DRIVER_EC_CROS))
		return;

	if (cros_ec_read_batt_flags(&flags))
		return;
	if (!(flags & EC_BATT_FLAG_BATT_PRESENT) ||
	    (flags & EC_BATT_FLAG_INVALID_DATA))
		return;
	if (cros_ec_read_batt_state_of_charge(&level))
		return;

	battery->present = true;
	battery->level = MIN(level, 100);
	/*
	 * Show the charging icon when the battery is charging, or when it is
	 * fully charged on AC power (the "Charging" and "Full" statuses in the
	 * OTA recovery UI). The EC sets neither EC_BATT_FLAG_CHARGING nor
	 * EC_BATT_FLAG_DISCHARGING for a fully charged battery on AC power.
	 * The battery level is not checked, because a fully charged battery
	 * may report a level below 100%.
	 */
	battery->charging = (flags & EC_BATT_FLAG_CHARGING) ||
			    ((flags & EC_BATT_FLAG_AC_PRESENT) &&
			     !(flags & EC_BATT_FLAG_DISCHARGING));
}

void ui_battery_update(struct ui_context *ui, uint32_t now_ms)
{
	struct ui_battery battery;

	if (ui->battery_updated &&
	    now_ms - ui->battery_update_time_ms < UI_BATTERY_UPDATE_INTERVAL_MS)
		return;

	read_battery(&battery);
	ui->battery_updated = true;
	ui->battery_update_time_ms = now_ms;

	if (battery.present != ui->battery.present ||
	    battery.level != ui->battery.level ||
	    battery.charging != ui->battery.charging) {
		UI_INFO("Battery: present=%d, level=%u, charging=%d\n",
			battery.present, battery.level, battery.charging);
		ui->battery = battery;
		ui->force_display = 1;
	}
}
