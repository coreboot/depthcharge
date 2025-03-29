/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#include <libpayload.h>

#include "drivers/gpio/mtk_eint_v2.h"
#include "drivers/gpio/mtk_gpio.h"

void gpio_calc_eint_pos_bit(u32 pin, u32 *pos, u32 *bit)
{
	*pos = 0;
	*bit = 0;

	if (pin >= eint_data_len)
		return;

	uint8_t index = eint_data[pin].index;

	*pos = index / MAX_EINT_REG_BITS;
	*bit = index % MAX_EINT_REG_BITS;
}

EintRegs *gpio_get_eint_reg(u32 pin)
{
	uintptr_t addr;

	if (pin >= eint_data_len)
		return NULL;

	switch (eint_data[pin].instance) {
	case EINT_E:
		addr = EINT_E_BASE;
		break;
	case EINT_S:
		addr = EINT_S_BASE;
		break;
	case EINT_W:
		addr = EINT_W_BASE;
		break;
	case EINT_N:
		addr = EINT_N_BASE;
		break;
	case EINT_C:
		addr = EINT_C_BASE;
		break;
	default:
		printf("%s: Failed to look up a valid EINT base for %d\n",
		       __func__, pin);
		return NULL;
	}

	return (EintRegs *)addr;
}
