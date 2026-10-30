// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <vboot/ui.h>

#include "drivers/ec/cros/ec.h"

/* Mocks */

int cros_ec_read_batt_flags(uint32_t *flags)
{
	*flags = mock_type(uint32_t);
	return mock_type(int);
}

int cros_ec_read_batt_state_of_charge(uint32_t *state)
{
	*state = mock_type(uint32_t);
	return mock_type(int);
}

#define WILL_READ_BATTERY(flags, level) do { \
	will_return(cros_ec_read_batt_flags, flags); \
	will_return(cros_ec_read_batt_flags, 0); \
	will_return(cros_ec_read_batt_state_of_charge, level); \
	will_return(cros_ec_read_batt_state_of_charge, 0); \
} while (0)

#define BATT_DISCHARGING (EC_BATT_FLAG_BATT_PRESENT | EC_BATT_FLAG_DISCHARGING)
#define BATT_CHARGING (EC_BATT_FLAG_BATT_PRESENT | EC_BATT_FLAG_AC_PRESENT | \
		       EC_BATT_FLAG_CHARGING)
#define BATT_IDLE_ON_AC (EC_BATT_FLAG_BATT_PRESENT | EC_BATT_FLAG_AC_PRESENT)

/* Tests */

static void assert_icon(uint32_t level, bool charging, const char *file,
			const struct rgb_color *color)
{
	const struct ui_battery battery = {
		.present = true,
		.level = level,
		.charging = charging,
	};
	const struct ui_battery_icon *icon = ui_battery_get_icon(&battery);

	assert_non_null(icon);
	assert_string_equal(icon->file, file);
	/* Compare the values; each file has its own copy of the colors. */
	assert_int_equal(icon->color->red, color->red);
	assert_int_equal(icon->color->green, color->green);
	assert_int_equal(icon->color->blue, color->blue);
}

static void test_battery_icon_not_present(void **state)
{
	const struct ui_battery battery = { .present = false, .level = 50 };

	assert_null(ui_battery_get_icon(&battery));
}

static void test_battery_icon_levels(void **state)
{
	const struct rgb_color *alert = &ui_color_battery_alert;
	const struct rgb_color *fg = &ui_color_fg;

	assert_icon(0, false, "ic_battery_android_alert.bmp", alert);
	assert_icon(18, false, "ic_battery_android_alert.bmp", alert);
	assert_icon(19, false, "ic_battery_android_3.bmp", fg);
	assert_icon(40, false, "ic_battery_android_3.bmp", fg);
	assert_icon(41, false, "ic_battery_android_4.bmp", fg);
	assert_icon(60, false, "ic_battery_android_4.bmp", fg);
	assert_icon(61, false, "ic_battery_android_5.bmp", fg);
	assert_icon(80, false, "ic_battery_android_5.bmp", fg);
	assert_icon(81, false, "ic_battery_android_full.bmp", fg);
	assert_icon(100, false, "ic_battery_android_full.bmp", fg);
}

static void test_battery_icon_charging(void **state)
{
	const struct rgb_color *fg = &ui_color_fg;

	assert_icon(0, true, "ic_battery_android_bolt.bmp", fg);
	assert_icon(50, true, "ic_battery_android_bolt.bmp", fg);
	assert_icon(100, true, "ic_battery_android_bolt.bmp", fg);
}

static void test_battery_update_discharging(void **state)
{
	struct ui_context ui = { 0 };

	WILL_READ_BATTERY(BATT_DISCHARGING, 42);
	ui_battery_update(&ui, 1000);

	assert_true(ui.battery.present);
	assert_int_equal(ui.battery.level, 42);
	assert_false(ui.battery.charging);
	assert_true(ui.force_display);
}

static void test_battery_update_charging(void **state)
{
	struct ui_context ui = { 0 };

	WILL_READ_BATTERY(BATT_CHARGING, 42);
	ui_battery_update(&ui, 1000);

	assert_true(ui.battery.present);
	assert_true(ui.battery.charging);
}

static void test_battery_update_full_on_ac(void **state)
{
	struct ui_context ui = { 0 };

	/* A fully charged battery may report a level below 100%. */
	WILL_READ_BATTERY(BATT_IDLE_ON_AC, 96);
	ui_battery_update(&ui, 1000);

	assert_true(ui.battery.present);
	assert_int_equal(ui.battery.level, 96);
	assert_true(ui.battery.charging);
}

