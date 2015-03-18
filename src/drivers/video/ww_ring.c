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

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/video/ww_ring_programs.h"

/* I2c address of the first of the controllers, the rest are contiguous. */
#define WW_RING_BASE_ADDR	0x32

/* Key lp55231 registers. */
#define LP55231_ENGCTRL1_REG	0x00
#define LP55231_ENGCTRL2_REG	0x01
#define LP55231_D1_CRT_CTRL_REG	0x26
#define LP55231_MISC_REG	0x36
#define LP55231_VARIABLE_REG	0x3c
#define LP55231_RESET_REG	0x3d
#define LP55231_ENG1_PROG_START	0x4c
#define LP55231_PROG_PAGE_REG	0x4f
#define LP55231_PROG_BASE_REG	0x50

/* LP55231_D1_CRT_CTRL_REG, default value, applies to all nine of them */
#define LP55231_CRT_CTRL_DEFAULT 0xaf

/* LP55231_ENGCTRL1_REG	fields */
#define LP55231_ENGCTRL1_CHIP_EN     0x40
#define LP55231_ENGCTRL1_ALL_ENG_GO  0x2a

/* LP55231_ENGCTRL2_REG	fields. */
#define LP55231_ENGCTRL2_ALL_DISABLE 0
#define LP55231_ENGCTRL2_ALL_LOAD    0x15
#define LP55231_ENGCTRL2_ALL_RUN     0x2a

/* LP55231_MISC_REG fields. */
#define LP55231_MISC_AUTOINCR  (1 << 6)
#define LP55231_MISC_PUMP_1X   (1 << 3)
#define LP55231_MISC_INT_CLK   (3 << 0)

/*
 * LP55231_VARIABLE_REG cookie value. It indicates to depthcharge that the
 * ring has been initialized by coreboot.
 */
#define LP55231_VARIABLE_COOKIE	0xb4

/* Goes into LP55231_RESET_REG to reset the chip. */
#define LP55231_RESET_VALUE	0xff

/*
 * The controller has 192 bytes of SRAM for code/data, availabe as six 32 byte
 * pages.
 */
#define LP55231_PROG_PAGE_SIZE  32
#define LP55231_PROG_PAGES      6
#define LP55231_MAX_PROG_SIZE  (LP55231_PROG_PAGE_SIZE * LP55231_PROG_PAGES)

/*
 * Structure to cache data relevant to accessing one controller. I2c interface
 * to use, device address on the i2c bus and a data buffer for write
 * transactions. The most bytes sent at a time is the register address plus
 * the program page size.
 */
typedef struct {
	I2cOps *ops;
	uint8_t dev_addr;
	uint8_t data_buffer[LP55231_PROG_PAGE_SIZE + 1];
} TiLp55231;

/* Dynamicaly allocated descriptors, one per controller */
static TiLp55231 *lp55231s;


/*
 * i2c transfer function for the driver. To keep things simple, the function
 * repeats the transfer, if the first attempt fails. This is OK with the
 * controller and makes it easier to handle errors.
 *
 * Note that the reset register accesses are expected to fail on writes, but
 * due to a bug in the ipq806x i2c controller, the error is reported on the
 * following read attempt.
 *
 * To work around this the driver writes and then reads the reset register,
 * the transfer function ignores errors when accessing the reset register.
 */

static int ledc_transfer(TiLp55231 *ledc, I2cSeg *segs,
			 int seg_count, int reset)
{
	int rv, max_attempts = 2;

	max_attempts = 2;

	while (max_attempts--) {

		rv = ledc->ops->transfer(ledc->ops, segs, seg_count);

		/* Accessing reset regsiter is expected to fail. */
		if (!rv || reset)
			break;
	}

	if (rv) {
		if (!reset)
			printf("%s: dev %#x, reg %#x, %s transaction error.\n",
			       __func__, segs->chip, segs->buf[0],
			       seg_count == 1 ? "write" : "read");
		else
			rv = 0;
	}

	return rv;
}

/*
 * The controller is programmed to autoincrement on writes, so up to page size
 * bytes can be transmitted in one write transaction.
 */
static int ledc_write(TiLp55231 *ledc, uint8_t start_addr,
		      const uint8_t *data, unsigned count)
{
	I2cSeg seg;

	if (count > (sizeof(ledc->data_buffer) - 1)) {
		printf("%s: transfer size too large (%d bytes)\n",
		       __func__, count);
		return -1;
	}

	memcpy(ledc->data_buffer + 1, data, count);
	ledc->data_buffer[0] = start_addr;

	seg.read = 0;
	seg.chip = ledc->dev_addr;
	seg.buf = ledc->data_buffer;
	seg.len = count + 1;

	return ledc_transfer(ledc, &seg, 1, start_addr == LP55231_RESET_REG);
}

