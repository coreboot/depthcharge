/*
 * Copyright 2015 Marvell Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>
#include <stdio.h>
#include "config.h"
#include "base/container_of.h"
#include "armada38x_i2c.h"

#undef MV_DEBUG
//#define MV_DEBUG
#ifdef MV_DEBUG
#define DB(x) x
#else
#define DB(x)
#endif

#define MAX_I2C_NUM 2
#define TWSI_SPEED 100000

#define MAX_RETRY_CNT 1000
#define TWSI_TIMEOUT_VALUE 0x500

#define TWSI_SLAVE_ADDR_REG(bus_num) (bus_num * 0x100 + 0x00)

#define MV_CPUIF_REGS_OFFSET(cpu) (0x21800 + (cpu)*0x100)
#define MV_CPUIF_REGS_BASE(cpu) (0xF1000000 + MV_CPUIF_REGS_OFFSET(cpu))
#define CPU_MAIN_INT_CAUSE_REG(vec, cpu) \
	(MV_CPUIF_REGS_BASE(cpu) + 0x80 + (vec * 0x4))
#define CPU_MAIN_INT_TWSI_OFFS(i) (2 + i)
#define CPU_MAIN_INT_CAUSE_TWSI(i) (31 + i)

#define TWSI_CPU_MAIN_INT_CAUSE_REG(cpu) CPU_MAIN_INT_CAUSE_REG(1, (cpu))
#define MV_TWSI_CPU_MAIN_INT_CAUSE(ch_num, cpu) TWSI_CPU_MAIN_INT_CAUSE_REG(cpu)

#define MV_MBUS_REGS_OFFSET (0x20000)
#define MV_CPUIF_SHARED_REGS_BASE (0xF1000000 + MV_MBUS_REGS_OFFSET)
#define CPU_INT_SOURCE_CONTROL_REG(i) \
	(MV_CPUIF_SHARED_REGS_BASE + 0xB00 + (i * 0x4))

#define CPU_INT_SOURCE_CONTROL_IRQ_OFFS 28
#define CPU_INT_SOURCE_CONTROL_IRQ_MASK (1 << CPU_INT_SOURCE_CONTROL_IRQ_OFFS)

#define TWSI_SLAVE_ADDR_GCE_ENA (1 << 0)
#define TWSI_SLAVE_ADDR_7_BIT_OFFS 0x1
#define TWSI_SLAVE_ADDR_7_BIT_MASK (0xFF << TWSI_SLAVE_ADDR_7_BIT_OFFS)
#define TWSI_SLAVE_ADDR_10_BIT_OFFS 0x7
#define TWSI_SLAVE_ADDR_10_BIT_MASK 0x300
#define TWSI_SLAVE_ADDR_10_BIT_CONST 0xF0

#define TWSI_DATA_REG(bus_num) (bus_num * 0x100 + 0x04)
#define TWSI_DATA_COMMAND_OFFS 0x0
#define TWSI_DATA_COMMAND_MASK (0x1 << TWSI_DATA_COMMAND_OFFS)
#define TWSI_DATA_COMMAND_WR (0x1 << TWSI_DATA_COMMAND_OFFS)
#define TWSI_DATA_COMMAND_RD (0x0 << TWSI_DATA_COMMAND_OFFS)
#define TWSI_DATA_ADDR_7_BIT_OFFS 0x1
#define TWSI_DATA_ADDR_7_BIT_MASK (0xFF << TWSI_DATA_ADDR_7_BIT_OFFS)
#define TWSI_DATA_ADDR_10_BIT_OFFS 0x7
#define TWSI_DATA_ADDR_10_BIT_MASK 0x300
#define TWSI_DATA_ADDR_10_BIT_CONST 0xF0

#define TWSI_CONTROL_REG(bus_num) (bus_num * 0x100 + 0x08)
#define TWSI_CONTROL_ACK (1 << 2)
#define TWSI_CONTROL_INT_FLAG_SET (1 << 3)
#define TWSI_CONTROL_STOP_BIT (1 << 4)
#define TWSI_CONTROL_START_BIT (1 << 5)
#define TWSI_CONTROL_ENA (1 << 6)
#define TWSI_CONTROL_INT_ENA (1 << 7)

#define TWSI_STATUS_BAUDE_RATE_REG(bus_num) \
	(bus_num * 0x100 + 0x0c)
#define TWSI_BAUD_RATE_N_OFFS 0
#define TWSI_BAUD_RATE_N_MASK (0x7 << TWSI_BAUD_RATE_N_OFFS)
#define TWSI_BAUD_RATE_M_OFFS 3
#define TWSI_BAUD_RATE_M_MASK (0xF << TWSI_BAUD_RATE_M_OFFS)

#define TWSI_EXTENDED_SLAVE_ADDR_REG(bus_num) \
	(bus_num * 0x100 + 0x10)
#define TWSI_EXTENDED_SLAVE_OFFS 0
#define TWSI_EXTENDED_SLAVE_MASK (0xFF << TWSI_EXTENDED_SLAVE_OFFS)

#define TWSI_SOFT_RESET_REG(bus_num) (bus_num * 0x100 + 0x1c)

#define TWSI_BUS_ERROR 0x00
#define TWSI_START_CON_TRA 0x08
#define TWSI_REPEATED_START_CON_TRA 0x10
#define TWSI_AD_PLS_WR_BIT_TRA_ACK_REC 0x18
#define TWSI_AD_PLS_WR_BIT_TRA_ACK_NOT_REC 0x20
#define TWSI_M_TRAN_DATA_BYTE_ACK_REC 0x28
#define TWSI_M_TRAN_DATA_BYTE_ACK_NOT_REC 0x30
#define TWSI_M_LOST_ARB_DUR_AD_OR_DATA_TRA 0x38
#define TWSI_AD_PLS_RD_BIT_TRA_ACK_REC 0x40
#define TWSI_AD_PLS_RD_BIT_TRA_ACK_NOT_REC 0x48
#define TWSI_M_REC_RD_DATA_ACK_TRA 0x50
#define TWSI_M_REC_RD_DATA_ACK_NOT_TRA 0x58
#define TWSI_SLA_REC_AD_PLS_WR_BIT_ACK_TRA 0x60
#define TWSI_M_LOST_ARB_DUR_AD_TRA_AD_IS_TRGT_TO_SLA_ACK_TRA_W 0x68
#define TWSI_GNL_CALL_REC_ACK_TRA 0x70
#define TWSI_M_LOST_ARB_DUR_AD_TRA_GNL_CALL_AD_REC_ACK_TRA 0x78
#define TWSI_SLA_REC_WR_DATA_AF_REC_SLA_AD_ACK_TRAN 0x80
#define TWSI_SLA_REC_WR_DATA_AF_REC_SLA_AD_ACK_NOT_TRAN 0x88
#define TWSI_SLA_REC_WR_DATA_AF_REC_GNL_CALL_ACK_TRAN 0x90
#define TWSI_SLA_REC_WR_DATA_AF_REC_GNL_CALL_ACK_NOT_TRAN 0x98
#define TWSI_SLA_REC_STOP_OR_REPEATED_STRT_CON 0xA0
#define TWSI_SLA_REC_AD_PLS_RD_BIT_ACK_TRA 0xA8
#define TWSI_M_LOST_ARB_DUR_AD_TRA_AD_IS_TRGT_TO_SLA_ACK_TRA_R 0xB0
#define TWSI_SLA_TRA_RD_DATA_ACK_REC 0xB8
#define TWSI_SLA_TRA_RD_DATA_ACK_NOT_REC 0xC0
#define TWSI_SLA_TRA_LAST_RD_DATA_ACK_REC 0xC8
#define TWSI_SEC_AD_PLS_WR_BIT_TRA_ACK_REC 0xD0
#define TWSI_SEC_AD_PLS_WR_BIT_TRA_ACK_NOT_REC 0xD8
#define TWSI_SEC_AD_PLS_RD_BIT_TRA_ACK_REC 0xE0
#define TWSI_SEC_AD_PLS_RD_BIT_TRA_ACK_NOT_REC 0xE8
#define TWSI_NO_REL_STS_INT_FLAG_IS_KEPT_0 0xF8

/* The TWSI interface supports both 7-bit and 10-bit addressing.            */
/* This enumerator describes addressing type.                               */
typedef enum _mv_twsi_addr_type {
	ADDR7_BIT, /* 7 bit address    */
	ADDR10_BIT /* 10 bit address   */
} MV_TWSI_ADDR_TYPE;

