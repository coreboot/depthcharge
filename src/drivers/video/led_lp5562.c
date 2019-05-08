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

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/video/led_lp5562_programs.h"

/* Key lp5562 registers. */
#define LP5562_ENABLE_REG       0x00
#define LP5562_OPMODE_REG       0x01
#define LP5562_PWMREG_B         0x02
#define LP5562_PWMREG_G         0x03
#define LP5562_PWMREG_R         0x04
#define LP5562_CURRENT_B        0x05
#define LP5562_CURRENT_G        0x06
#define LP5562_CURRENT_R        0x07
#define LP5562_CONFIG_REG       0x08
#define LP5562_ENGINE1_PC       0x09
#define LP5562_ENGINE2_PC       0x0a
#define LP5562_ENGINE3_PC       0x0b
#define LP5562_STATUS_REG       0x0c
#define LP5562_RESET_REG        0x0d
#define LP5562_PWMREG_W         0x0e
#define LP5562_CURRENT_W        0x0f
#define LP5562_LED_MAP_REG      0x70
#define LP5562_PROG_MEM_ENG1    0x10
#define LP5562_PROG_MEM_ENG2    0x30
#define LP5562_PROG_MEM_ENG3    0x50

/* LP5562 ENABLE REG fields. */
#define LP5562_ENABLE_LOG_EN         0x80
#define LP5562_ENABLE_CHIP_EN        0x40
#define LP5562_ENABLE_EXEC_HOLD      0x00
#define LP5562_ENABLE_EXEC_STEP      0x01
#define LP5562_ENABLE_EXEC_RUN       0x02
#define LP5562_ENABLE_EXEC_MASK      0x03
#define LP5562_ENABLE_ENG1_SHIFT     4
#define LP5562_ENABLE_ENG2_SHIFT     2
#define LP5562_ENABLE_ENG3_SHIFT     0
#define LP5562_ENABLE_ALL_MASK       0x3f
#define LP5562_ENABLE_ALL_HOLD       0x00
#define LP5562_ENABLE_ALL_RUN        0x2a

/* LP5562 OPMODE REG fields. */
#define LP5562_OPMODE_DISABLED       0x00
#define LP5562_OPMODE_LOAD           0x01
#define LP5562_OPMODE_RUN            0x02
#define LP5562_OPMODE_DIRECT         0x03
#define LP5562_OPMODE_MASK           0x03
#define LP5562_OPMODE_ENG1_SHIFT     4
#define LP5562_OPMODE_ENG2_SHIFT     2
#define LP5562_OPMODE_ENG3_SHIFT     0
#define LP5562_OPMODE_ALL_DISABLE    0x00
#define LP5562_OPMODE_ALL_LOAD       0x15
#define LP5562_OPMODE_ALL_RUN        0x2a

/*
 * LP5562_ENABLE_REG, default value
 */
#define LP5562_ENABLE_REG_DEFAULT    (LP5562_ENABLE_LOG_EN|LP5562_ENABLE_CHIP_EN)

/*
 * LP5562_CONFIG_REG, default value
 * PWM_HF=0(256Hz)
 * PWRSAVE_EN=0 (Disable)
 * CLKDET_EN/INT_CLK_EN=00 (External Clock Source)
 */
/* #define LP5562_CONFIG_REG_DEFAULT    0x00 */
#define LP5562_CONFIG_REG_DEFAULT    0x03 /* Internal Clock */

/*
 * LP5562_LED_MAP_REG, default value
 * B:ENG1, G:ENG2, R:ENG3
 */
#define LP5562_LED_MAP_REG_DEFAULT   0x39

/* LP5562 Current default value, applies to all four of them */
#define LP5562_CRT_CTRL_DEFAULT      0xaf

/* Goes into LP5562_RESET_REG to reset the chip. */
#define LP5562_RESET_VALUE           0xff

/*
 * The controller has 96 bytes of SRAM for code/data, available as
 * three 32 byte pages.
 */
#define LP5562_PROG_PAGE_SIZE   32
#define LP5562_PROG_PAGES       3
#define LP5562_MAX_PROG_SIZE    LP5562_PROG_PAGE_SIZE

/*
 * Structure to cache data relevant to accessing one controller. I2c interface
 * to use, device address on the i2c bus and a data buffer for write
 * transactions. The most bytes sent at a time is the register address plus
 * the program page size.
 */