/* To keep things simple, read is limited to one byte at a time. */
static int ledc_read(TiLp55231 *ledc, uint8_t addr, uint8_t *data)
{
	I2cSeg seg[2];

	seg[0].read = 0;
	seg[0].chip = ledc->dev_addr;
	seg[0].buf = &addr;
	seg[0].len =1;

	seg[1].read = 1;
	seg[1].chip = ledc->dev_addr;
	seg[1].buf = data;
	seg[1].len =1;

	return ledc_transfer(ledc, seg, ARRAY_SIZE(seg),
			     addr == LP55231_RESET_REG);
}

/*
 * Reset the LED ring if necessary.
 *
 * This function detects three conditions:
 *  - LED controller not present
 *  - LED controller present, but not initialized by coreboot
 *  - LED controller present and initialized by coreboot
 *
 * In case the controller is present but notinitialized, the reset command is
 * issued. It is expected to result in a failing i2c transaction, the next
 * read command is restoring the i2c bus condition.
 *
 * Return -1 on failure to detect controller, 0 on finding an uninitialized
 * controller, 1on finding an initialized controller.
 */
static int ledc_reset(TiLp55231 *ledc)
{
	uint8_t data;

	data = ~0;
	ledc_read(ledc, LP55231_RESET_REG, &data);
	if (data) {
		printf("WW_RING: no controller found at %#2.2x\n",
		       ledc->dev_addr);
		return -1;
	}

	ledc_read(ledc, LP55231_VARIABLE_REG, &data);
	if (data == LP55231_VARIABLE_COOKIE) {
		data = 0;
		printf("WW_RING: initialized controller found at %#2.2x\n",
		       ledc->dev_addr);
		/* make sure this condition does not persist. */
		ledc_write(ledc, LP55231_VARIABLE_REG, &data, 1);
		return 1;
	}

	data = LP55231_RESET_VALUE;
	ledc_write(ledc, LP55231_RESET_REG, &data, 1);

	/*
	 * This read is not necessary for the chip reset, but is required to
	 * work around the i2c driver bug where the missing ACK on the last
	 * byte of the write transaction is ignored, but the next transaction
	 * fails.
	 */
	ledc_read(ledc, LP55231_RESET_REG, &data);
	return 0;
}

/*
 * Write a program into the internal lp55231 memory. Split write transactions
 * into sections fitting into memory pages.
 */
static void ledc_write_program(TiLp55231 *ledc, uint8_t load_addr,
			       const uint8_t *program, unsigned count)
{
	uint8_t page_num = load_addr / LP55231_PROG_PAGE_SIZE;
	unsigned page_offs = load_addr % LP55231_PROG_PAGE_SIZE;

	if ((load_addr + count) > LP55231_MAX_PROG_SIZE) {
		printf("%s: program of size %#x does not fit at addr %#x\n",
		       __func__, count, load_addr);
		return;
	}

	while (count) {
		unsigned segment_size = LP55231_PROG_PAGE_SIZE - page_offs;

		if (segment_size > count)
			segment_size = count;

		ledc_write(ledc, LP55231_PROG_PAGE_REG, &page_num, 1);
		ledc_write(ledc, LP55231_PROG_BASE_REG + page_offs,
			   program, segment_size);

		count -= segment_size;
		program += segment_size;
		page_offs = 0;
		page_num++;
	}
}

static void ledc_write_engctrl2(TiLp55231 *ledc, uint8_t value)
{
	ledc_write(ledc, LP55231_ENGCTRL2_REG, &value, 1);
	udelay(1500);
}

/* Run an lp55231 program on a controller. */
static void ledc_run_program(TiLp55231 *ledc,
			     const TiLp55231Program *program_desc)
{
	int i;
	uint8_t data;

	/* All engines on hold. */
	data = LP55231_ENGCTRL1_CHIP_EN;
	ledc_write(ledc, LP55231_ENGCTRL1_REG, &data, 1);

	ledc_write_engctrl2(ledc, LP55231_ENGCTRL2_ALL_LOAD);

	ledc_write_program(ledc, program_desc->load_addr,
			   program_desc->program_text,
			   program_desc->program_size);

	for (i = 0; i < sizeof(program_desc->engine_start_addr); i++)
		ledc_write(ledc, LP55231_ENG1_PROG_START + i,
			   program_desc->engine_start_addr + i, 1);

	data = LP55231_ENGCTRL1_CHIP_EN | LP55231_ENGCTRL1_ALL_ENG_GO;
	ledc_write(ledc, LP55231_ENGCTRL1_REG, &data, 1);
	ledc_write_engctrl2(ledc, LP55231_ENGCTRL2_ALL_RUN);
}