/* This structure describes TWSI address.                                   */
typedef struct _mv_twsi_addr {
	uint32_t address;       /* address          */
	MV_TWSI_ADDR_TYPE type; /* Address type     */
} MV_TWSI_ADDR;

/* This structure describes a TWSI slave.                                   */
typedef struct _mv_twsi_slave {
	MV_TWSI_ADDR slave_addr;
	int valid_offset; /* whether the slave has offset (i.e. Eeprom  etc.) */
	uint32_t offset;  /* offset in the slave. */
	int more_than256; /* whether the ofset is bigger then 256 */
} MV_TWSI_SLAVE;

/* This enumerator describes TWSI protocol commands.                        */
typedef enum _mv_twsi_cmd {
	MV_TWSI_WRITE, /* TWSI write command - 0 according to spec   */
	MV_TWSI_READ   /* TWSI read command  - 1 according to spec */
} MV_TWSI_CMD;

static void twsi_int_flg_clr(Armada38xI2c *i2c);
static uint8_t twsi_main_int_get(uint8_t bus_num);
static void twsi_ack_bit_set(Armada38xI2c *i2c);
static uint32_t twsi_sts_get(Armada38xI2c *i2c);
static void twsi_reset(uint32_t base, uint8_t bus_num);
static int twsi_addr7_bit_set(Armada38xI2c *i2c,
			      uint32_t device_address,
			      MV_TWSI_CMD command);
static int twsi_addr10_bit_set(Armada38xI2c *i2c,
			       uint32_t device_address,
			       MV_TWSI_CMD command);
static int twsi_data_transmit(Armada38xI2c *i2c,
			      uint8_t *p_block,
			      uint32_t block_size);
static int twsi_data_receive(Armada38xI2c *i2c,
			     uint8_t *p_block,
			     uint32_t block_size);
static int twsi_target_offs_set(Armada38xI2c *i2c,
				uint32_t offset,
				uint8_t more_than256);
static int mv_twsi_start_bit_set(Armada38xI2c *i2c);
static int mv_twsi_stop_bit_set(Armada38xI2c *i2c);
static int mv_twsi_addr_set(Armada38xI2c *i2c,
			    MV_TWSI_ADDR *twsi_addr,
			    MV_TWSI_CMD command);
static uint32_t mv_twsi_init(uint32_t base,
			     uint8_t bus_num,
			     uint32_t frequency,
			     uint32_t tclk,
			     MV_TWSI_ADDR *twsi_addr,
			     uint8_t general_call_enable);
static int mv_twsi_read(Armada38xI2c *i2c,
			MV_TWSI_SLAVE *twsi_slave,
			uint8_t *p_block,
			uint32_t block_size);
static int mv_twsi_write(Armada38xI2c *i2c,
			 MV_TWSI_SLAVE *twsi_slave,
			 uint8_t *p_block,
			 uint32_t block_size);
static uint32_t who_am_i(void);

static inline uint32_t i2c_reg_read(uint32_t reg)
{
	return read32((void *)reg);
}
static inline void i2c_reg_write(uint32_t reg, uint32_t val)
{
	write32((void *)reg, val);
}
static inline void i2c_reg_bit_set(uint32_t reg, uint32_t bit_mask)
{
	i2c_reg_write(reg, (i2c_reg_read(reg) | bit_mask));
}
static inline void i2c_reg_bit_reset(uint32_t reg, uint32_t bit_mask)
{
	i2c_reg_write(reg, (i2c_reg_read(reg) & (~bit_mask)));
}

static uint8_t twsi_timeout_chk(uint32_t timeout, const char *p_string)
{
	if (timeout >= TWSI_TIMEOUT_VALUE) {
		DB(printf("%s", p_string));
		return 1;
	}
	return 0;
}

