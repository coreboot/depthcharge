/* SPDX-License-Identifier: BSD-3-Clause */

#include <libpayload.h>
#include "base/late_init_funcs.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/soc/qcom_spmi.h"
#include "drivers/soc/qcom_spmi_defs.h"

/*
 * SDAM15_MEM_061 (SPMI address 0x7E7D):
 * Bit 7 - MISSING_BATT_STS: set when battery is absent (AC-only boot)
 */
#define SDAM15_MEM_061_ADDR	0x7E7D
#define MISSING_BATT_STS	BIT(7)

/*
 * SDAM16 BOOT_REASON register (SDAM16_MEM_030) at SPMI address 0x7F5E
 * Bits 5:4 - ADSP boot reason (fresh boot / recovery)
 * Bits 3:1 - ADSP power state
 * Bit 0    - OSType flag (0 = Bootloader, 1 = HLOS)
 */
#define SDAM16_MEM_030_ADDR	0x7F5E
#define OS_TYPE_HLOS		BIT(0)
#define BOOT_REASON_MASK	(0x3 << 4)  /* Bits 5:4 */

/*
 * Notify the SOCCP of the boot context by writing SDAM16_MEM_030 via PMIC SPMI:
 *   - Bits 5:4 (BOOT_REASON): cleared to 0x00 (FRESHBOOT = fresh/cold boot)
 *   - Bit 0 (OS_TYPE): set to 1 (HLOS)
 * Called as a late init function so that all drivers are available and the
 * write happens before vboot selects a kernel.
 */
static int soccp_init_boot_context(struct LateInitFunc *init)
{
	QcomSpmi *pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
					    PMIC_REG_CHAN0_ADDR,
					    PMIC_REG_LAST_CHAN_ADDR -
					    PMIC_REG_FIRST_CHAN_ADDR);

	int val = pmic_spmi->read8(pmic_spmi, SDAM16_MEM_030_ADDR);
	if (val < 0) {
		printf("PMIC: SPMI read failed: %d\n", val);
		free(pmic_spmi);
		return 0;
	}

	/* Masked write: clear bits 5:4 (BOOT_REASON=FRESHBOOT) and set bit 0 (OS_TYPE_HLOS) */
	int ret = pmic_spmi->write8(pmic_spmi, SDAM16_MEM_030_ADDR,
				    (uint8_t)((val & ~BOOT_REASON_MASK) | OS_TYPE_HLOS));
	if (ret)
		printf("PMIC: SPMI write failed: %d\n", ret);

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

LATE_INIT_FUNC(set_missing_batt_status);
LATE_INIT_FUNC(soccp_init_boot_context);
