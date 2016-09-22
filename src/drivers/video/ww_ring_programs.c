/*
 * Copyright (C) 2015 Google, Inc.
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
 * This is a driver for the Whirlwind LED ring, which is equipped with two LED
 * microcontrollers TI LP55231 (http://www.ti.com/product/lp55231), each of
 * them driving three multicolor LEDs.
 *
 * The only connection between the ring and the main board is an i2c bus.
 *
 * This driver imitates a depthcharge display device. On initialization the
 * driver sets up the controllers to prepare them to accept programs to run.
 *
 * When a certain vboot state needs to be indicated, the program for that
 * state is loaded into the controllers, resulting in the state appropriate
 * LED behavior.
 */

#include "drivers/video/ww_ring_programs.h"

/****************************************************************
 *   LED ring program definitions for different patterns.
 *
 * Comments below are real lp55231 source code, they are compiled using
 * lasm.exe, the TI tool available from their Web site (search for lp55231)
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

row_red:   dw 0000000000000001b
row_green: dw 0000000000000010b
row_blue:  dw 0000000000000100b

.segment program1
	mux_map_addr row_red
	set_pwm 0
	end

.segment program2
	mux_map_addr row_green
	set_pwm 0
	end

.segment program3
	mux_map_addr row_blue
	set_pwm 0
	end
*/

/* RGB set to 000000, resulting in all LEDs off. */
static const uint8_t solid_000000_text[] = {
	0x00, 0x01, 0x00, 0x02, 0x00, 0x04, 0x9F, 0x80,
	0x40,    0, 0xC0, 0x00, 0x9F, 0x81, 0x40,    0,
	0xC0, 0x00, 0x9F, 0x82, 0x40,    0, 0xC0, 0x00
};

static const TiLp55231Program solid_000000_program = {
	solid_000000_text,
	sizeof(solid_000000_text),
	0,
	{ 3, 6, 9 }
};

/*
 * Blinking patterns are tricker then solid ones.
 *
 * The three internal engines seem to be competing for resources and get out
 * of sync in seconds if left running asynchronously.
 *
 * When solid patterns are deployed with instanteneous color intensity
 * changes, all three LEDs can be controlled by one engine in sequential
 * accesses. But the controllers still neeed to be synchronized.
 *
 * The maximum timer duration of lp55231 is .48 seconds. To achieve longer
 * blinking intervals the loops delays are deployed. Only the first controller
 * intervals need to be changed, as the second one is in lockstep with the
 * first.
 *
 * The time granularity is set at .1 second (see commands 'wait 0.1' in the
 * code), and then the loop counters can be set up to 63 (registers rb and rc),
 * which allows to generate intervals up to 6.3 seconds in .1 second
 * increments.
 */
