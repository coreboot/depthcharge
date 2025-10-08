/*
 * Copyright 2025 Google LLC
 *
 * Commands for testing SPMI read/write.
 */

#include "common.h"
#include "drivers/soc/qcom_spmi.h"
#include "drivers/soc/qcom_spmi_defs.h"

static QcomSpmi *pmic_spmi;
static int initialized;

static int do_spmir(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int size;
	u16 slave;
	u16 reg;

	if (argc < 3)
		return CMD_RET_USAGE;

	/* Check for size specification */
	size = cmd_get_data_size(argv[0], 1);
	if (size < 1)
		return CMD_RET_USAGE;

	/* Get slave ID and register offset */
	slave = strtoul(argv[1], NULL, 16);
	reg = strtoul(argv[2], NULL, 16);

	if (!initialized) {
		pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
				    PMIC_REG_CHAN0_ADDR,
				    PMIC_REG_LAST_CHAN_ADDR - PMIC_REG_FIRST_CHAN_ADDR);
		initialized = 1;
	}

	switch (size) {
	case 1:
		console_printf("0x%02x\n", pmic_spmi->read8(pmic_spmi, slave << 16 | reg));
		break;
	default:
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

static int do_spmiw(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int size;
	u16 slave;
	u16 reg;
	u8 value;

	if (argc < 4)
		return CMD_RET_USAGE;

	/* Check for size specification */
	size = cmd_get_data_size(argv[0], 1);
	if (size < 1)
		return CMD_RET_USAGE;

	/* Get slave ID and register offset */
	slave = strtoul(argv[1], NULL, 16);
	reg = strtoul(argv[2], NULL, 16);

	/* Get write value */
	value = strtoul(argv[3], NULL, 16);

	if (!initialized) {
		pmic_spmi = new_qcom_spmi(PMIC_CORE_REGISTERS_ADDR,
				    PMIC_REG_CHAN0_ADDR,
				    PMIC_REG_LAST_CHAN_ADDR - PMIC_REG_FIRST_CHAN_ADDR);
		initialized = 1;
	}

	switch (size) {
	case 1:
		pmic_spmi->write8(pmic_spmi, slave << 16 | reg, value);
		break;
	default:
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	spmir,	3,	1,
	"SPMI read",
	"[.b] slave register"
);

U_BOOT_CMD(
	spmiw,	4,	1,
	"SPMI write",
	"[.b] slave register value"
);
