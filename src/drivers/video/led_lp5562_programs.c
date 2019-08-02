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

#include "drivers/video/led_lp5562_programs.h"
#include "drivers/video/led_lp5562_calibration.h"

/****************************************************************
 *   LED ring program definitions for different patterns.
 *
 * Comments below are real lp5562 source code, they are compiled using
 * lasm.exe, the TI tool available from their Web site (search for lp5562)
 * and running only on Windows :P.
 *
 * Different hex dumps are results of tweaking the source code parameters to
 * achieve desirable LED ring behavior. It is possible to use just one code
 * dump and replace certain values in the body to achieve different behaviour
 * with the same basic dump, but keeping track of location to tweak with every
 * code change would be quite tedious.
 */

/*
 * Solid LED display, the arguments of the set_pwm commands set intensity and
 * color of the display:

  1 00		.ENGINE1(B)
  2 00 4080		set_pwm 128
  3 01 C000		end
  4
  5 10		.ENGINE2(G)
  6 10 4080		set_pwm 128
  7 11 C000		end
  8
  9 20		.ENGINE3(R)
 10 20 4080		set_pwm 128
 11 21 C000		end
*/

/* RGB set to 000000, resulting in all LEDs off. */
static uint8_t solid_00_text[] = {
	0x40, 0x00, 0xc0, 0x00
};

