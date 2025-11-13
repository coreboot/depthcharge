/* SPDX-License-Identifier: BSD-3-Clause */

#include <libpayload.h>
#include "base/cleanup_funcs.h"
#include "base/init_funcs.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/power/power.h"
#include "drivers/soc/qcom_spmi.h"

#define PMIC_CORE_REGISTERS_ADDR 0x0C500000
#define PMIC_REG_CHAN0_ADDR 0x0C402000
#define PMIC_REG_LAST_CHAN_ADDR 0x3000
#define PMIC_REG_FIRST_CHAN_ADDR 0x2000

#define SMB1_SLAVE_ID 0x07
#define SMB2_SLAVE_ID 0x0A
#define SCHG_CHGR_CHARGING_ENABLE_CMD 0x2642
#define SMB1_CHGR_CHRG_EN_CMD ((SMB1_SLAVE_ID << 16) | SCHG_CHGR_CHARGING_ENABLE_CMD)
#define SMB2_CHGR_CHRG_EN_CMD ((SMB2_SLAVE_ID << 16) | SCHG_CHGR_CHARGING_ENABLE_CMD)

#define CHRG_DISABLE 0x00

#define DELAY_CHARGING_APPLET_MS 2000 /* 2ms */

#define SCHG_CHGR_CHARGING_FCC 0x260A
#define SMB1_CHGR_CHARGING_FCC ((SMB1_SLAVE_ID << 16) | SCHG_CHGR_CHARGING_FCC)
#define SMB2_CHGR_CHARGING_FCC ((SMB2_SLAVE_ID << 16) | SCHG_CHGR_CHARGING_FCC)

static bool board_boot_in_low_battery_mode(void)
{
	return lib_sysinfo.boot_mode == CB_BOOT_MODE_LOW_BATTERY;
}

static int get_battery_icurr_ma(void)
{
	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
				    PMIC_REG_CHAN0_ADDR,
				    PMIC_REG_LAST_CHAN_ADDR - PMIC_REG_FIRST_CHAN_ADDR);

	/* Read battery i-current value */
	int icurr = pmic_spmi->read8(pmic_spmi, SMB1_CHGR_CHARGING_FCC);
	if (icurr <= 0)
		icurr = pmic_spmi->read8(pmic_spmi, SMB2_CHGR_CHARGING_FCC);
	if (icurr < 0)
		icurr = 0;

	icurr *= 50;
	free(pmic_spmi);
	return icurr;
}

static void disable_battery_charging(void)
{
	if (!board_boot_in_low_battery_mode())
		return;

	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
				    PMIC_REG_CHAN0_ADDR,
				    PMIC_REG_LAST_CHAN_ADDR - PMIC_REG_FIRST_CHAN_ADDR);

	/* Disable charging */
	pmic_spmi->write8(pmic_spmi, SMB1_CHGR_CHRG_EN_CMD, CHRG_DISABLE);
	pmic_spmi->write8(pmic_spmi, SMB2_CHGR_CHRG_EN_CMD, CHRG_DISABLE);

	free(pmic_spmi);
	printf("Disabled battery charging!\n");
}

static int board_cleanup(struct CleanupFunc *cleanup, CleanupType type)
{
	disable_battery_charging();
	return 0;
}

static int board_cleanup_install(void)
{
	static CleanupFunc dev =
	{
		&board_cleanup,
		CleanupOnHandoff | CleanupOnLegacy,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(board_cleanup_install);

static void clear_ec_power_button_input(void)
{
	cros_ec_clear_host_events(EC_HOST_EVENT_MASK(EC_HOST_EVENT_POWER_BUTTON));
}

static int detect_ec_power_button_input(void)
{
	const uint32_t pwr_btn_mask = EC_HOST_EVENT_MASK(EC_HOST_EVENT_POWER_BUTTON);
	uint32_t events;

	if ((cros_ec_get_host_events(&events) == 0) && (events & pwr_btn_mask)) {
		cros_ec_clear_host_events(pwr_btn_mask);
		return 1;
	}

	return 0;
}

static int detect_ec_ac_disconnect_input(void)
{
	const uint32_t ac_disc_mask = EC_HOST_EVENT_MASK(EC_HOST_EVENT_AC_DISCONNECTED);
	uint32_t events;

	if ((cros_ec_get_host_events(&events) == 0) && (events & ac_disc_mask)) {
		cros_ec_clear_host_events(ac_disc_mask);
		return 1;
	}

	return 0;
}

static int launch_charger_applet(void)
{
	if (!CONFIG(DRIVER_EC_CROS) || !board_boot_in_low_battery_mode())
		return 0;

	printf("Inside %s. Initiating charging\n", __func__);

	/* clear any pending power button press event */
	clear_ec_power_button_input();

	do {
		/*
		 * Read the charger status, bail out if not present or not
		 * charging then issue a shutdown
		 */
		if (detect_ec_ac_disconnect_input() || !get_battery_icurr_ma())
			cros_ec_ap_poweroff();

		/*
		 * Sample the Power Button press and exit the charging loop.
		 *
		 * Reset the device to ensure a fresh boot to OS.
		 * This is required to avoid any kind of tear-down due to ADSP-lite
		 * being loaded and need some clean up before loading ADSP firmware by
		 * linux kernel.
		 */
		if (detect_ec_power_button_input())
			reboot();

		/* Add static delay before reading the charging applet pre-requisites */
		mdelay(DELAY_CHARGING_APPLET_MS);
	} while (true);

	return 0;
}

/*
 * Launch charging applet for below boot reasons:
 *  - boot in low-battery mode
 *  - boot in off-mode charging
 */
INIT_FUNC(launch_charger_applet);
