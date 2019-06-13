/*
 * Copyright (C) 2018 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * This is a driver for the TI LP5562 (http://www.ti.com/product/lp5562),
 * driving a tri-color LED.
 *
 * The only connection between the LED and the main board is an i2c bus.
 *
 * This driver imitates a depthcharge display device. On initialization the
 * driver sets up the controllers to prepare them to accept programs to run.
 *
 * When a certain vboot state needs to be indicated, the program for that
 * state is loaded into the controllers, resulting in the state appropriate
 * LED behavior.
 */

#ifndef __THIRD_PARTY_COREBOOT_SRC_DRIVERS_I2C_LED_LP5562_LED_LP5562_PROGRAMS_H__
#define __THIRD_PARTY_COREBOOT_SRC_DRIVERS_I2C_LED_LP5562_LED_LP5562_PROGRAMS_H__

#include <stdint.h>
#include "drivers/video/led_lp5562.h"

/* There are threee independent engines/cores in the controller. */
#define LED_LP5562_NUM_OF_ENGINES 3

/* Number of LP5562 controllers on this implementation */
#define LED_LP5562_NUM_LED_CONTROLLERS 1

/* Default LED current (x0.1 mA) */
#define LED_LP5562_DEFAULT_CURRENT 120

/*
 * Structure to describe an lp5562 program: pointer to the text of the
 * program, its size for each engines, and start address relative to
 * each engine program memory. Program memory address for each engine is
 * fixed on LP5562.
 */
typedef struct {
	struct {
		uint8_t *program_text;
		uint8_t program_size;
		uint8_t engine_start_addr;
		uint8_t led_current;
	} engine_program[LED_LP5562_NUM_OF_ENGINES];
} TiLp5562Program;

/* A structure to bind controller programs to a vboot state. */
typedef struct {
	enum VbScreenType_t    vb_screen;
	TiLp5562Program *programs[LED_LP5562_NUM_LED_CONTROLLERS];
} Led5562StateProg;

extern const Led5562StateProg led_lp5562_state_programs[];

#endif