static void test_battery_update_discharging_on_ac(void **state)
{
	struct ui_context ui = { 0 };

	/* For example, the power source is too weak to charge the battery. */
	WILL_READ_BATTERY(BATT_DISCHARGING | EC_BATT_FLAG_AC_PRESENT, 90);
	ui_battery_update(&ui, 1000);

	assert_true(ui.battery.present);
	assert_false(ui.battery.charging);
}

static void test_battery_update_level_clamped(void **state)
{
	struct ui_context ui = { 0 };

	WILL_READ_BATTERY(BATT_DISCHARGING, 101);
	ui_battery_update(&ui, 1000);

	assert_int_equal(ui.battery.level, 100);
}

static void test_battery_update_no_battery(void **state)
{
	struct ui_context ui = { 0 };

	will_return(cros_ec_read_batt_flags, EC_BATT_FLAG_AC_PRESENT);
	will_return(cros_ec_read_batt_flags, 0);
	ui_battery_update(&ui, 1000);

	assert_false(ui.battery.present);
	assert_false(ui.force_display);
}

static void test_battery_update_invalid_data(void **state)
{
	struct ui_context ui = { 0 };

	will_return(cros_ec_read_batt_flags,
		    EC_BATT_FLAG_BATT_PRESENT | EC_BATT_FLAG_INVALID_DATA);
	will_return(cros_ec_read_batt_flags, 0);
	ui_battery_update(&ui, 1000);

	assert_false(ui.battery.present);
}

static void test_battery_update_flags_error(void **state)
{
	struct ui_context ui = { 0 };

	will_return(cros_ec_read_batt_flags, 0);
	will_return(cros_ec_read_batt_flags, -1);
	ui_battery_update(&ui, 1000);

	assert_false(ui.battery.present);
}

static void test_battery_update_level_error(void **state)
{
	struct ui_context ui = { 0 };

	will_return(cros_ec_read_batt_flags, BATT_DISCHARGING);
	will_return(cros_ec_read_batt_flags, 0);
	will_return(cros_ec_read_batt_state_of_charge, 0);
	will_return(cros_ec_read_batt_state_of_charge, -1);
	ui_battery_update(&ui, 1000);

	assert_false(ui.battery.present);
}

static void test_battery_update_interval(void **state)
{
	struct ui_context ui = { 0 };

	WILL_READ_BATTERY(BATT_DISCHARGING, 50);
	ui_battery_update(&ui, 1000);
	assert_int_equal(ui.battery.level, 50);
	ui.force_display = 0;

	/* Not updated within the interval. */
	ui_battery_update(&ui, 1000 + UI_BATTERY_UPDATE_INTERVAL_MS - 1);
	assert_int_equal(ui.battery.level, 50);
	assert_false(ui.force_display);

	/* Updated after the interval, but unchanged. */
	WILL_READ_BATTERY(BATT_DISCHARGING, 50);
	ui_battery_update(&ui, 1000 + UI_BATTERY_UPDATE_INTERVAL_MS);
	assert_false(ui.force_display);

	/* Updated and changed. */
	WILL_READ_BATTERY(BATT_DISCHARGING, 49);
	ui_battery_update(&ui, 1000 + UI_BATTERY_UPDATE_INTERVAL_MS * 2);
	assert_int_equal(ui.battery.level, 49);
	assert_true(ui.force_display);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_battery_icon_not_present),
		cmocka_unit_test(test_battery_icon_levels),
		cmocka_unit_test(test_battery_icon_charging),
		cmocka_unit_test(test_battery_update_discharging),
		cmocka_unit_test(test_battery_update_charging),
		cmocka_unit_test(test_battery_update_full_on_ac),
		cmocka_unit_test(test_battery_update_discharging_on_ac),
		cmocka_unit_test(test_battery_update_level_clamped),
		cmocka_unit_test(test_battery_update_no_battery),
		cmocka_unit_test(test_battery_update_invalid_data),
		cmocka_unit_test(test_battery_update_flags_error),
		cmocka_unit_test(test_battery_update_level_error),
		cmocka_unit_test(test_battery_update_interval),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
