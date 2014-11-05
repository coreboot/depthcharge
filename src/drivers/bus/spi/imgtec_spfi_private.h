/*
 * This file is part of the depthcharge project.
 *
 * Copyright (C) 2014 Imagination Technologies
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


#ifndef __DRIVERS_BUS_SPI_IMGTEC_PRIVATE_H__
#define __DRIVERS_BUS_SPI_IMGTEC_PRIVATE_H__

#define spi_read_reg_field(regval, field)		\
(							\
	((field##_MASK) == 0xFFFFFFFF) ?		\
	(regval) :					\
	(((regval) & (field##_MASK)) >> (field##_SHIFT))\
)

#define spi_write_reg_field(regval, field, val)		\
(							\
	((field##_MASK) == 0xFFFFFFFF) ?		\
	(val) :						\
	(((regval) & ~(field##_MASK)) |			\
	(((val) << (field##_SHIFT)) & (field##_MASK)))	\
)

/*
 * Parameter register
 * Each of these corresponds to a single port (ie CS line) in the interface
 * Fields	Name		Description
 * ======	====		===========
 * b31:24	CLK_RATE	Bit Clock rate = (24.576 * value / 512) MHz
 * b23:16	CS_SETUP	Chip Select setup = (40 * value) ns
 * b15:8	CS_HOLD		Chip Select hold = (40 * value) ns
 * b7:0		CS_DELAY	Chip Select delay = (40 * value) ns
 */

#define SPIM_CLK_DIVIDE_MASK     (0xFF000000)
#define SPIM_CS_SETUP_MASK       (0x00FF0000)
#define SPIM_CS_HOLD_MASK        (0x0000FF00)
#define SPIM_CS_DELAY_MASK       (0x000000FF)
#define SPIM_CS_PARAM_MASK      (SPIM_CS_SETUP_MASK \
				| SPIM_CS_HOLD_MASK \
				| SPIM_CS_DELAY_MASK)

#define SPIM_CLK_DIVIDE_SHIFT            (24)
#define SPIM_CS_SETUP_SHIFT              (16)
#define SPIM_CS_HOLD_SHIFT                (8)
#define SPIM_CS_DELAY_SHIFT               (0)
#define SPIM_CS_PARAM_SHIFT               (0)

/* Control register */

#define SPFI_DRIBBLE_COUNT_MASK  (0x000e0000)
#define SPFI_MEMORY_IF_MASK      (0x00008000)
#define SPIM_BYTE_DELAY_MASK     (0x00004000)
#define SPIM_CS_DEASSERT_MASK    (0x00002000)
#define SPIM_CONTINUE_MASK       (0x00001000)
#define SPIM_SOFT_RESET_MASK     (0x00000800)
#define SPIM_SEND_DMA_MASK       (0x00000400)
#define SPIM_GET_DMA_MASK        (0x00000200)
#define SPIM_EDGE_TX_RX_MASK     (0x00000100)
#define SPFI_TRNSFR_MODE_MASK    (0x000000e0)
#define SPFI_TRNSFR_MODE_DQ_MASK (0x0000001c)
#define SPFI_TX_RX_MASK          (0x00000002)
#define SPFI_EN_MASK             (0x00000001)

#define SPFI_DRIBBLE_COUNT_SHIFT         (17)
#define SPFI_MEMORY_IF_SHIFT             (15)
#define SPIM_BYTE_DELAY_SHIFT            (14)
#define SPIM_CS_DEASSERT_SHIFT           (13)
#define SPIM_CONTINUE_SHIFT              (12)
#define SPIM_SOFT_RESET_SHIFT            (11)
#define SPIM_SEND_DMA_SHIFT              (10)
#define SPIM_GET_DMA_SHIFT                (9)
#define SPIM_EDGE_TX_RX_SHIFT             (8)
#define SPFI_TRNSFR_MODE_SHIFT            (5)
#define SPFI_TRNSFR_MODE_DQ_SHIFT         (2)
#define SPFI_TX_RX_SHIFT                  (1)
#define SPFI_EN_SHIFT                     (0)

