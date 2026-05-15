/* SPDX-License-Identifier: BSD-3-Clause */

#include <libpayload.h>
#include "base/cleanup_funcs.h"
#include "base/init_funcs.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/power/power.h"
#include "drivers/soc/qcom_spmi.h"
#include "drivers/soc/qcom_tsens.h"
#include "vboot/stages.h"

#define PMIC_CORE_REGISTERS_ADDR 0x0C500000
#define PMIC_REG_CHAN0_ADDR 0x0C402000
#define PMIC_REG_LAST_CHAN_ADDR 0x3000
#define PMIC_REG_FIRST_CHAN_ADDR 0x2000

#define SMB1_SLAVE_ID 0x07
#define SMB2_SLAVE_ID 0x0A
#define SCHG_CHGR_MAX_FAST_CHARGE_CURRENT_CFG 0x2666
#define SMB1_CHGR_MAX_FCC_CFG ((SMB1_SLAVE_ID << 16) | SCHG_CHGR_MAX_FAST_CHARGE_CURRENT_CFG)
#define SMB2_CHGR_MAX_FCC_CFG ((SMB2_SLAVE_ID << 16) | SCHG_CHGR_MAX_FAST_CHARGE_CURRENT_CFG)
#define SCHG_CHGR_CHARGING_ENABLE_CMD 0x2642
#define SMB1_CHGR_CHRG_EN_CMD ((SMB1_SLAVE_ID << 16) | SCHG_CHGR_CHARGING_ENABLE_CMD)
#define SMB2_CHGR_CHRG_EN_CMD ((SMB2_SLAVE_ID << 16) | SCHG_CHGR_CHARGING_ENABLE_CMD)

#define FCC_1A_STEP_50MA 0x14
#define FCC_DISABLE 0x8c

enum charging_status {
	CHRG_DISABLE,
	CHRG_ENABLE,
};

static void disable_slow_battery_charging(void)
{
	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
				    PMIC_REG_CHAN0_ADDR,
				    PMIC_REG_LAST_CHAN_ADDR - PMIC_REG_FIRST_CHAN_ADDR);

	/* Disable charging */
	pmic_spmi->write8(pmic_spmi, SMB1_CHGR_CHRG_EN_CMD, CHRG_DISABLE);
	pmic_spmi->write8(pmic_spmi, SMB2_CHGR_CHRG_EN_CMD, CHRG_DISABLE);
	pmic_spmi->write8(pmic_spmi, SMB1_CHGR_MAX_FCC_CFG, FCC_DISABLE);
	pmic_spmi->write8(pmic_spmi, SMB2_CHGR_MAX_FCC_CFG, FCC_DISABLE);

	free(pmic_spmi);
	printf("Disabled slow battery charging!\n");
}

static int enable_slow_battery_charging(void)
{
	/* Don't enable slow charging when no battery */
	if (CONFIG(DRIVER_EC_CROS) && !cros_ec_is_battery_present()) {
		printf("No battery, do not enable slow charging.\n");
		return 0;
	}

	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
				    PMIC_REG_CHAN0_ADDR,
				    PMIC_REG_LAST_CHAN_ADDR - PMIC_REG_FIRST_CHAN_ADDR);

	/* Enable charging */
	pmic_spmi->write8(pmic_spmi, SMB1_CHGR_MAX_FCC_CFG, FCC_1A_STEP_50MA);
	pmic_spmi->write8(pmic_spmi, SMB2_CHGR_MAX_FCC_CFG, FCC_1A_STEP_50MA);
	pmic_spmi->write8(pmic_spmi, SMB1_CHGR_CHRG_EN_CMD, CHRG_ENABLE);
	pmic_spmi->write8(pmic_spmi, SMB2_CHGR_CHRG_EN_CMD, CHRG_ENABLE);

	free(pmic_spmi);
	printf("Enable slow battery charging!\n");

	return 0;
}

INIT_FUNC(enable_slow_battery_charging);

static int board_cleanup(struct CleanupFunc *cleanup, CleanupType type)
{
	disable_slow_battery_charging();
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