/*******************************************************************************
* mv_twsi_start_bit_set - Set start bit on the bus
*
* DESCRIPTION:
*       This routine sets the start bit on the TWSI bus.
*       The routine first checks for interrupt flag condition, then it sets
*       the start bit  in the TWSI Control register.
*       If the interrupt flag condition check previously was set, the function
*       will clear it.
*       The function then wait for the start bit to be cleared by the HW.
*       Then it waits for the interrupt flag to be set and eventually, the
*       TWSI status is checked to be 0x8 or 0x10(repeated start bit).
*
* INPUT:
*       i2c - i2c controller.
*
* OUTPUT:
*       None.
*
* RETURN:
*       0 if start bit was set successfuly on the bus.
*       -1 if start_bit not set or status does not indicate start condition
*	transmitted.
*
*******************************************************************************/
static int mv_twsi_start_bit_set(Armada38xI2c *i2c)
{
	uint8_t is_int_flag = 0;
	uint32_t timeout, temp;
	uint8_t bus_num;
	uint32_t base;

	DB(printf("TWSI: mv_twsi_start_bit_set\n"));
	bus_num = i2c->bus_num;
	base = i2c->base;
	/* check Int flag */
	if (twsi_main_int_get(bus_num))
		is_int_flag = 1;
	/* set start Bit */
	i2c_reg_bit_set(base + TWSI_CONTROL_REG(bus_num),
			TWSI_CONTROL_START_BIT);

	/* in case that the int flag was set before i.e. repeated start bit */
	if (is_int_flag) {
		DB(printf(
		    "TWSI: mv_twsi_start_bit_set repeated start Bit\n"));
		twsi_int_flg_clr(i2c);
	}

	/* wait for interrupt */
	timeout = 0;
	while (!twsi_main_int_get(bus_num) && (timeout++ < TWSI_TIMEOUT_VALUE))
		;

	/* check for timeout */
	if (1 ==
	    twsi_timeout_chk(timeout,
			     (const char *)"TWSI: Start Clear bit time_out.\n"))
		return -1;

	/* check that start bit go down */
	if ((i2c_reg_read(base + TWSI_CONTROL_REG(bus_num)) &
	     TWSI_CONTROL_START_BIT) != 0) {
		printf("TWSI: start bit didn't go down\n");
		return -1;
	}

	/* check the status */
	temp = twsi_sts_get(i2c);
	if ((TWSI_M_LOST_ARB_DUR_AD_OR_DATA_TRA == temp) ||
	    (TWSI_M_LOST_ARB_DUR_AD_TRA_GNL_CALL_AD_REC_ACK_TRA == temp)) {
		DB(printf("TWSI: Lost Arb, status %x\n", temp));
		return -2;
	} else if ((temp != TWSI_START_CON_TRA) &&
		   (temp != TWSI_REPEATED_START_CON_TRA)) {
		printf("TWSI: status %x after Set Start Bit.\n", temp);
		return -1;
	}

	return 0;
}

/*******************************************************************************
* mv_twsi_stop_bit_set - Set stop bit on the bus
*
* DESCRIPTION:
*       This routine set the stop bit on the TWSI bus.
*       The function then wait for the stop bit to be cleared by the HW.
*       Finally the function checks for status of 0xF8.
*
* INPUT:
*	i2c - i2c controller
*
* OUTPUT:
*       None.
*
* RETURN:
*       1 is stop bit was set successfuly on the bus.
*
*******************************************************************************/
static int mv_twsi_stop_bit_set(Armada38xI2c *i2c)
{
	uint32_t timeout, temp;
	uint8_t bus_num;
	uint32_t base;

	bus_num = i2c->bus_num;
	base = i2c->base;
	/* Generate stop bit */
	i2c_reg_bit_set(base + TWSI_CONTROL_REG(bus_num),
			TWSI_CONTROL_STOP_BIT);

	twsi_int_flg_clr(i2c);

	/* wait for stop bit to come down */
	timeout = 0;
	while (((i2c_reg_read(base + TWSI_CONTROL_REG(bus_num)) &
		 TWSI_CONTROL_STOP_BIT) != 0) &&
	       (timeout++ < TWSI_TIMEOUT_VALUE))
		;

	/* check for timeout */
	if (1 ==
	    twsi_timeout_chk(timeout,
			     (const char *)"TWSI: ERROR - Stop bit timeout\n"))
		return -1;

	/* check that the stop bit go down */
	if ((i2c_reg_read(base + TWSI_CONTROL_REG(bus_num)) &
	     TWSI_CONTROL_STOP_BIT) != 0) {
		printf(
		    "TWSI: ERROR - stop bit not go down\n");
		return -1;
	}

	/* check the status */
	temp = twsi_sts_get(i2c);
	if ((TWSI_M_LOST_ARB_DUR_AD_OR_DATA_TRA == temp) ||
	    (TWSI_M_LOST_ARB_DUR_AD_TRA_GNL_CALL_AD_REC_ACK_TRA == temp)) {
		DB(printf("TWSI: Lost Arb, status %x\n", temp));
		return -2;
	} else if (temp != TWSI_NO_REL_STS_INT_FLAG_IS_KEPT_0) {
		printf(
		    "TWSI: ERROR - status %x after Stop Bit\n",
		    temp);
		return -1;
	}

	return 0;
}

/*******************************************************************************
* twsi_main_int_get - Get twsi bit from main Interrupt cause.
*
* DESCRIPTION:
*       This routine returns the twsi interrupt flag value.
*
* INPUT:
*       None.
*
* OUTPUT:
*       None.
*
* RETURN:
*       1 is interrupt flag is set, 0 otherwise.
*
*******************************************************************************/
static uint32_t who_am_i(void)
{
	uint32_t value;

	__asm__ __volatile__("mrc p15, 0, %0, c0, c0, 5   @ read CPUID reg\n"
			     : "=r"(value)
			     :
			     : "memory");
	return value & 0x1;
}
static uint8_t twsi_main_int_get(uint8_t bus_num)
{
	uint32_t temp;

	/* get the int flag bit */
	temp = i2c_reg_read(MV_TWSI_CPU_MAIN_INT_CAUSE(bus_num, who_am_i()));
	if (temp & (1 << CPU_MAIN_INT_TWSI_OFFS(bus_num)))
		return 1;

	return 0;
}

/*******************************************************************************
* twsi_int_flg_clr - Clear Interrupt flag.
*
* DESCRIPTION:
*       This routine clears the interrupt flag. It does NOT poll the interrupt
*       to make sure the clear. After clearing the interrupt, it waits for at
*       least 1 miliseconds.
*
* INPUT:
*	i2c - i2c controller
*
* OUTPUT:
*       None.
*
* RETURN:
*       None.
*
*******************************************************************************/
static void twsi_int_flg_clr(Armada38xI2c *i2c)
{
	uint8_t bus_num;
	uint32_t base;

	bus_num = i2c->bus_num;
	base = i2c->base;
	/* wait for 1ms to prevent TWSI register write after write problems */
	mdelay(1);
	/* clear the int flag bit */
	i2c_reg_bit_reset(base + TWSI_CONTROL_REG(bus_num),
			TWSI_CONTROL_INT_FLAG_SET);
	/* wait for 1 mili sec for the clear to take effect */
	mdelay(1);
}