/*
 * blink_solid1.src

row_red:   dw 0000000000000001b
row_green: dw 0000000000000010b
row_blue:  dw 0000000000000100b

.segment program1
	ld ra, 2   # LED on duration
	ld rb, 10  # LED off duration
	mux_map_addr row_red
loop:
	ld rc, 98	; red intensity
	set_pwm rc
	mux_map_addr row_green
	ld rc, 0	; green intensity
	set_pwm rc
	mux_map_addr row_blue
	ld rc, 234	; blue intensity
	set_pwm rc
wait1:
	wait 0.1
	branch ra, wait1
	set_pwm 0
	mux_map_addr row_green
	set_pwm 0
	mux_map_addr row_red
	set_pwm 0
wait2:
	wait 0.1
	branch rb, wait2
	branch 0, loop

.segment program2
	 end

.segment program3
	 end
*/
static const uint8_t blink_dev1_text[] = {
	0x00,  0x01,  0x00,  0x02,  0x00,  0x04,  0x90,  0x02,
	0x94,  0x0a,  0x9f,  0x80,  0x98,    98,  0x84,  0x62,
	0x9f,  0x81,  0x98,     0,  0x84,  0x62,  0x9f,  0x82,
	0x98,   234,  0x84,  0x62,  0x4c,  0x00,  0x86,  0x2C,
	0x40,  0x00,  0x9f,  0x81,  0x40,  0x00,  0x9f,  0x80,
	0x40,  0x00,  0x4c,  0x00,  0x86,  0x49,  0xa0,  0x03,
	0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_dev1_program = {
	blink_dev1_text,
	sizeof(blink_dev1_text),
	0,
	{ 3,  24,  25,  }
};

static const uint8_t blink_rcv_remove1_text[] = {
	0x00,  0x01,  0x00,  0x02,  0x00,  0x04,  0x90,  0x02,
	0x94,  0x0a,  0x9f,  0x80,  0x98,   255,  0x84,  0x62,
	0x9f,  0x81,  0x98,    50,  0x84,  0x62,  0x9f,  0x82,
	0x98,    50,  0x84,  0x62,  0x4c,  0x00,  0x86,  0x2C,
	0x40,  0x00,  0x9f,  0x81,  0x40,  0x00,  0x9f,  0x80,
	0x40,  0x00,  0x4c,  0x00,  0x86,  0x49,  0xa0,  0x03,
	0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_rcv_remove1_program = {
	blink_rcv_remove1_text,
	sizeof(blink_rcv_remove1_text),
	0,
	{ 3,  24,  25,  }
};

static const uint8_t blink_rcv_insert1_text[] = {
	0x00,  0x01,  0x00,  0x02,  0x00,  0x04,  0x90,  0x02,
	0x94,  0x0a,  0x9f,  0x80,  0x98,   255,  0x84,  0x62,
	0x9f,  0x81,  0x98,   100,  0x84,  0x62,  0x9f,  0x82,
	0x98,    10,  0x84,  0x62,  0x4c,  0x00,  0x86,  0x2C,
	0x40,  0x00,  0x9f,  0x81,  0x40,  0x00,  0x9f,  0x80,
	0x40,  0x00,  0x4c,  0x00,  0x86,  0x49,  0xa0,  0x03,
	0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_rcv_insert1_program = {
	blink_rcv_insert1_text,
	sizeof(blink_rcv_insert1_text),
	0,
	{ 3,  24,  25,  }
};

static const uint8_t blink_rcv2dev1_text[] = {
	0x00,  0x01,  0x00,  0x02,  0x00,  0x04,  0x90,  0x02,
	0x94,  0x02,  0x9f,  0x80,  0x98,    98,  0x84,  0x62,
	0x9f,  0x81,  0x98,     0,  0x84,  0x62,  0x9f,  0x82,
	0x98,   234,  0x84,  0x62,  0x4c,  0x00,  0x86,  0x2C,
	0x40,  0x00,  0x9f,  0x81,  0x40,  0x00,  0x9f,  0x80,
	0x40,  0x00,  0x4c,  0x00,  0x86,  0x49,  0xa0,  0x03,
	0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_rcv2dev1_program = {
	blink_rcv2dev1_text,
	sizeof(blink_rcv2dev1_text),
	0,
	{ 3,  24,  25,  }
};

/* Ping pong of colors (duty cycle 50%, odd and even LEDs' colors reversed. */
/*
 * ping_pong1.src
row_red:   dw 0000000000000001b
row_green: dw 0000000000000010b
row_blue:  dw 0000000000000100b

.segment program1
	ld ra, 5  # LED ping duration
	ld rb, 5  # LED pong duration
loop:
	mux_map_addr row_red
	set_pwm 255
	mux_map_addr row_green
	set_pwm 100
	mux_map_addr row_blue
	set_pwm 10
	mux_map_addr row_red
	set_pwm 160
	mux_map_addr row_green
	set_pwm 90
	mux_map_addr row_blue
	set_pwm 0
wait1:
	wait 0.1
	branch ra, wait1
	mux_map_addr row_red
	set_pwm 160
	mux_map_addr row_green
	set_pwm 90
	mux_map_addr row_blue
	set_pwm 0
	mux_map_addr row_red
	set_pwm 255
	mux_map_addr row_green
	set_pwm 100
	mux_map_addr row_blue
	set_pwm 10
wait2:
	wait 0.1
	branch rb, wait2
	branch 0, loop

.segment program2
	 end

.segment program3
	 end
*/

static const uint8_t ping_pong1_text[] = {
	0x00,  0x01,  0x00,  0x02,  0x00,  0x04,  0x90,  0x05,
	0x94,  0x05,  0x9f,  0x80,  0x40,   255,  0x9f,  0x81,
	0x40,   100,  0x9f,  0x82,  0x40,    10,  0x9f,  0x80,
	0x40,   160,  0x9f,  0x81,  0x40,    90,  0x9f,  0x82,
	0x40,     0,  0x4c,  0x00,  0x86,  0x38,  0x9f,  0x80,
	0x40,   160,  0x9f,  0x81,  0x40,    90,  0x9f,  0x82,
	0x40,     0,  0x9f,  0x80,  0x40,   255,  0x9f,  0x81,
	0x40,   100,  0x9f,  0x82,  0x40,    10,  0x4c,  0x00,
	0x86,  0x71,  0xa0,  0x02,  0xc0,  0x00,  0xc0,  0x00,
	0x00,
};

static const TiLp55231Program ping_pong1_program = {
	ping_pong1_text,
	sizeof(ping_pong1_text),
	0,
	{ 3,  34,  35,  }
};

/* Running multicolor lights, used as "unknown pattern" indicator. */
/* run_lights1.src
rowall: dw 0000000000000111b
row1:   dw 0000000000000001b
	dw 0000000000000010b
rowX:   dw 0000000000000100b

.segment program1	       ; Program for engine 1.
	mux_map_addr rowall
	set_pwm 0
loop:
	mux_map_start row1
	mux_ld_end rowX
loop1:  ramp 0.1, 255
	ramp 0.1, -255
	mux_map_next
	branch 2, loop1
	branch 0, loop

.segment program2
	end

.segment program3
	end
*/

static const uint8_t run_lights1_text[] = {
	0x00,  0x07,  0x00,  0x01,  0x00,  0x02,  0x00,  0x04,
	0x9f,  0x80,  0x40,  0x00,  0x9c,  0x01,  0x9c,  0x83,
	0x02,  0xff,  0x03,  0xff,  0x9d,  0x80,  0xa1,  0x04,
	0xa0,  0x02,  0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program run_lights1_program = {
	run_lights1_text,
	sizeof(run_lights1_text),
	0,
	{ 4,  13,  14,  }
};

const WwRingStateProg wwr_state_programs[] = {
	/*
	 * for test purposes the blank screen program is set to blinking, will
	 * be changed soon.
	 */
	{VB_SCREEN_BLANK, {&solid_000000_program} },
	{VB_SCREEN_DEVELOPER_WARNING, {&blink_dev1_program} },
	{VB_SCREEN_RECOVERY_REMOVE, {&blink_rcv_remove1_program} },
	{VB_SCREEN_RECOVERY_INSERT, {&blink_rcv_insert1_program} },
	{VB_SCREEN_RECOVERY_TO_DEV, {&blink_rcv2dev1_program} },
	{VB_SCREEN_RECOVERY_NO_GOOD, {&ping_pong1_program} },
	{VB_SCREEN_DEVELOPER_EGG, {&run_lights1_program} },
	{}, /* Empty record to mark the end of the table. */
};