typedef struct {
	I2cOps *ops;
	uint8_t  dev_addr;
	uint8_t  data_buffer[LP5562_PROG_PAGE_SIZE + 1];
} TiLp5562;

/* Dynamicaly allocated descriptors, one per controller */
static TiLp5562 *lp5562s;

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

static int ledc_transfer(TiLp5562 *ledc, I2cSeg *segs,
			 int seg_count, int reset)
{
	int rv, max_attempts = 2;

	max_attempts = 2;

	while (max_attempts--) {

		rv = ledc->ops->transfer(ledc->ops, segs, seg_count);

		/* Accessing reset register is expected to fail. */
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
static int ledc_write(TiLp5562 *ledc, uint8_t start_addr,
		      const uint8_t *data, unsigned int count)
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

	return ledc_transfer(ledc, &seg, 1, start_addr == LP5562_RESET_REG);
}

/* To keep things simple, read is limited to one byte at a time. */
static int ledc_read(TiLp5562 *ledc, uint8_t addr, uint8_t *data)
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
			     addr == LP5562_RESET_REG);
}

/*
 * Reset transaction is expected to result in a failing i2c command. But even
 * before trying it, read the reset register, which is supposed to always
 * return 0. If this fails - there is no lp5562 at this address.
 *
 * Return 0 on success, -1 on failure to detect controller.
 */
static int ledc_reset(TiLp5562 *ledc)
{
	uint8_t data;

	data = ~0;
	ledc_read(ledc, LP5562_RESET_REG, &data);
	if (data) {
		printf("LED_LP5562: no controller found at %#2.2x\n",
		       ledc->dev_addr);
		return -1;
	}

	data = LP5562_RESET_VALUE;
	ledc_write(ledc, LP5562_RESET_REG, &data, 1);

	/*
	 * This read is not necessary for the chip reset, but is required to
	 * work around the i2c driver bug where the missing ACK on the last
	 * byte of the write transaction is ignored, but the next transaction
	 * fails.
	 */
	ledc_read(ledc, LP5562_RESET_REG, &data);
	return 0;
}

/*
 * Write a program into the internal lp5562 memory.
 */
static void ledc_write_program(TiLp5562 *ledc, uint8_t load_addr,
			       const uint8_t *program, unsigned int count)
{
	if (count > LP5562_MAX_PROG_SIZE) {
		printf("%s: program of size %#x does not fit at addr %#x\n",
		       __func__, count, load_addr);
		return;
	}
	ledc_write(ledc, load_addr, program, count);
}

static void ledc_write_enable(TiLp5562 *ledc, uint8_t value)
{
	ledc_write(ledc, LP5562_ENABLE_REG, &value, 1);
	udelay(500); /* Must wait longer than 488usec */
}

static void ledc_write_opmode(TiLp5562 *ledc, uint8_t value)
{
	ledc_write(ledc, LP5562_OPMODE_REG, &value, 1);
	udelay(160); /* Must wait longer than 153usec */
}

/* Run an lp5562 program on a controller. */
static void ledc_run_program(TiLp5562 *ledc,
			     const TiLp5562Program *program_desc)
{
	int i;
	uint8_t data;
	uint8_t enable_reg;
	uint8_t load_addr;

	/* All engines on hold. */
	ledc_read(ledc, LP5562_ENABLE_REG, &enable_reg);
	enable_reg &= ~LP5562_ENABLE_ALL_MASK;
	enable_reg |= LP5562_ENABLE_CHIP_EN;
	enable_reg |= LP5562_ENABLE_ALL_HOLD;
	ledc_write_enable(ledc, enable_reg);

	/* All engines in LOAD mode. */
	ledc_write_opmode(ledc, LP5562_OPMODE_ALL_LOAD);

	/* Inject program code for each engine and start address */
	load_addr = LP5562_PROG_MEM_ENG1;
	for (i = 0; i < LED_LP5562_NUM_OF_ENGINES; i++) {
		ledc_write_program(ledc, load_addr,
				   program_desc->engine_program[i].program_text,
				   program_desc->engine_program[i].program_size);
		data = program_desc->engine_program[i].engine_start_addr;
		ledc_write(ledc, LP5562_ENGINE1_PC + i, &data, 1);
		udelay(160); /* Must wait longer than 153usec */
		load_addr += LP5562_PROG_PAGE_SIZE;
	}

	/* All engines on run. */
	ledc_write_opmode(ledc, LP5562_OPMODE_ALL_RUN);

	enable_reg &= ~LP5562_ENABLE_ALL_MASK;
	enable_reg |= LP5562_ENABLE_ALL_RUN;
	ledc_write_enable(ledc, enable_reg);
}