/*******************************************************************************
* twsi_ack_bit_set - Set acknowledge bit on the bus
*
* DESCRIPTION:
*       This routine set the acknowledge bit on the TWSI bus.
*
* INPUT:
*       i2c - i2c controller
*
* OUTPUT:
*       None.
*
* RETURN:
*       None.
*
*******************************************************************************/
static void twsi_ack_bit_set(Armada38xI2c *i2c)
{
	uint8_t bus_num;
	uint32_t base;

	bus_num = i2c->bus_num;
	base = i2c->base;
	/*Set the Ack bit */
	i2c_reg_bit_set(base + TWSI_CONTROL_REG(bus_num),
			TWSI_CONTROL_ACK);
	/* Add delay of 1ms */
	mdelay(1);
}

/*******************************************************************************
* twsi_init - Initialize TWSI interface
*
* DESCRIPTION:
*       This routine:
*	-Reset the TWSI.
*	-Initialize the TWSI clock baud rate according to given frequency
*	 parameter based on tclk frequency and enables TWSI slave.
*       -Set the ack bit.
*	-Assign the TWSI slave address according to the TWSI address Type.
*
* INPUT:
*       base - TWSI base register
*	bus_num - TWSI channel
*       frequency - TWSI frequency in KHz. (up to 100_kHZ)
*
* OUTPUT:
*       None.
*
* RETURN:
*       Actual frequency.
*
*******************************************************************************/
static uint32_t mv_twsi_init(uint32_t base,
			     uint8_t bus_num,
			     uint32_t frequency,
			     uint32_t tclk,
			     MV_TWSI_ADDR *p_twsi_addr,
			     uint8_t general_call_enable)
{
	uint32_t n, m, freq, margin, min_margin = 0xffffffff;
	uint32_t power;
	uint32_t actual_freq = 0, actual_n = 0, actual_m = 0, val;

	if (frequency > 100000)
		die("TWSI frequency is too high.\n");

	DB(printf("TWSI: mv_twsi_init - tclk = %d freq = %d\n", tclk,
			frequency));
	/* Calucalte N and M for the TWSI clock baud rate */
	for (n = 0; n < 8; n++) {
		for (m = 0; m < 16; m++) {
			power = 2 << n; /* power = 2^(n+1) */
			freq = tclk / (10 * (m + 1) * power);
			margin = abs(frequency - freq);

			if ((freq <= frequency) && (margin < min_margin)) {
				min_margin = margin;
				actual_freq = freq;
				actual_n = n;
				actual_m = m;
			}
		}
	}
	DB(printf("TWSI: mv_twsi_init - act_n %u act_m %u act_freq %u\n",
			actual_n, actual_m, actual_freq));
	/* Reset the TWSI logic */
	twsi_reset(base, bus_num);

	/* Set the baud rate */
	val = ((actual_m << TWSI_BAUD_RATE_M_OFFS) |
	       actual_n << TWSI_BAUD_RATE_N_OFFS);
	i2c_reg_write(base + TWSI_STATUS_BAUDE_RATE_REG(bus_num), val);

	/* Enable the TWSI and slave */
	i2c_reg_write(base + TWSI_CONTROL_REG(bus_num),
		       TWSI_CONTROL_ENA | TWSI_CONTROL_ACK);

	/* set the TWSI slave address */
	if (p_twsi_addr->type == ADDR10_BIT) {
		/* writing the 2 most significant bits of the 10 bit address */
		val = ((p_twsi_addr->address & TWSI_SLAVE_ADDR_10_BIT_MASK) >>
		       TWSI_SLAVE_ADDR_10_BIT_OFFS);
		/* bits 7:3 must be 0x11110 */
		val |= TWSI_SLAVE_ADDR_10_BIT_CONST;
		/* set GCE bit */
		if (general_call_enable)
			val |= TWSI_SLAVE_ADDR_GCE_ENA;
		/* write slave address */
		i2c_reg_write(base + TWSI_SLAVE_ADDR_REG(bus_num), val);

		/* writing the 8 least significant bits of the 10 bit address */
		val = (p_twsi_addr->address << TWSI_EXTENDED_SLAVE_OFFS) &
		      TWSI_EXTENDED_SLAVE_MASK;
		i2c_reg_write(base + TWSI_EXTENDED_SLAVE_ADDR_REG(bus_num),
				val);
	} else {
		/* set the 7 Bits address */
		i2c_reg_write(base + TWSI_EXTENDED_SLAVE_ADDR_REG(bus_num),
				0x0);
		val = (p_twsi_addr->address << TWSI_SLAVE_ADDR_7_BIT_OFFS) &
		      TWSI_SLAVE_ADDR_7_BIT_MASK;
		i2c_reg_write(base + TWSI_SLAVE_ADDR_REG(bus_num), val);
	}

	/* unmask twsi int */
	i2c_reg_bit_set(base + TWSI_CONTROL_REG(bus_num),
			TWSI_CONTROL_INT_ENA);

	/* unmask twsi int in Interrupt source control register */
	i2c_reg_bit_set(CPU_INT_SOURCE_CONTROL_REG(
			CPU_MAIN_INT_CAUSE_TWSI(bus_num)),
			(1 << CPU_INT_SOURCE_CONTROL_IRQ_OFFS));

	/* Add delay of 1ms */
	mdelay(1);

	return actual_freq;
}

/*******************************************************************************
* twsi_sts_get - Get the TWSI status value.
*
* DESCRIPTION:
*       This routine returns the TWSI status value.
*
* INPUT:
*	i2c - i2c controller
*
* OUTPUT:
*       None.
*
* RETURN:
*       uint32_t - the TWSI status.
*
*******************************************************************************/
static uint32_t twsi_sts_get(Armada38xI2c *i2c)
{
	uint8_t bus_num;
	uint32_t base;

	bus_num = i2c->bus_num;
	base = i2c->base;
	return i2c_reg_read(base + TWSI_STATUS_BAUDE_RATE_REG(bus_num));
}

/*******************************************************************************
* twsi_reset - Reset the TWSI.
*
* DESCRIPTION:
*       Resets the TWSI logic and sets all TWSI registers to their reset values.
*
* INPUT:
*      base - TWSI base
*      bus_num - TWSI channel
*
* OUTPUT:
*       None.
*
* RETURN:
*       None
*
*******************************************************************************/
static void twsi_reset(uint32_t base, uint8_t bus_num)
{
	/* Reset the TWSI logic */
	i2c_reg_write(base + TWSI_SOFT_RESET_REG(bus_num), 0);

	/* wait for 2 mili sec */
	mdelay(2);
}

