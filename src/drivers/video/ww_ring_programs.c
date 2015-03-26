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

row_red:   dw 0000000001001001b
row_green: dw 0000000010010010b
row_blue:  dw 0000000100100100b

.segment program1
	mux_map_addr row_red
	set_pwm 1
	end

.segment program2
	mux_map_addr row_green
	set_pwm 1
	end

.segment program3
	mux_map_addr row_blue
	set_pwm 1
	end
*/

/* RGB set to 000000, resulting in all LEDs off. */
static const uint8_t solid_000000_text[] = {
	0x00, 0x49, 0x00, 0x92, 0x01, 0x24, 0x9F, 0x80,
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
 * When there are two separate controllers, with three engine each, they all
 * run away from each other in no time, resulting in some completely chaotic
 * LED behavior.
 *
 * When solid patterns are deployed with instanteneous color intensity
 * changes, all three LEDs can be controlled by one engine in sequential
 * accesses. But the controllers still neeed to be synchronized.
 *
 * The first controller is loaded and started first, but it sits waiting for
 * the trigger from the second controller to actually start the cycle. Once
 * both controllers start running, the first controller is the master, sending
 * sych triggers to the second one each time the LED is supposed to be turned
 * on or off.
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
row_red:   dw 0000000001001001b
row_green: dw 0000000010010010b
row_blue:  dw 0000000100100100b

.segment program1
	ld ra, 2   # LED on duration
	ld rb, 10  # LED off duration
	mux_map_addr row_red
	trigger w{e}
loop:
	trigger s{e}
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
	trigger s{e}
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
	 end*/
/*
 * blink_solid2.src

row_red:   dw 0000000001001001b
row_green: dw 0000000010010010b
row_blue:  dw 0000000100100100b

.segment program1
	ld ra, 98
	ld rb, 0
	ld rc, 234
	trigger s{e}
	mux_map_addr row_red
loop:
	trigger w{e}
	set_pwm ra	; red intensity
	mux_map_addr row_green
	set_pwm rb	; green intensity
	mux_map_addr row_blue
	set_pwm rc	; blue intensity
wait1:
	wait 0.1
	branch 1, wait1
	trigger w{e}
	set_pwm 0
	mux_map_addr row_green
	set_pwm 0
	mux_map_addr row_red
	set_pwm 0
wait2:
	wait 0.1
	branch 1, wait2
	branch 0, loop

.segment program2
	 end

.segment program3
	 end
*/
static const uint8_t blink_dev1_text[] = {
	0x00,  0x49,  0x00,  0x92,  0x01,  0x24,  0x90,  0x02,
	0x94,  0x0a,  0x9f,  0x80,  0xf0,  0x00,  0xe0,  0x40,
	0x98,    98,  0x84,  0x62,  0x9f,  0x81,  0x98,     0,
	0x84,  0x62,  0x9f,  0x82,  0x98,   234,  0x84,  0x62,
	0x4c,  0x00,  0x86,  0x34,  0xe0,  0x40,  0x40,  0x00,
	0x9f,  0x81,  0x40,  0x00,  0x9f,  0x80,  0x40,  0x00,
	0x4c,  0x00,  0x86,  0x55,  0xa0,  0x04,  0xc0,  0x00,
	0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_dev1_program = {
	blink_dev1_text,
	sizeof(blink_dev1_text),
	0,
	{ 3,  27,  28,  }
};

static const uint8_t blink_dev2_text[] = {
	0x00,  0x49,  0x00,  0x92,  0x01,  0x24,  0x90,    98,
	0x94,     0,  0x98,   234,  0xe0,  0x40,  0x9f,  0x80,
	0xf0,  0x00,  0x84,  0x60,  0x9f,  0x81,  0x84,  0x61,
	0x9f,  0x82,  0x84,  0x62,  0x4c,  0x00,  0xa0,  0x8b,
	0xf0,  0x00,  0x40,  0x00,  0x9f,  0x81,  0x40,  0x00,
	0x9f,  0x80,  0x40,  0x00,  0x4c,  0x00,  0xa0,  0x93,
	0xa0,  0x05,  0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_dev2_program = {
	blink_dev2_text,
	sizeof(blink_dev2_text),
	0,
	{ 3,  25,  26,  }
};

static const uint8_t blink_rcv_remove1_text[] = {
	0x00,  0x49,  0x00,  0x92,  0x01,  0x24,  0x90,  0x02,
	0x94,  0x0a,  0x9f,  0x80,  0xf0,  0x00,  0xe0,  0x40,
	0x98,   255,  0x84,  0x62,  0x9f,  0x81,  0x98,    50,
	0x84,  0x62,  0x9f,  0x82,  0x98,   50,  0x84,  0x62,
	0x4c,  0x00,  0x86,  0x34,  0xe0,  0x40,  0x40,  0x00,
	0x9f,  0x81,  0x40,  0x00,  0x9f,  0x80,  0x40,  0x00,
	0x4c,  0x00,  0x86,  0x55,  0xa0,  0x04,  0xc0,  0x00,
	0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_rcv_remove1_program = {
	blink_rcv_remove1_text,
	sizeof(blink_rcv_remove1_text),
	0,
	{ 3,  27,  28,  }
};

static const uint8_t blink_rcv_remove2_text[] = {
	0x00,  0x49,  0x00,  0x92,  0x01,  0x24,  0x90,   255,
	0x94,    50,  0x98,    50,  0xe0,  0x40,  0x9f,  0x80,
	0xf0,  0x00,  0x84,  0x60,  0x9f,  0x81,  0x84,  0x61,
	0x9f,  0x82,  0x84,  0x62,  0x4c,  0x00,  0xa0,  0x8b,
	0xf0,  0x00,  0x40,  0x00,  0x9f,  0x81,  0x40,  0x00,
	0x9f,  0x80,  0x40,  0x00,  0x4c,  0x00,  0xa0,  0x93,
	0xa0,  0x05,  0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_rcv_remove2_program = {
	blink_rcv_remove2_text,
	sizeof(blink_rcv_remove2_text),
	0,
	{ 3,  25,  26,  }
};

static const uint8_t blink_rcv_insert1_text[] = {
	0x00,  0x49,  0x00,  0x92,  0x01,  0x24,  0x90,  0x02,
	0x94,  0x0a,  0x9f,  0x80,  0xf0,  0x00,  0xe0,  0x40,
	0x98,   255,  0x84,  0x62,  0x9f,  0x81,  0x98,    24,
	0x84,  0x62,  0x9f,  0x82,  0x98,     0,  0x84,  0x62,
	0x4c,  0x00,  0x86,  0x34,  0xe0,  0x40,  0x40,  0x00,
	0x9f,  0x81,  0x40,  0x00,  0x9f,  0x80,  0x40,  0x00,
	0x4c,  0x00,  0x86,  0x55,  0xa0,  0x04,  0xc0,  0x00,
	0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_rcv_insert1_program = {
	blink_rcv_insert1_text,
	sizeof(blink_rcv_insert1_text),
	0,
	{ 3,  27,  28,  }
};

static const uint8_t blink_rcv_insert2_text[] = {
	0x00,  0x49,  0x00,  0x92,  0x01,  0x24,  0x90,   255,
	0x94,    24,  0x98,     0,  0xe0,  0x40,  0x9f,  0x80,
	0xf0,  0x00,  0x84,  0x60,  0x9f,  0x81,  0x84,  0x61,
	0x9f,  0x82,  0x84,  0x62,  0x4c,  0x00,  0xa0,  0x8b,
	0xf0,  0x00,  0x40,  0x00,  0x9f,  0x81,  0x40,  0x00,
	0x9f,  0x80,  0x40,  0x00,  0x4c,  0x00,  0xa0,  0x93,
	0xa0,  0x05,  0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_rcv_insert2_program = {
	blink_rcv_insert2_text,
	sizeof(blink_rcv_insert2_text),
	0,
	{ 3,  25,  26,  }
};

static const uint8_t blink_rcv2dev1_text[] = {
	0x00,  0x49,  0x00,  0x92,  0x01,  0x24,  0x90,  0x02,
	0x94,  0x02,  0x9f,  0x80,  0xf0,  0x00,  0xe0,  0x40,
	0x98,    98,  0x84,  0x62,  0x9f,  0x81,  0x98,     0,
	0x84,  0x62,  0x9f,  0x82,  0x98,   234,  0x84,  0x62,
	0x4c,  0x00,  0x86,  0x34,  0xe0,  0x40,  0x40,  0x00,
	0x9f,  0x81,  0x40,  0x00,  0x9f,  0x80,  0x40,  0x00,
	0x4c,  0x00,  0x86,  0x55,  0xa0,  0x04,  0xc0,  0x00,
	0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_rcv2dev1_program = {
	blink_rcv2dev1_text,
	sizeof(blink_rcv2dev1_text),
	0,
	{ 3,  27,  28,  }
};

static const uint8_t blink_rcv2dev2_text[] = {
	0x00,  0x49,  0x00,  0x92,  0x01,  0x24,  0x90,    98,
	0x94,     0,  0x98,   234,  0xe0,  0x40,  0x9f,  0x80,
	0xf0,  0x00,  0x84,  0x60,  0x9f,  0x81,  0x84,  0x61,
	0x9f,  0x82,  0x84,  0x62,  0x4c,  0x00,  0xa0,  0x8b,
	0xf0,  0x00,  0x40,  0x00,  0x9f,  0x81,  0x40,  0x00,
	0x9f,  0x80,  0x40,  0x00,  0x4c,  0x00,  0xa0,  0x93,
	0xa0,  0x05,  0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program blink_rcv2dev2_program = {
	blink_rcv2dev2_text,
	sizeof(blink_rcv2dev2_text),
	0,
	{ 3,  25,  26,  }
};

/* Ping pong of colors (duty cycle 50%, odd and even LEDs' colors reversed. */
/*
 * ping_pong1.src
row1_red:   dw 0000000001000001b
row1_green: dw 0000000010000010b
row1_blue:  dw 0000000100000100b
row2_red:   dw 0000000000001000b
row2_green: dw 0000000000010000b
row2_blue:  dw 0000000000100000b

.segment program1
	ld ra, 5  # LED ping duration
	ld rb, 5  # LED pong duration
	trigger w{e}
loop:
	trigger s{e}
	mux_map_addr row1_red
	set_pwm 255
	mux_map_addr row1_green
	set_pwm 61
	mux_map_addr row1_blue
	set_pwm 0
	mux_map_addr row2_red
	set_pwm 255
	mux_map_addr row2_green
	set_pwm 238
	mux_map_addr row2_blue
	set_pwm 80
wait1:
	wait 0.1
	branch ra, wait1
	trigger s{e}
	mux_map_addr row1_red
	set_pwm 255
	mux_map_addr row1_green
	set_pwm 238
	mux_map_addr row1_blue
	set_pwm 80
	mux_map_addr row2_red
	set_pwm 255
	mux_map_addr row2_green
	set_pwm 61
	mux_map_addr row2_blue
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
/*
 * ping_pong2.src
row1_red:   dw 0000000000001000b
row1_green: dw 0000000000010000b
row1_blue:  dw 0000000000100000b
row2_red:   dw 0000000001000001b
row2_green: dw 0000000010000010b
row2_blue:  dw 0000000100000100b

.segment program1
	trigger s{e}
loop:
	trigger w{e}
	mux_map_addr row1_red
	set_pwm 255
	mux_map_addr row1_green
	set_pwm 61
	mux_map_addr row1_blue
	set_pwm 0
	mux_map_addr row2_red
	set_pwm 255
	mux_map_addr row2_green
	set_pwm 238
	mux_map_addr row2_blue
	set_pwm 80
wait1:
	wait 0.1
	branch 1, wait1
	trigger w{e}
	mux_map_addr row1_red
	set_pwm 255
	mux_map_addr row1_green
	set_pwm 238
	mux_map_addr row1_blue
	set_pwm 80
	mux_map_addr row2_red
	set_pwm 255
	mux_map_addr row2_green
	set_pwm 61
	mux_map_addr row2_blue
	set_pwm 0
wait2:
	wait 0.1
	branch 1, wait2
	branch 0, loop

.segment program2
	end

.segment program3
	end
*/

static const uint8_t ping_pong1_text[] = {
	0x00,  0x41,  0x00,  0x82,  0x01,  0x04,  0x00,  0x08,
	0x00,  0x10,  0x00,  0x20,  0x90,  0x05,  0x94,  0x05,
	0xf0,  0x00,  0xe0,  0x40,  0x9f,  0x80,  0x40,   255,
	0x9f,  0x81,  0x40,    24,  0x9f,  0x82,  0x40,     0,
	0x9f,  0x83,  0x40,   255,  0x9f,  0x84,  0x40,    80,
	0x9f,  0x85,  0x40,     0,  0x4c,  0x00,  0x86,  0x40,
	0xe0,  0x40,  0x9f,  0x80,  0x40,   255,  0x9f,  0x81,
	0x40,    80,  0x9f,  0x82,  0x40,     0,  0x9f,  0x83,
	0x40,   255,  0x9f,  0x84,  0x40,    24,  0x9f,  0x85,
	0x40,     0,  0x4c,  0x00,  0x86,  0x7d,  0xa0,  0x03,
	0xc0,  0x00,  0xc0,  0x00,  0x00,
};

static const TiLp55231Program ping_pong1_program = {
	ping_pong1_text,
	sizeof(ping_pong1_text),
	0,
	{ 6,  40,  41,  }
};

static const uint8_t ping_pong2_text[] = {
	0x00,  0x08,  0x00,  0x10,  0x00,  0x20,  0x00,  0x41,
	0x00,  0x82,  0x01,  0x04,  0xe0,  0x40,  0xf0,  0x00,
	0x9f,  0x80,  0x40,   255,  0x9f,  0x81,  0x40,    24,
	0x9f,  0x82,  0x40,     0,  0x9f,  0x83,  0x40,   255,
	0x9f,  0x84,  0x40,    80,  0x9f,  0x85,  0x40,     0,
	0x4c,  0x00,  0xa0,  0x8e,  0xf0,  0x00,  0x9f,  0x80,
	0x40,   255,  0x9f,  0x81,  0x40,    80,  0x9f,  0x82,
	0x40,     0,  0x9f,  0x83,  0x40,   255,  0x9f,  0x84,
	0x40,    24,  0x9f,  0x85,  0x40,     0,  0x4c,  0x00,
	0xa0,  0x9d,  0xa0,  0x01,  0xc0,  0x00,  0xc0,  0x00,
	0x00,
};

static const TiLp55231Program ping_pong2_program = {
	ping_pong2_text,
	sizeof(ping_pong2_text),
	0,
	{ 6,  38,  39,  }
};


/* Running multicolor lights, used as "unknown pattern" indicator. */
/* run_lights1.src
rowall: dw 0000000111111111b
row1:   dw 0000000000000001b
	dw 0000000000010000b
rowX:   dw 0000000100000000b

.segment program1	       ; Program for engine 1.
	mux_map_addr rowall
	set_pwm 0
loop:
	trigger w{e}
	mux_map_start row1
	mux_ld_end rowX
loop1:  ramp 0.1, 255
	ramp 0.1, -255
	mux_map_next
	branch 2, loop1
	trigger s{e}
	branch 0, loop

.segment program2
	end

.segment program3
	end
*/
/* run_lights2.src
rowall: dw 0000000111111111b
row1:   dw 0000000000000001b
	dw 0000000000010000b
rowX:   dw 0000000100000000b

.segment program1	       ; Program for engine 1.
	mux_map_addr rowall
	set_pwm 0	    ; Shut off all
	trigger s{e}
loop0:
	trigger w{e}
	mux_map_start row1   ; First row
	mux_ld_end rowX	     ; Last row
loop1:  ramp 0.1, 255	     ; Increase PWM 0 -> 255 in .1 second
	ramp 0.1, -255	     ; decrease PWM 255 -> 0 in .1 seconds
	mux_map_next	     ; Set the next row active in the mapping table.
	branch 2, loop1	     ; Loop 3 times (from first to last row)
	trigger s{e}
	branch 0, loop0

.segment program2
	end

.segment program3
	end
*/

static const uint8_t run_lights1_text[] = {
	0x01,  0xff,  0x00,  0x01,  0x00,  0x10,  0x01,  0x00,
	0x9f,  0x80,  0x40,  0x00,  0xf0,  0x00,  0x9c,  0x01,
	0x9c,  0x83,  0x02,  0xff,  0x03,  0xff,  0x9d,  0x80,
	0xa1,  0x05,  0xe0,  0x40,  0xa0,  0x02,  0xc0,  0x00,
	0xc0,  0x00,  0x00,
};

static const TiLp55231Program run_lights1_program = {
	run_lights1_text,
	sizeof(run_lights1_text),
	0,
	{ 4,  15,  16,  }
};
static const uint8_t run_lights2_text[] = {
	0x01,  0xff,  0x00,  0x01,  0x00,  0x10,  0x01,  0x00,
	0x9f,  0x80,  0x40,  0x00,  0xe0,  0x40,  0xf0,  0x00,
	0x9c,  0x01,  0x9c,  0x83,  0x02,  0xff,  0x03,  0xff,
	0x9d,  0x80,  0xa1,  0x06,  0xe0,  0x40,  0xa0,  0x03,
	0xc0,  0x00,  0xc0,  0x00,  0x00,
};
static const TiLp55231Program run_lights2_program = {
	run_lights2_text,
	sizeof(run_lights2_text),
	0,
	{ 4,  16,  17,  }
};


const WwRingStateProg wwr_state_programs[] = {
	/*
	 * for test purposes the blank screen program is set to blinking, will
	 * be changed soon.
	 */
	{VB_SCREEN_BLANK, {&solid_000000_program, &solid_000000_program} },
	{VB_SCREEN_DEVELOPER_WARNING, {&blink_dev1_program,
				       &blink_dev2_program} },
	{VB_SCREEN_RECOVERY_REMOVE, {&blink_rcv_remove1_program,
				     &blink_rcv_remove2_program} },
	{VB_SCREEN_RECOVERY_INSERT, {&blink_rcv_insert1_program,
				     &blink_rcv_insert2_program} },
	{VB_SCREEN_RECOVERY_TO_DEV, {&blink_rcv2dev1_program,
				     &blink_rcv2dev2_program} },
	{VB_SCREEN_RECOVERY_NO_GOOD, {&ping_pong1_program,
				      &ping_pong2_program} },
	{VB_SCREEN_DEVELOPER_EGG, {&run_lights1_program,
				   &run_lights2_program} },
	{}, /* Empty record to mark the end of the table. */
};