static TiLp5562Program solid_000000_program = {
	{
		{ /* Engine1:Blue */
			solid_00_text,
			sizeof(solid_00_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{ /* Engine2:Green */
			solid_00_text,
			sizeof(solid_00_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{ /* Engine3:Red */
			solid_00_text,
			sizeof(solid_00_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
	}
};

/*
  1 00          .ENGINE1(B)
  2 00 E200             trigger w3
  3 01 40EA             set_pwm 234
  4 02 E200             trigger w3
  5 03 4000             set_pwm 0
  6 04 0000             gotostart
  7
  8 10          .ENGINE2(G)
  9 10 E200             trigger w3
 10 11 4000             set_pwm 0
 11 12 E200             trigger w3
 12 13 4000             set_pwm 0
 13 14 0000             gotostart
 14
 15 20          .ENGINE3(R)
 16 20 E006             trigger s21
 17 21 4062             set_pwm 98
 18 22 5300             wait 300
 19 23 E006             trigger s21
 20 24 4000             set_pwm 0
 21 25 6300             wait 550
 22 26 6300             wait 550
 23 27 0000             gotostart
*/

/* VB_SCREEN_DEVELOPER_WARNING, Developer mode indication */
/* Flashing Purple, (98,0,248) for 0.3sec, (0,0,0) for 1.1sec */
static uint8_t blink_dev1_b_text[] = {
	0xE2,  0x00,  0x40,   234,  0xE2,  0x00,  0x40,     0,
	0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00
};

static uint8_t blink_dev1_g_text[] = {
	0xE2,  0x00,  0x40,     0,  0xE2,  0x00,  0x40,     0,
	0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00
};

static uint8_t blink_dev1_r_text[] = {
	0xE0,  0x06,  0x40,    98,  0x53,  0x00,  0xE0,  0x06,
	0x40,     0,  0x63,  0x00,  0x63,  0x00,  0x00,  0x00
};

static TiLp5562Program blink_dev1_program = {
	{
		{
			blink_dev1_b_text,
			sizeof(blink_dev1_b_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_dev1_g_text,
			sizeof(blink_dev1_g_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_dev1_r_text,
			sizeof(blink_dev1_r_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
	}
};

/* VB_SCREEN_OS_BROKEN, Manual Recovery Required */
/* Flashing Pink, (50,50,255) for 0.3sec, (0,0,0) for 1.1sec */
static uint8_t blink_rcv_broken1_b_text[] = {
	0xE2,  0x00,  0x40,    50,  0xE2,  0x00,  0x40,     0,
	0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00
};

static uint8_t blink_rcv_broken1_g_text[] = {
	0xE2,  0x00,  0x40,    50,  0xE2,  0x00,  0x40,     0,
	0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00
};

static uint8_t blink_rcv_broken1_r_text[] = {
	0xE0,  0x06,  0x40,   255,  0x53,  0x00,  0xE0,  0x06,
	0x40,     0,  0x63,  0x00,  0x63,  0x00,  0x00,  0x00
};

static TiLp5562Program blink_rcv_broken1_program = {
	{
		{
			blink_rcv_broken1_b_text,
			sizeof(blink_rcv_broken1_b_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_rcv_broken1_g_text,
			sizeof(blink_rcv_broken1_g_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_rcv_broken1_r_text,
			sizeof(blink_rcv_broken1_r_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
	}
};

/* VB_SCREEN_RECOVERY_INSERT, Waiting for USB stick */
/* Flashing Red, (0,0,255) for 0.3sec, (0,0,0) for 1.1sec */
static uint8_t blink_rcv_insert1_b_text[] = {
	0xE2,  0x00,  0x40,     0,  0xE2,  0x00,  0x40,     0,
	0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00
};

static uint8_t blink_rcv_insert1_g_text[] = {
	0xE2,  0x00,  0x40,     0,  0xE2,  0x00,  0x40,     0,
	0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00
};

static uint8_t blink_rcv_insert1_r_text[] = {
	0xE0,  0x06,  0x40,   255,  0x53,  0x00,  0xE0,  0x06,
	0x40,     0,  0x63,  0x00,  0x63,  0x00,  0x00,  0x00
};

static TiLp5562Program blink_rcv_insert1_program = {
	{
		{
			blink_rcv_insert1_b_text,
			sizeof(blink_rcv_insert1_b_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_rcv_insert1_g_text,
			sizeof(blink_rcv_insert1_g_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_rcv_insert1_r_text,
			sizeof(blink_rcv_insert1_r_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
	}
};

/* VB_SCREEN_RECOVERY_TO_DEV, Switching from REC to DEV mode */
/* Flashing Purple, (98,0,248) for 0.3sec, (0,0,0) for 1.1sec */
static uint8_t blink_rcv2dev1_b_text[] = {
	0xE2,  0x00,  0x40,   234,  0xE2,  0x00,  0x40,     0,
	0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00
};

static uint8_t blink_rcv2dev1_g_text[] = {
	0xE2,  0x00,  0x40,     0,  0xE2,  0x00,  0x40,     0,
	0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00
};

static uint8_t blink_rcv2dev1_r_text[] = {
	0xE0,  0x06,  0x40,    98,  0x53,  0x00,  0xE0,  0x06,
	0x40,     0,  0x63,  0x00,  0x63,  0x00,  0x00,  0x00
};

static TiLp5562Program blink_rcv2dev1_program = {
	{
		{
			blink_rcv2dev1_b_text,
			sizeof(blink_rcv2dev1_b_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_rcv2dev1_g_text,
			sizeof(blink_rcv2dev1_g_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_rcv2dev1_r_text,
			sizeof(blink_rcv2dev1_r_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
	}
};

/* VB_SCREEN_RECOVERY_NO_GOOD, Bad USB stick */
/* Flashing Red/Yellow, (255,0,0) for 0.6sec, (255,204,0) for 0.6sec */
static uint8_t blink_no_good1_b_text[] = {
	0xE2,  0x00,  0x40,     0,  0xE2,  0x00,  0x40,     0,
	0x00,  0x00,
};

static uint8_t blink_no_good1_g_text[] = {
	0xE2,  0x00,  0x40,     0,  0xE2,  0x00,  0x40,   204,
	0x00,  0x00,
};

static uint8_t blink_no_good1_r_text[] = {
	0xE0,  0x06,  0x40,   255,  0x66,  0x00,  0xE0,  0x06,
	0x40,   255,  0x66,  0x00,  0x00,  0x00,
};

static TiLp5562Program blink_no_good1_program = {
	{
		{
			blink_no_good1_b_text,
			sizeof(blink_no_good1_b_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_no_good1_g_text,
			sizeof(blink_no_good1_g_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
		{
			blink_no_good1_r_text,
			sizeof(blink_no_good1_r_text),
			0,
			LED_LP5562_DEFAULT_CURRENT
		},
	}
};

const Led5562StateProg led_lp5562_state_programs[] = {
	/*
	 * for test purposes the blank screen program is set to blinking, will
	 * be changed soon.
	 */
	{VB_SCREEN_BLANK, {&solid_000000_program} },
	{VB_SCREEN_DEVELOPER_WARNING, {&blink_dev1_program} },
	{VB_SCREEN_OS_BROKEN, {&blink_rcv_broken1_program} },
	{VB_SCREEN_RECOVERY_INSERT, {&blink_rcv_insert1_program} },
	{VB_SCREEN_RECOVERY_TO_DEV, {&blink_rcv2dev1_program} },
	{VB_SCREEN_RECOVERY_NO_GOOD, {&blink_no_good1_program} },
	{}, /* Empty record to mark the end of the table. */
};

/*
 * Calibration code map for "blink" pattern.
 * Set PWM values, wait, set PWMs to 0, wait, loop.
 */
const struct lp5562_calibration_code_map mistral_code_map_blink[] = {
	{
		blue,
		set_pwm,
		0x01,
		0,
		{ }
	},
	{
		green,
		set_pwm,
		0x11,
		0,
		{ }
	},
	{
		red,
		set_pwm,
		0x21,
		0,
		{ }
	},
	{
		0,
		invalid,
		0x00,
		0,
		{ }
	}
};

const struct lp5562_calibration_data mistral_calibration_database[] = {
	{
		&blink_rcv_insert1_program,
		2, /* Red */
		100, /* 100% brightness */
		mistral_code_map_blink,
	},
	{
		NULL,
		0,
		0,
		NULL
	}
};
