// SPDX-License-Identifier: GPL-2.0-only
// Copyright 2024 Google LLC.

#include "drivers/gpio/mtk_gpio.h"

void gpio_calc_eint_pos_bit(u32 pin, u32 *pos, u32 *bit)
{
	*pos = pin / MAX_EINT_REG_BITS;
	*bit = pin % MAX_EINT_REG_BITS;
}

EintRegs *gpio_get_eint_reg(u32 pin)
{
	return (EintRegs *)(EINT_BASE);
}
