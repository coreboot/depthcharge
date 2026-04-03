/* SPDX-License-Identifier: BSD-3-Clause */

#include <libpayload.h>
#include "base/cleanup_funcs.h"
#include "base/init_funcs.h"
#include "base/late_init_funcs.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/soc/qcom_spmi.h"
#include "drivers/soc/qcom_spmi_defs.h"

/*
 * SDAM16 BOOT_REASON register (SDAM16_MEM_030) at SPMI address 0x7F5E
 * Bits 5:4 - ADSP boot reason (fresh boot / recovery)
 * Bits 3:1 - ADSP power state
 * Bit 0    - OSType flag (0 = RTOS, 1 = HLOS)
 */
/*
 * SDAM15_MEM_061 (SPMI address 0x7E7D) - SetMaxPwrReq_BattSts register
 * Bit 7 - MISSING_BATT_STS: set when battery is absent (AC-only boot)
 * Bit 6 - DEAD_BATT_STS
 * Bit 5 - BARREL_ACTIVE
 * Bit 4 - SKIP_SMB_RESET
 * Bits 1:0 - INPUT_PWR_STS
 */
#define SDAM15_MEM_061_ADDR	0x7E7D
#define MISSING_BATT_STS	BIT(7)

#define SDAM16_MEM_030_ADDR	0x7F5E
#define OS_TYPE_HLOS		BIT(0)
#define BOOT_REASON_MASK	(0x3 << 4)  /* Bits 5:4 */

#define LPASS_AON_CC_LPASS_CORE_HM_COLLAPSE_VOTE_FOR_Q6	((void *)0x06E14000)
#define LPASS_CORE_HM_GDSCR_ADDR			((void *)0x07E00000)
#define GCC_LPASS_CFG_NOC_SWAY_CBCR_ADDR		((void *)0x00147000)
#define LPASS_TOP_CC_LPASS_CORE_SWAY_AHB_LS_CBCR_ADDR	((void *)0x07E09000)

#define LPASS_CORE_HM_VOTE_POWER_OFF	0x1
#define GDSC_PWR_ON			(1 << 31)
#define CLK_CTL_EN_SHFT			0
#define HW_CTL				(1 << 1)

#define GDSC_TIMEOUT_US			150000

static int wait_for_condition(uint32_t timeout_us, int condition)
{
	uint64_t start = timer_us(0);

	while (!condition) {
		if (timer_us(start) > timeout_us)
			return 0;
		udelay(1);
	}
	return 1;
}

/*
 * Notify the ADSP of the boot context by writing SDAM16_MEM_030 via PMIC SPMI:
 *   - Bits 5:4 (BOOT_REASON): cleared to 0x00 (FRESHBOOT = fresh/cold boot)
 *   - Bit 0 (OS_TYPE): set to 1 (HLOS)
 * Called as a late init function so that all drivers are available and the
 * write happens before vboot selects a kernel.
 */
static int adsp_init_boot_context(struct LateInitFunc *init)
{
	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
					    PMIC_REG_CHAN0_ADDR,
					    PMIC_REG_LAST_CHAN_ADDR -
					    PMIC_REG_FIRST_CHAN_ADDR);

	int val = pmic_spmi->read8(pmic_spmi, SDAM16_MEM_030_ADDR);
	if (val < 0) {
		printf("ADSP: SPMI read failed: %d\n", val);
		free(pmic_spmi);
		return 0;
	}

	/* Masked write: clear bits 5:4 (BOOT_REASON=FRESHBOOT) and set bit 0 (OS_TYPE_HLOS) */
	int ret = pmic_spmi->write8(pmic_spmi, SDAM16_MEM_030_ADDR,
				    (uint8_t)((val & ~BOOT_REASON_MASK) | OS_TYPE_HLOS));
	if (ret)
		printf("ADSP: SPMI write failed: %d\n", ret);

	free(pmic_spmi);
	return 0;
}

/*
 * Notify the PMIC of a missing battery by setting MISSING_BATT_STS (bit 7)
 * in SDAM15_MEM_061 via SPMI. Required for stable AC-only (battery-less)
 * boot; without this, hard resets are observed during handoff.
 * Registered as a LATE_INIT_FUNC so all drivers are available when the
 * EC is queried and the SPMI write occurs before vboot selects a kernel.
 */
static int set_missing_batt_status(struct LateInitFunc *init)
{
	if (!CONFIG(DRIVER_EC_CROS) || cros_ec_is_battery_present())
		return 0;

	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
					    PMIC_REG_CHAN0_ADDR,
					    PMIC_REG_LAST_CHAN_ADDR -
					    PMIC_REG_FIRST_CHAN_ADDR);

	int val = pmic_spmi->read8(pmic_spmi, SDAM15_MEM_061_ADDR);
	if (val < 0) {
		printf("PMIC: SPMI read of SDAM15_MEM_061 failed: %d\n", val);
		free(pmic_spmi);
		return 0;
	}

	if (!(val & MISSING_BATT_STS)) {
		int ret = pmic_spmi->write8(pmic_spmi, SDAM15_MEM_061_ADDR,
					    (uint8_t)(val | MISSING_BATT_STS));
		if (ret)
			printf("PMIC: Failed to set MISSING_BATT_STS: %d\n", ret);
		else
			printf("PMIC: MISSING_BATT_STS set in SDAM15_MEM_061\n");
	}

	free(pmic_spmi);
	return 0;
}

static int lpass_cleanup(struct CleanupFunc *cleanup, CleanupType type)
{
	printf("LPASS: Performing cleanup before handoff\n");

	write32(LPASS_AON_CC_LPASS_CORE_HM_COLLAPSE_VOTE_FOR_Q6, LPASS_CORE_HM_VOTE_POWER_OFF);
	if (!wait_for_condition(GDSC_TIMEOUT_US,
				(read32(LPASS_CORE_HM_GDSCR_ADDR) & GDSC_PWR_ON))) {
		printf("LPASS: Core HM GDSC PWR_OFF timeout after vote\n");
		return -1;
	}

	clrbits32(GCC_LPASS_CFG_NOC_SWAY_CBCR_ADDR, (1 << CLK_CTL_EN_SHFT));

	clrbits32(LPASS_TOP_CC_LPASS_CORE_SWAY_AHB_LS_CBCR_ADDR, HW_CTL);

	printf("LPASS: Cleanup completed successfully\n");
	return 0;
}

static int lpass_cleanup_install(void)
{
	static CleanupFunc dev = {
		&lpass_cleanup,
		CleanupOnHandoff | CleanupOnLegacy,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(lpass_cleanup_install);
LATE_INIT_FUNC(set_missing_batt_status);
LATE_INIT_FUNC(adsp_init_boot_context);