/*******************************************************************************
* mv_twsi_addr_set - Set address on TWSI bus.
*
* DESCRIPTION:
*       This function Set address (7 or 10 Bit address) on the Twsi Bus.
*
* INPUT:
*	i2c - i2c controller
*       p_twsi_addr - twsi address.
*	command	 - read / write .
*
* OUTPUT:
*       None.
*
* RETURN:
*       0 - if setting the address completed successfully.
*	-1 otherwmise.
*
*******************************************************************************/
static int mv_twsi_addr_set(Armada38xI2c *i2c,
			    MV_TWSI_ADDR *p_twsi_addr,
			    MV_TWSI_CMD command)
{
	DB(printf(
	    "TWSI: mv_twsi_addr7_bit_set addr %x , type %d, cmd is %s\n",
	    p_twsi_addr->address, p_twsi_addr->type,
	    ((command == MV_TWSI_WRITE) ? "Write" : "Read")));
	/* 10 Bit address */
	if (p_twsi_addr->type == ADDR10_BIT)
		return twsi_addr10_bit_set(i2c, p_twsi_addr->address,
					   command);
	/* 7 Bit address */
	else
		return twsi_addr7_bit_set(i2c, p_twsi_addr->address,
					  command);
}

/*******************************************************************************
* twsi_addr10_bit_set - Set 10 Bit address on TWSI bus.
*
* DESCRIPTION:
*       There are two address phases:
*       1) Write '11110' to data register bits [7:3] and 10-bit address MSB
*          (bits [9:8]) to data register bits [2:1] plus a write(0) or read(1)
*bit
*          to the Data register. Then it clears interrupt flag which drive
*          the address on the TWSI bus. The function then waits for interrupt
*          flag to be active and status 0x18 (write) or 0x40 (read) to be set.
*       2) write the rest of 10-bit address to data register and clears
*          interrupt flag which drive the address on the TWSI bus. The
*          function then waits for interrupt flag to be active and status
*          0xD0 (write) or 0xE0 (read) to be set.
*
* INPUT:
*	i2c - i2c controller
*       device_address - twsi address.
*	command	 - read / write .
*
* OUTPUT:
*       None.
*
* RETURN:
*       0 - if setting the address completed successfully.
*	-1 otherwmise.
*
*******************************************************************************/
static int twsi_addr10_bit_set(Armada38xI2c *i2c,
			       uint32_t device_address,
			       MV_TWSI_CMD command)
{
	uint32_t val, timeout;
	uint8_t bus_num;
	uint32_t base;

	bus_num = i2c->bus_num;
	base = i2c->base;
	/* writing the 2 most significant bits of the 10 bit address */
	val = ((device_address & TWSI_DATA_ADDR_10_BIT_MASK) >>
	       TWSI_DATA_ADDR_10_BIT_OFFS);
	/* bits 7:3 must be 0x11110 */
	val |= TWSI_DATA_ADDR_10_BIT_CONST;
	/* set command */
	val |= command;
	i2c_reg_write(base + TWSI_DATA_REG(bus_num), val);
	/* WA add a delay */
	mdelay(1);

	/* clear Int flag */
	twsi_int_flg_clr(i2c);

	/* wait for Int to be Set */
	timeout = 0;
	while (!twsi_main_int_get(bus_num) && (timeout++ < TWSI_TIMEOUT_VALUE))
		;

	/* check for timeout */
	if (1 ==
	    twsi_timeout_chk(
		timeout, (const char *)"TWSI: addr (10_bit) Int time_out.\n"))
		return -1;

	/* check the status */
	val = twsi_sts_get(i2c);
	if ((TWSI_M_LOST_ARB_DUR_AD_OR_DATA_TRA == val) ||
	    (TWSI_M_LOST_ARB_DUR_AD_TRA_GNL_CALL_AD_REC_ACK_TRA == val)) {
		DB(printf("TWSI: Lost Arb, status %x\n", val));
		return -2;
	} else if (((val != TWSI_AD_PLS_RD_BIT_TRA_ACK_REC) &&
		    (command == MV_TWSI_READ)) ||
		   ((val != TWSI_AD_PLS_WR_BIT_TRA_ACK_REC) &&
		    (command == MV_TWSI_WRITE))) {
		printf("TWSI: status %x 1st addr (10 Bit) in %s mode.\n",
			     val,
			     ((command == MV_TWSI_WRITE) ? "Write" : "Read"));
		return -1;
	}

	/* set  8 LSB of the address */
	val = (device_address << TWSI_DATA_ADDR_7_BIT_OFFS) &
	      TWSI_DATA_ADDR_7_BIT_MASK;
	i2c_reg_write(base + TWSI_DATA_REG(bus_num), val);

	/* clear Int flag */
	twsi_int_flg_clr(i2c);

	/* wait for Int to be Set */
	timeout = 0;
	while (!twsi_main_int_get(bus_num) && (timeout++ < TWSI_TIMEOUT_VALUE))
		;

	/* check for timeout */
	if (1 ==
	    twsi_timeout_chk(timeout,
			     (const char *)"TWSI: 2nd (10 Bit) Int tim_out.\n"))
		return -1;

	/* check the status */
	val = twsi_sts_get(i2c);
	if ((TWSI_M_LOST_ARB_DUR_AD_OR_DATA_TRA == val) ||
	    (TWSI_M_LOST_ARB_DUR_AD_TRA_GNL_CALL_AD_REC_ACK_TRA == val)) {
		DB(printf("TWSI: Lost Arb, status %x\n", val));
		return -2;
	} else if (((val != TWSI_SEC_AD_PLS_RD_BIT_TRA_ACK_REC) &&
		    (command == MV_TWSI_READ)) ||
		   ((val != TWSI_SEC_AD_PLS_WR_BIT_TRA_ACK_REC) &&
		    (command == MV_TWSI_WRITE))) {
		printf("TWSI: status %x 2nd addr(10 Bit) in %s mode.\n",
			     val,
			     ((command == MV_TWSI_WRITE) ? "Write" : "Read"));
		return -1;
	}

	return 0;
}

