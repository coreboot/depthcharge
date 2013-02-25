/*
 * Copyright 2013 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_GPIO_S5P_H__
#define __DRIVERS_GPIO_S5P_H__

enum {
	GPIO_A,
	GPIO_B,
	GPIO_C,
	GPIO_D,
	GPIO_E,
	GPIO_F,
	GPIO_G,
	GPIO_H,
	GPIO_V,
	GPIO_X,
	GPIO_Y,
	GPIO_Z
};

static inline unsigned s5p_gpio_index(unsigned group, unsigned bank,
				      unsigned bit) __attribute__((unused));
static inline unsigned s5p_gpio_index(unsigned group, unsigned bank,
				      unsigned bit)
{
	return (group << 5) | ((bank & 0x3) << 3) | (bit & 0x7);
}

static inline unsigned s5p_gpio_group(unsigned index) __attribute__((unused));
static inline unsigned s5p_gpio_group(unsigned index)
{
	return index >> 5;
}

static inline unsigned s5p_gpio_bank(unsigned index) __attribute__((unused));
static inline unsigned s5p_gpio_bank(unsigned index)
{
	return (index >> 3) & 0x3;
}

static inline unsigned s5p_gpio_bit(unsigned index) __attribute__((unused));
static inline unsigned s5p_gpio_bit(unsigned index)
{
	return index & 0x7;
}

int s5p_gpio_set_pud(unsigned index, unsigned value);

#endif /* __DRIVERS_GPIO_S5P_H__ */
