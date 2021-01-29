/*
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <libpayload.h>

#include "ipq40xx.h"
/*******************************************************
Function description: check for invalid GPIO #
Arguments :
gpio_t gpio - Gpio number

Return : GPIO Valid(0)/Invalid(1)
*******************************************************/

static inline int gpio_not_valid(gpio_t gpio)
{
	return (gpio > GPIO_MAX_NUM);
}


/*******************************************************
Function description: configure GPIO functinality
Arguments :
gpio_t gpio - Gpio number
unsigned func - Functionality number
unsigned pull - pull up/down, no pull range(0-3)
unsigned drvstr - range (0 - 7)-> (2- 16)MA steps of 2
unsigned enable - 0 Disable, 1 - Enable.

Return : None
*******************************************************/


void gpio_tlmm_config_set(gpio_t gpio, unsigned func,
			  unsigned pull, unsigned drvstr,
			  unsigned enable)
{
	unsigned val = 0;

	if (gpio_not_valid(gpio))
		return;

	val |= (pull & GPIO_CFG_PULL_MASK) << GPIO_CFG_PULL_SHIFT;
	val |= (func & GPIO_CFG_FUNC_MASK) << GPIO_CFG_FUNC_SHIFT;
	val |= (drvstr & GPIO_CFG_DRV_MASK) << GPIO_CFG_DRV_SHIFT;
	val |= (enable & GPIO_CFG_OE_MASK) << GPIO_CFG_OE_SHIFT;

	write32(GPIO_CONFIG_ADDR(gpio), val);
}
/*******************************************************
Function description: configure GPIO functinality
Arguments :
unsigned int gpio - Gpio number
unsigned int func - Functionality number
unsigned int dir  - direction 0- i/p, 1- o/p
unsigned int pull - pull up/down, no pull range(0-3)
unsigned int drvstr - range (0 - 7)-> (2- 16)MA steps of 2
unsigned int oe - 0 - Disable, 1- Enable.

Return : None
*******************************************************/
void gpio_tlmm_config(unsigned int gpio, unsigned int func,
		      unsigned int out, unsigned int pull,
		      unsigned int drvstr, unsigned int oe,
		      unsigned int gpio_vm, unsigned int gpio_od_en,
		      unsigned int gpio_pu_res)
{
	unsigned int val = 0;
	unsigned int *addr = (unsigned int *)GPIO_CONFIG_ADDR(gpio);

	val |= pull;
	val |= func << 2;
	val |= drvstr << 6;
	val |= oe << 9;
	val |= gpio_vm << 11;
	val |= gpio_od_en << 12;
	val |= gpio_pu_res << 13;

	write32(addr, val);

	/* Output value is only relevant if GPIO has been configured for fixed
	 * output setting - i.e. func == 0 */
	if (func == 0) {
		addr = (unsigned int *)GPIO_IN_OUT_ADDR(gpio);
		val = read32(addr);
		val |= out << 1;
		write32(addr, val);
	}
}

/*******************************************************
Function description: Get GPIO configuration
Arguments :
gpio_t gpio - Gpio number
unsigned *func - Functionality number
unsigned *pull - pull up/down, no pull range(0-3)
unsigned *drvstr - range (0 - 7)-> (2- 16)MA steps of 2
unsigned *enable - 0 - Disable, 1- Enable.

Return : None
*******************************************************/


void gpio_tlmm_config_get(gpio_t gpio, unsigned *func,
			  unsigned *pull, unsigned *drvstr,
			  unsigned *enable)
{
	unsigned val;
	void *addr = GPIO_CONFIG_ADDR(gpio);

	if (gpio_not_valid(gpio))
		return;

	val = read32(addr);

	*pull = (val >> GPIO_CFG_PULL_SHIFT) & GPIO_CFG_PULL_MASK;
	*func = (val >> GPIO_CFG_FUNC_SHIFT) & GPIO_CFG_FUNC_MASK;
	*drvstr = (val >> GPIO_CFG_DRV_SHIFT) & GPIO_CFG_DRV_MASK;
	*enable = (val >> GPIO_CFG_OE_SHIFT) & GPIO_CFG_OE_MASK;
}

/*******************************************************
Function description: get GPIO IO functinality details
Arguments :
gpio_t gpio - Gpio number
unsigned *in - Value of GPIO input
unsigned *out - Value of GPIO output

Return : None
*******************************************************/
int gpio_get_in_value(gpio_t gpio)
{
	if (gpio_not_valid(gpio))
		return -1;


	return (read32(GPIO_IN_OUT_ADDR(gpio)) >> GPIO_IO_IN_SHIFT) &
		GPIO_IO_IN_MASK;
}

void gpio_set_out_value(gpio_t gpio, unsigned value)
{
	if (gpio_not_valid(gpio))
		return;

	write32(GPIO_IN_OUT_ADDR(gpio), (value & 1) << GPIO_IO_OUT_SHIFT);
}

void gpio_input_pulldown(gpio_t gpio)
{
	gpio_tlmm_config_set(gpio, GPIO_FUNC_DISABLE,
			     GPIO_PULL_DOWN, GPIO_2MA, GPIO_DISABLE);
}

void gpio_input_pullup(gpio_t gpio)
{
	gpio_tlmm_config_set(gpio, GPIO_FUNC_DISABLE,
			     GPIO_PULL_UP, GPIO_2MA, GPIO_DISABLE);
}

void gpio_input(gpio_t gpio)
{
	gpio_tlmm_config_set(gpio, GPIO_FUNC_DISABLE,
			     GPIO_NO_PULL, GPIO_2MA, GPIO_DISABLE);
}