/*******************************************************************************
* twsi_addr7_bit_set - Set 7 Bit address on TWSI bus.
*
* DESCRIPTION:
*       This function writes 7 bit address plus a write or read bit to the
*       Data register. Then it clears interrupt flag which drive the address on
*       the TWSI bus. The function then waits for interrupt flag to be active
*       and status 0x18 (write) or 0x40 (read) to be set.
*
* INPUT:
*	i2c - i2c controller
*       device_address - twsi address.
*	command	 - read / write .
*
* OUTPUT:
*       None.
*
* RETURN:
*       0 - if setting the address completed successfully.
*	-1 otherwmise.
*
*******************************************************************************/
static int twsi_addr7_bit_set(Armada38xI2c *i2c,
			      uint32_t device_address,
			      MV_TWSI_CMD command)
{
	uint32_t val, timeout;
	uint8_t bus_num;
	uint32_t base;

	bus_num = i2c->bus_num;
	base = i2c->base;
	/* set the address */
	val = (device_address << TWSI_DATA_ADDR_7_BIT_OFFS) &
	      TWSI_DATA_ADDR_7_BIT_MASK;
	/* set command */
	val |= command;
	i2c_reg_write(base + TWSI_DATA_REG(bus_num), val);
	/* WA add a delay */
	mdelay(1);

	/* clear Int flag */
	twsi_int_flg_clr(i2c);

	/* wait for Int to be Set */
	timeout = 0;
	while (!twsi_main_int_get(bus_num) && (timeout++ < TWSI_TIMEOUT_VALUE))
		;

	/* check for timeout */
	if (1 ==
	    twsi_timeout_chk(
		timeout, (const char *)"TWSI: Addr (7 Bit) int time_out.\n"))
		return -1;

	/* check the status */
	val = twsi_sts_get(i2c);
	if ((TWSI_M_LOST_ARB_DUR_AD_OR_DATA_TRA == val) ||
	    (TWSI_M_LOST_ARB_DUR_AD_TRA_GNL_CALL_AD_REC_ACK_TRA == val)) {
		DB(printf("TWSI: Lost Arb, status %x\n", val));
		return -2;
	} else if (((val != TWSI_AD_PLS_RD_BIT_TRA_ACK_REC) &&
		    (command == MV_TWSI_READ)) ||
		   ((val != TWSI_AD_PLS_WR_BIT_TRA_ACK_REC) &&
		    (command == MV_TWSI_WRITE))) {
		/* only in debug, since in boot we try to read the SPD of both
		   DRAM, and we don't
		   want error messeges in case DIMM doesn't exist. */
		DB(printf(
		    "TWSI: status %x addr (7 Bit) in %s mode.\n", val,
		    ((command == MV_TWSI_WRITE) ? "Write" : "Read")));
		return -1;
	}

	return 0;
}

/*******************************************************************************
* twsi_data_write - Trnasmit a data block over TWSI bus.
*
* DESCRIPTION:
*       This function writes a given data block to TWSI bus in 8 bit
*       granularity.
*	first The function waits for interrupt flag to be active then
*       For each 8-bit data:
*        The function writes data to data register. It then clears
*        interrupt flag which drives the data on the TWSI bus.
*        The function then waits for interrupt flag to be active and status
*        0x28 to be set.
*
*
* INPUT:
*	i2c - i2c controller
*       p_block - Data block.
*	block_size - number of chars in p_block.
*
* OUTPUT:
*       None.
*
* RETURN:
*       0 - if transmiting the block completed successfully,
*	-1 otherwmise.
*
*******************************************************************************/
static int twsi_data_transmit(Armada38xI2c *i2c,
			      uint8_t *p_block,
			      uint32_t block_size)
{
	uint32_t timeout, temp, block_size_wr = block_size;
	uint8_t bus_num;
	uint32_t base;

	bus_num = i2c->bus_num;
	base = i2c->base;
	if (NULL == p_block){
		printf("%s ERROR: invalid args\n", __func__);
		return -1;
	}

	/* wait for Int to be Set */
	timeout = 0;
	while (!twsi_main_int_get(bus_num) && (timeout++ < TWSI_TIMEOUT_VALUE))
		;

	/* check for timeout */
	if (1 ==
	    twsi_timeout_chk(timeout,
			     (const char *)"TWSI: Read Data Int time_out.\n"))
		return -1;

	while (block_size_wr) {
		/* write the data */
		i2c_reg_write(base + TWSI_DATA_REG(bus_num),
				(uint32_t)*p_block);
		DB(printf(
		    "TWSI: twsi_data_transmit place = %d write %x\n",
		    block_size - block_size_wr, *p_block));
		p_block++;
		block_size_wr--;

		twsi_int_flg_clr(i2c);

		/* wait for Int to be Set */
		timeout = 0;
		while (!twsi_main_int_get(bus_num) &&
		       (timeout++ < TWSI_TIMEOUT_VALUE))
			;

		/* check for timeout */
		if (1 == twsi_timeout_chk(
				   timeout, (const char *)"TWSI: time_out.\n"))
			return -1;

		/* check the status */
		temp = twsi_sts_get(i2c);
		if ((TWSI_M_LOST_ARB_DUR_AD_OR_DATA_TRA == temp) ||
		    (TWSI_M_LOST_ARB_DUR_AD_TRA_GNL_CALL_AD_REC_ACK_TRA ==
		     temp)) {
			DB(printf("TWSI: Lost Arb, status %x\n", temp));
			return -2;
		} else if (temp != TWSI_M_TRAN_DATA_BYTE_ACK_REC) {
			printf("TWSI: status %x in write trans\n", temp);
			return -1;
		}
	}

	return 0;
}

/*******************************************************************************
* twsi_data_receive - Receive data block from TWSI bus.
*
* DESCRIPTION:
*       This function receive data block from TWSI bus in 8bit granularity
*       into p_block buffer.
*	first The function waits for interrupt flag to be active then
*       For each 8-bit data:
*        It clears the interrupt flag which allows the next data to be
*        received from TWSI bus.
*	 The function waits for interrupt flag to be active,
*	 and status reg is 0x50.
*	 Then the function reads data from data register, and copies it to
*	 the given buffer.
*
* INPUT:
*	i2c - i2c controller
*       block_size - number of bytes to read.
*
* OUTPUT:
*       p_block - Data block.
*
* RETURN:
*       0 - if receive transaction completed successfully,
*	-1 otherwmise.
*
*******************************************************************************/
static int twsi_data_receive(Armada38xI2c *i2c,
			     uint8_t *p_block,
			     uint32_t block_size)
{
	uint32_t timeout, temp, block_size_rd = block_size;
	uint8_t bus_num;
	uint32_t base;

	bus_num = i2c->bus_num;
	base = i2c->base;
	if (NULL == p_block){
		printf("%s ERROR: invalid args\n", __func__);
		return -1;
	}

	/* wait for Int to be Set */
	timeout = 0;
	while (!twsi_main_int_get(bus_num) && (timeout++ < TWSI_TIMEOUT_VALUE))
		;

	/* check for timeout */
	if (1 ==
	    twsi_timeout_chk(timeout,
			     (const char *)"TWSI: Read Data int Time out .\n"))
		return -1;

	while (block_size_rd) {
		if (block_size_rd == 1) {
			/* clear ack and Int flag */
			i2c_reg_bit_reset(base + TWSI_CONTROL_REG(bus_num),
						TWSI_CONTROL_ACK);
		}
		twsi_int_flg_clr(i2c);
		/* wait for Int to be Set */
		timeout = 0;
		while ((!twsi_main_int_get(bus_num)) &&
		       (timeout++ < TWSI_TIMEOUT_VALUE))
			;

		/* check for timeout */
		if (1 ==
		    twsi_timeout_chk(timeout, (const char *)"TWSI: Timeout.\n"))
			return -1;

		/* check the status */
		temp = twsi_sts_get(i2c);
		if ((TWSI_M_LOST_ARB_DUR_AD_OR_DATA_TRA == temp) ||
		    (TWSI_M_LOST_ARB_DUR_AD_TRA_GNL_CALL_AD_REC_ACK_TRA ==
		     temp)) {
			DB(printf("TWSI: Lost Arb, status %x\n", temp));
			return -2;
		} else if ((temp != TWSI_M_REC_RD_DATA_ACK_TRA) &&
			   (block_size_rd != 1)) {
			printf("TWSI: status %x in read trans\n", temp);
			return -1;
		} else if ((temp != TWSI_M_REC_RD_DATA_ACK_NOT_TRA) &&
			   (block_size_rd == 1)) {
			printf("TWSI: status %x in Rd Terminate\n", temp);
			return -1;
		}

		/* read the data */
		*p_block = (uint8_t)i2c_reg_read(base + TWSI_DATA_REG(bus_num));
		DB(printf("TWSI: twsi_data_receive  place %d read %x\n",
				block_size - block_size_rd, *p_block));
		p_block++;
		block_size_rd--;
	}

	return 0;
}