/*
 * Initialize a controller to a state were it is ready to accept programs, and
 * try to confirm that we are in fact talking to a lp5562
 */
static int ledc_init_validate(TiLp5562 *ledc)
{
	uint8_t data;
	int i;

	if (ledc_reset(ledc))
		return -1;

	/* Enable the chip, keep engines in hold state. */
	ledc_write_enable(ledc, LP5562_ENABLE_REG_DEFAULT | LP5562_ENABLE_ALL_HOLD);

	data = LP5562_CONFIG_REG_DEFAULT;
	ledc_write(ledc, LP5562_CONFIG_REG, &data, 1);

	data = LP5562_LED_MAP_REG_DEFAULT;
	ledc_write(ledc, LP5562_LED_MAP_REG, &data, 1);

	/*
	 * All four current control registers are supposed to return the same
	 * value at reset.
	 */
	for (i = 0; i < 4; i++) {
		uint8_t reg = (i < 3) ? LP5562_CURRENT_B + i : LP5562_CURRENT_W;
		ledc_read(ledc, reg, &data);
		if (data != LP5562_CRT_CTRL_DEFAULT) {
			printf("%s: read %#2.2x from register %#x\n", __func__,
			       data, reg);
			return -1;
		}
	}

	return 0;
}

/*
 * Find a program matching screen type, and run it on both controllers, if
 * found.
 */
int led_lp5562_display_screen(DisplayOps *me,
			      enum VbScreenType_t screen_type)
{
	const Led5562StateProg *state_program;

	for (state_program = led_lp5562_state_programs;
	     state_program->programs[0];
	     state_program++)
		if (state_program->vb_screen == screen_type) {
			int j;

			/*
			 * First stop all running programs to avoid
			 * inerference between the controllers.
			 */
			for (j = 0; j < LED_LP5562_NUM_LED_CONTROLLERS; j++) {
				if (!lp5562s[j].dev_addr)
					continue;
				ledc_write_opmode
					(lp5562s + j,
					 LP5562_OPMODE_ALL_DISABLE);
			}

			for (j = 0; j < LED_LP5562_NUM_LED_CONTROLLERS; j++) {
				if (!lp5562s[j].dev_addr)
					continue;
				ledc_run_program(lp5562s + j,
						 state_program->programs[j]);
			}
			return 0;
		}

	printf("%s: did not find program for screen %d\n",
	       __func__, screen_type);

	return -1;
}

static int led_lp5562_init(DisplayOps *me)
{
	Lp5562DisplayOps *display;
	TiLp5562 *ledc;
	int i, count = 0;

	if (lp5562s)
		return 0; /* Already initialized. */

	lp5562s = xzalloc(sizeof(*ledc) * LED_LP5562_NUM_LED_CONTROLLERS);
	display = container_of(me, Lp5562DisplayOps, lp5562_display_ops);

	for (i = 0, ledc = lp5562s;
	     i < LED_LP5562_NUM_LED_CONTROLLERS;
	     i++, ledc++) {

		ledc->ops = display->lp5562_i2c_ops;
		ledc->dev_addr = display->lp5562_base_addr + i;

		if (!ledc_init_validate(ledc))
			count++;
		else
			ledc->dev_addr = 0; /* Mark disabled. */
	}

	printf("LED_LP5562: initialized %d out of %d\n", count, i);
	if (count != i) {
		if (count)
			printf("LED_LP5562: will keep going anyway\n");
		else
			printf("LED_LP5562: LED ring not present\n");
	}

	return 0;
}


#define LP5562_I2C_BASE_ADDR 0x30

DisplayOps *new_led_lp5562_display(I2cOps *i2cOps, uint8_t base_addr)
{
	Lp5562DisplayOps *lp5562_display_ops = xzalloc
		(sizeof(*lp5562_display_ops));

	lp5562_display_ops->lp5562_display_ops.display_screen =
		led_lp5562_display_screen;

	lp5562_display_ops->lp5562_display_ops.init = led_lp5562_init;
	lp5562_display_ops->lp5562_i2c_ops = i2cOps;
	lp5562_display_ops->lp5562_base_addr = LP5562_I2C_BASE_ADDR;

	return &lp5562_display_ops->lp5562_display_ops;
}