/*
 * Initialize a controller to a state were it is ready to accept programs, and
 * try to confirm that we are in fact talking to a lp55231
 */
static int ledc_init_validate(TiLp55231 *ledc)
{
	uint8_t data;
	int i;

	switch (ledc_reset(ledc)) {
	case -1:
		return -1;
	case 1:
		return 0;
	default:
		break;
	}

	/* Enable the chip, keep engines in hold state. */
	data = LP55231_ENGCTRL1_CHIP_EN;
	ledc_write(ledc, LP55231_ENGCTRL1_REG, &data, 1);

	/*
	 * Internal clock, 3.3V output (pump 1x), autoincrement on multibyte
	 * writes.
	 */
	data = LP55231_MISC_AUTOINCR |
		LP55231_MISC_PUMP_1X | LP55231_MISC_INT_CLK;
	ledc_write(ledc, LP55231_MISC_REG, &data, 1);

	/*
	 * All nine current control registers are supposed to return the same
	 * value at reset.
	 */
	for (i = 0; i < 9; i++) {
		ledc_read(ledc, LP55231_D1_CRT_CTRL_REG + i, &data);
		if (data != LP55231_CRT_CTRL_DEFAULT) {
			printf("%s: read %#2.2x from register %#x\n", __func__,
			       data, LP55231_D1_CRT_CTRL_REG + i);
			return -1;
		}
	}

	return 0;
}

/*
 * Find a program matching screen type, and run it on both controllers, if
 * found.
 */
static int ww_ring_display_screen(DisplayOps *me,
				  enum VbScreenType_t screen_type)
{
	const WwRingStateProg *state_program;

	for (state_program = wwr_state_programs;
	     state_program->programs[0];
	     state_program++)
		if (state_program->vb_screen == screen_type) {
			int j;

			/*
			 * First stop all running programs to avoid
			 * inerference between the controllers.
			 */
			for (j = 0; j < WW_RING_NUM_LED_CONTROLLERS; j++) {
				if (!lp55231s[j].dev_addr)
					continue;
				ledc_write_engctrl2
					(lp55231s + j,
					 LP55231_ENGCTRL2_ALL_DISABLE);
			}

			for (j = 0; j < WW_RING_NUM_LED_CONTROLLERS; j++) {
				if (!lp55231s[j].dev_addr)
					continue;
				ledc_run_program (lp55231s + j,
						  state_program->programs[j]);
			}
			return 0;
		}

	printf("%s: did not find program for screen %d\n",
	       __func__, screen_type);

	return -1;
}

static int ww_ring_init(DisplayOps *me)
{
	WwRingDisplayOps *display;
	TiLp55231 *ledc;
	int i, count = 0;

	if (lp55231s)
		return 0; /* Already initialized. */

	lp55231s = xzalloc(sizeof(*ledc) * WW_RING_NUM_LED_CONTROLLERS);
	display = container_of(me, WwRingDisplayOps, wwr_display_ops);

	for (i = 0, ledc = lp55231s;
	     i < WW_RING_NUM_LED_CONTROLLERS;
	     i++, ledc++) {

		ledc->ops = display->wwr_i2c_ops;
		ledc->dev_addr = display->wwr_base_addr + i;

		if (!ledc_init_validate(ledc))
			count++;
		else
			ledc->dev_addr = 0; /* Mark disabled. */
	}

	printf("WW_RING: initialized %d out of %d\n", count, i);
	if (count != i) {
		if (count)
			printf("WW_RING: will keep going anyway\n");
		else
			printf("WW_RING: LED ring not present\n");
	}

	return 0;
}

DisplayOps *new_ww_ring_display(I2cOps *i2cOps, uint8_t base_addr)
{
	WwRingDisplayOps *wwr_display_ops = xzalloc
		(sizeof(*wwr_display_ops));

	wwr_display_ops->wwr_display_ops.display_screen =
		ww_ring_display_screen;

	wwr_display_ops->wwr_display_ops.init = ww_ring_init;
	wwr_display_ops->wwr_i2c_ops = i2cOps;
	wwr_display_ops->wwr_base_addr = base_addr;

	return &wwr_display_ops->wwr_display_ops;
}