/*******************************************************************************
* twsi_target_offs_set - Set TWST target offset on TWSI bus.
*
* DESCRIPTION:
*       The function support TWSI targets that have inside address space (for
*       example EEPROMs). The function:
*       1) Convert the given offset into p_block and size.
*		in case the offset should be set to a TWSI slave which support
*		more then 256 bytes offset, the offset setting will be done
*		in 2 transactions.
*       2) Use twsi_data_transmit to place those on the bus.
*
* INPUT:
*	i2c - i2c controller
*       offset - offset to be set on the EEPROM device.
*	more_than256 - whether the EEPROM device support more then 256 byte
*offset.
*
* OUTPUT:
*       None.
*
* RETURN:
*       0 - if setting the offset completed successfully.
*	-1 otherwmise.
*
*******************************************************************************/
static int twsi_target_offs_set(Armada38xI2c *i2c,
				uint32_t offset,
				uint8_t more_than256)
{
	uint8_t off_block[2];
	uint32_t off_size;

	if (more_than256 == 1) {
		off_block[0] = (offset >> 8) & 0xff;
		off_block[1] = offset & 0xff;
		off_size = 2;
	} else {
		off_block[0] = offset & 0xff;
		off_size = 1;
	}
	DB(printf(
	    "TWSI: twsi_target_offs_set off_size = %x addr1 = %x addr2 = %x\n",
	    off_size, off_block[0], off_block[1]));
	return twsi_data_transmit(i2c, off_block, off_size);
}

/*******************************************************************************
* mv_twsi_read - Read data block from a TWSI Slave.
*
* DESCRIPTION:
*       The function calls the following functions:
*       -) mv_twsi_start_bit_set();
*	if (EEPROM device)
*	-) mv_twsi_addr_set(w);
*	-) twsi_target_offs_set();
*	-) mv_twsi_start_bit_set();
*	-) mv_twsi_addr_set(r);
*	-) twsi_data_receive();
*	-) mv_twsi_stop_bit_set();
*
* INPUT:
*	i2c - i2c controller
*	p_twsi_slave - Twsi Slave structure.
*	block_size - number of bytes to read.
*
* OUTPUT:
*	p_block - Data block.
*
* RETURN:
*	0 - if EEPROM read transaction completed successfully,
*	-1 otherwmise.
*
*******************************************************************************/
static int mv_twsi_read(Armada38xI2c *i2c,
			MV_TWSI_SLAVE *p_twsi_slave,
			uint8_t *p_block,
			uint32_t block_size)
{
	int rc;
	int ret = -1;
	uint32_t counter = 0;

	if ((NULL == p_block) || (NULL == p_twsi_slave)){
		printf("%s ERROR: invalid args\n", __func__);
		return -1;
	}

	do {
		/* wait for 1 mili sec for the clear to take effect */
		if (counter > 0)
			mdelay(1);
		ret = mv_twsi_start_bit_set(i2c);

		if (-2 == ret)
			continue;
		else if (0 != ret) {
			mv_twsi_stop_bit_set(i2c);
			DB(printf(
			    "mv_twsi_read:mv_twsi_start_bit_set failed\n"));
			return -1;
		}

		DB(printf(
		    "TWSI: mv_twsi_eeprom_read after mv_twsi_start_bit_set\n"));

		/* in case offset exsist (i.e. eeprom ) */
		if (1 == p_twsi_slave->valid_offset) {
			rc = mv_twsi_addr_set(i2c,
					      &(p_twsi_slave->slave_addr),
					      MV_TWSI_WRITE);
			if (-2 == rc)
				continue;
			else if (0 != rc) {
				mv_twsi_stop_bit_set(i2c);
				DB(printf(
				    "mv_twsi_addr_set(%d,0x%x,%d) rc=%d\n",
				    bus_num,
				    (uint32_t) &(p_twsi_slave->slave_addr),
				    MV_TWSI_WRITE, rc));
				return -1;
			}

			ret =
			    twsi_target_offs_set(i2c, p_twsi_slave->offset,
						 p_twsi_slave->more_than256);
			if (-2 == ret)
				continue;
			else if (0 != ret) {
				mv_twsi_stop_bit_set(i2c);
				DB(printf(
				    "TWSI: twsi_target_offs_set Failed\n"));
				return -1;
			}
			DB(printf("TWSI: after twsi_target_offs_set\n"));
			ret = mv_twsi_start_bit_set(i2c);
			if (-2 == ret)
				continue;
			else if (0 != ret) {
				mv_twsi_stop_bit_set(i2c);
				DB(printf(
				    "TWSI: mv_twsi_start_bit_set failed\n"));
				return -1;
			}
			DB(printf("TWSI: after mv_twsi_start_bit_set\n"));
		}
		ret = mv_twsi_addr_set(i2c, &(p_twsi_slave->slave_addr),
				       MV_TWSI_READ);
		if (-2 == ret)
			continue;
		else if (0 != ret) {
			mv_twsi_stop_bit_set(i2c);
			DB(printf(
			    "mv_twsi_read: mv_twsi_addr_set 2 Failed\n"));
			return -1;
		}
		DB(printf(
		    "TWSI: mv_twsi_eeprom_read after mv_twsi_addr_set\n"));

		ret = twsi_data_receive(i2c, p_block, block_size);
		if (-2 == ret)
			continue;
		else if (0 != ret) {
			mv_twsi_stop_bit_set(i2c);
			DB(printf(
			    "mv_twsi_read: twsi_data_receive Failed\n"));
			return -1;
		}
		DB(printf(
		    "TWSI: mv_twsi_eeprom_read after twsi_data_receive\n"));

		ret = mv_twsi_stop_bit_set(i2c);
		if (-2 == ret)
			continue;
		else if (0 != ret) {
			DB(printf(
			    "mv_twsi_read: mv_twsi_stop_bit_set 3 Failed\n"));
			return -1;
		}
		counter++;
	} while ((-2 == ret) && (counter < MAX_RETRY_CNT));

	if (counter == MAX_RETRY_CNT)
		DB(printf("mv_twsi_write: Retry Expire\n"));

	twsi_ack_bit_set(i2c);

	DB(printf(
	    "TWSI: mv_twsi_eeprom_read after mv_twsi_stop_bit_set\n"));

	return 0;
}