/* Transaction register*/

#define SPFI_TSIZE_MASK           (0xffff0000)
#define SPFI_CMD_LENGTH_MASK      (0x0000e000)
#define SPFI_ADDR_LENGTH_MASK     (0x00001c00)
#define SPFI_DUMMY_LENGTH_MASK    (0x000003e0)
#define SPFI_PI_LENGTH_MASK       (0x0000001c)

#define SPFI_TSIZE_SHIFT                  (16)
#define SPFI_CMD_LENGTH_SHIFT             (13)
#define SPFI_ADDR_LENGTH_SHIFT            (10)
#define SPFI_DUMMY_LENGTH_SHIFT            (5)
#define SPFI_PI_LENGTH_SHIFT               (2)

/* Port state register */

#define SPFI_PORT_SELECT_MASK    (0x00700000)
/* WARNING the following bits are reversed */
#define SPFI_CLOCK0_IDLE_MASK    (0x000f8000)
#define SPFI_CLOCK0_PHASE_MASK   (0x00007c00)
#define SPFI_CS0_IDLE_MASK       (0x000003e0)
#define SPFI_DATA0_IDLE_MASK     (0x0000001f)

#define SPIM_CLOCK0_IDLE_MASK    (0x000f8000)
#define SPIM_CLOCK0_PHASE_MASK   (0x00007c00)
#define SPIM_CS0_IDLE_MASK       (0x000003e0)
#define SPIM_DATA0_IDLE_MASK     (0x0000001f)

#define SPIM_PORT0_MASK          (0x00084210)

#define SPFI_PORT_SELECT_SHIFT           (20)
/* WARNING the following bits are reversed, bit 0 is highest */
#define SPFI_CLOCK0_IDLE_SHIFT           (19)
#define SPFI_CLOCK0_PHASE_SHIFT          (14)
#define SPFI_CS0_IDLE_SHIFT               (9)
#define SPFI_DATA0_IDLE_SHIFT             (4)

#define SPIM_CLOCK0_IDLE_SHIFT           (19)
#define SPIM_CLOCK0_PHASE_SHIFT          (14)
#define SPIM_CS0_IDLE_SHIFT               (9)
#define SPIM_DATA0_IDLE_SHIFT             (4)


/*
 * Interrupt registers
 * SPFI_GDOF_MASK means Rx buffer full, not an overflow, because clock stalls
 * SPFI_SDUF_MASK means Tx buffer empty, not an underflow, because clock stalls
 */
#define SPFI_IACCESS_MASK        (0x00001000)
#define SPFI_GDEX8BIT_MASK       (0x00000800)
#define SPFI_ALLDONE_MASK        (0x00000200)
#define SPFI_GDFUL_MASK          (0x00000100)
#define SPFI_GDHF_MASK           (0x00000080)
#define SPFI_GDEX32BIT_MASK      (0x00000040)
#define SPFI_GDTRIG_MASK         (0x00000020)
#define SPFI_SDFUL_MASK          (0x00000008)
#define SPFI_SDHF_MASK           (0x00000004)
#define SPFI_SDE_MASK            (0x00000002)
#define SPFI_SDTRIG_MASK         (0x00000001)

#define SPFI_IACCESS_SHIFT               (12)
#define SPFI_GDEX8BIT_SHIFT              (11)
#define SPFI_ALLDONE_SHIFT                (9)
#define SPFI_GDFUL_SHIFT                  (8)
#define SPFI_GDHF_SHIFT                   (7)
#define SPFI_GDEX32BIT_SHIFT              (6)
#define SPFI_GDTRIG_SHIFT                 (5)
#define SPFI_SDFUL_SHIFT                  (3)
#define SPFI_SDHF_SHIFT                   (2)
#define SPFI_SDE_SHIFT                    (1)
#define SPFI_SDTRIG_SHIFT                 (0)