/*******************************************************************************
* mv_twsi_write - Write data block to a TWSI Slave.
*
* DESCRIPTION:
*       The function calls the following functions:
*       -) mv_twsi_start_bit_set();
*       -) mv_twsi_addr_set();
*	-)if (EEPROM device)
*	-) twsi_target_offs_set();
*       -) twsi_data_transmit();
*       -) mv_twsi_stop_bit_set();
*
* INPUT:
*	i2c - i2c controller
*	eeprom_address - eeprom address.
*       block_size - number of bytes to write.
*	p_block - Data block.
*
* OUTPUT:
*	None
*
* RETURN:
*       0 - if EEPROM read transaction completed successfully.
*	-1 otherwmise.
*
* NOTE: Part of the EEPROM, required that the offset will be aligned to the
*	max write burst supported.
*******************************************************************************/
static int mv_twsi_write(Armada38xI2c *i2c,
			 MV_TWSI_SLAVE *p_twsi_slave,
			 uint8_t *p_block,
			 uint32_t block_size)
{
	int ret = -1;
	uint32_t counter = 0;

	if ((NULL == p_block) || (NULL == p_twsi_slave)){
		printf("%s ERROR: invalid args\n", __func__);
		return -1;
	}

	do {
		if (counter > 0)
			mdelay(1);
		ret = mv_twsi_start_bit_set(i2c);

		if (-2 == ret)
			continue;
		else if (0 != ret) {
			mv_twsi_stop_bit_set(i2c);
			DB(printf(
			    "mv_twsi_write: mv_twsi_start_bit_set failed\n"));
			return -1;
		}

		ret = mv_twsi_addr_set(i2c, &(p_twsi_slave->slave_addr),
				       MV_TWSI_WRITE);
		if (-2 == ret)
			continue;
		else if (0 != ret) {
			mv_twsi_stop_bit_set(i2c);
			DB(printf(
			    "mv_twsi_write: mv_twsi_addr_set failed\n"));
			return -1;
		}

		/* in case offset exsist (i.e. eeprom ) */
		if (1 == p_twsi_slave->valid_offset) {
			ret =
			    twsi_target_offs_set(i2c, p_twsi_slave->offset,
						 p_twsi_slave->more_than256);
			if (-2 == ret)
				continue;
			else if (0 != ret) {
				mv_twsi_stop_bit_set(i2c);
				DB(printf(
				    "TWSI: twsi_target_offs_set failed\n"));
				return -1;
			}
		}

		ret = twsi_data_transmit(i2c, p_block, block_size);
		if (-2 == ret)
			continue;
		else if (0 != ret) {
			mv_twsi_stop_bit_set(i2c);
			DB(printf(
			    "mv_twsi_write: twsi_data_transmit failed\n"));
			return -1;
		}
		ret = mv_twsi_stop_bit_set(i2c);
		if (-2 == ret)
			continue;
		else if (0 != ret) {
			DB(printf(
			    "mv_twsi_write: failed to set stopbit\n"));
			return -1;
		}
		counter++;
	} while ((-2 == ret) && (counter < MAX_RETRY_CNT));

	if (counter == MAX_RETRY_CNT)
		DB(printf("mv_twsi_write: Retry Expire\n"));

	return 0;
}

static int i2c_init(uint32_t base, uint32_t tclk, uint8_t bus)
{
	MV_TWSI_ADDR slave;

	if (bus >= MAX_I2C_NUM)
		return 1;

	/* TWSI init */
	slave.type = ADDR7_BIT;
	slave.address = 0;
	mv_twsi_init(base, bus, TWSI_SPEED, tclk, &slave, 0);

	return 0;
}

static void i2c_reset(struct I2cOps *me)
{
	Armada38xI2c *bus = container_of(me, Armada38xI2c, ops);

	bus->initialized = 0;
}

static int i2c_transfer(struct I2cOps *me, I2cSeg *segments, int seg_count)
{
	MV_TWSI_SLAVE twsi_slave;
	Armada38xI2c *bus = container_of(me, Armada38xI2c, ops);
	I2cSeg *seg = segments;
	int ret = 0;

	if (!bus->initialized) {
		if (0 != i2c_init(bus->base, bus->tclk, bus->bus_num))
			return 1;
		bus->initialized = 1;
	}

	while (!ret && seg_count--) {
		twsi_slave.slave_addr.address = seg->chip;
		twsi_slave.slave_addr.type = ADDR7_BIT;
		twsi_slave.more_than256 = 0;
		twsi_slave.valid_offset = 0;
		if (seg->read)
			ret = mv_twsi_read(bus, &twsi_slave,
						seg->buf, seg->len);
		else
			ret = mv_twsi_write(bus, &twsi_slave,
						seg->buf, seg->len);
		seg++;
	}

	if (ret) {
		i2c_reset(me);
		return 1;
	}

	return 0;
}

Armada38xI2c *new_armada38x_i2c(u32 base, u32 tclk, u8 bus_num)
{
	Armada38xI2c *bus = 0;

	if (!i2c_init(base, tclk, bus_num)) {
		bus = xzalloc(sizeof(*bus));
		bus->initialized = 1;
		bus->base = base;
		bus->tclk = tclk;
		bus->bus_num = bus_num;
		bus->ops.transfer = &i2c_transfer;
		if (CONFIG_CLI)
			add_i2c_controller_to_list(&bus->ops, "busnum%d",
						   bus_num);
	}
	return bus;
}