/* SPFI register block */

#define SPFI_PORT_0_PARAM_REG_OFFSET             (0x00)
#define SPFI_PORT_1_PARAM_REG_OFFSET             (0x04)
#define SPFI_PORT_2_PARAM_REG_OFFSET             (0x08)
#define SPFI_PORT_3_PARAM_REG_OFFSET             (0x0C)
#define SPFI_PORT_4_PARAM_REG_OFFSET             (0x10)
#define SPFI_CONTROL_REG_OFFSET                  (0x14)
#define SPFI_TRANSACTION_REG_OFFSET              (0x18)
#define SPFI_PORT_STATE_REG_OFFSET               (0x1C)

#define SPFI_SEND_LONG_REG_OFFSET                (0x20)
#define SPFI_SEND_BYTE_REG_OFFSET                (0x24)
#define SPFI_GET_LONG_REG_OFFSET                 (0x28)
#define SPFI_GET_BYTE_REG_OFFSET                 (0x2C)

#define SPFI_INT_STATUS_REG_OFFSET               (0x30)
#define SPFI_INT_ENABLE_REG_OFFSET               (0x34)
#define SPFI_INT_CLEAR_REG_OFFSET                (0x38)

#define SPFI_IMMEDIATE_STATUS_REG_OFFSET         (0x3c)

#define SPFI_FLASH_BASE_ADDRESS_REG_OFFSET       (0x48)
#define SPFI_FLASH_STATUS_REG_OFFSET             (0x4C)

#define IMG_FALSE				0
#define IMG_TRUE				1

/* Number of SPIM interfaces*/
#define SPIM_NUM_BLOCKS				2
/* Number of chip select lines supported by the SPI master port. */
#define SPIM_NUM_PORTS_PER_BLOCK		(SPIM_DUMMY_CS)
/* Maximum transfer size (in bytes) for the SPI master port. */
#define SPIM_MAX_TRANSFER_BYTES			(0xFFFF)
/* Maximum size of a flash command: command bytes+address_bytes. */
#define SPIM_MAX_FLASH_COMMAND_BYTES		(0x8)
/* Write operation to fifo done in blocks of 16 words (64 bytes) */
#define SPIM_MAX_BLOCK_BYTES			(0x40)
/* Microseconds until timeout is returned */
#define SPI_TIMEOUT_VALUE_US			500000

/* This type defines the SPIM device numbers (chip select lines). */
enum spim_device {
	/* Device 0 (CS0). */
	SPIM_DEVICE0 = 0,
	/* Device 1 (CS1). */
	SPIM_DEVICE1,
	/* Device 2 (CS2). */
	SPIM_DEVICE2,
	/* Device 3 (CS3). */
	SPIM_DEVICE3,
	/* Device 4 (CS4). */
	SPIM_DEVICE4,
	/* Dummy chip select. */
	SPIM_DUMMY_CS

};

/* Command transfer mode */
enum command_mode {
	/* Command, address, dummy and PI cycles are transferred on sio0 */
	SPIM_CMD_MODE_0 = 0,
	/*
	 * Command and Address is transferred on sio0 port only but dummy
	 * cycles and PI is transferred on all the interface ports.
	 */
	SPIM_CMD_MODE_1,
	/*
	 * Command is transferred on sio0 port only but address, dummy
	 * and PI is transferred on all the interface portS
	 */
	SPIM_CMD_MODE_2,
	/*
	 * Command, address, dummy and PI bytes are transferred on all
	 * the interfaces
	 */
	SPIM_CMD_MODE_3
};

/* Data transfer mode */
enum transfer_mode {
	/* Transfer data in single mode */
	SPIM_DMODE_SINGLE = 0,
	/* Transfer data in dual mode */
	SPIM_DMODE_DUAL,
	/* Transfer data in quad mode */
	SPIM_DMODE_QUAD
};

#endif /* __DRIVERS_BUS_SPI_IMGTEC_PRIVATE_H__ */
