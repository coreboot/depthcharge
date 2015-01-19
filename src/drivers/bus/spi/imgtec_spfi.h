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

#ifndef __DRIVERS_BUS_SPI_IMGTEC_H__
#define __DRIVERS_BUS_SPI_IMGTEC_H__

#include "drivers/bus/spi/spi.h"
#include "drivers/gpio/gpio.h"
#include <arch/types.h>

/* SPIM initialisation function return value.*/
enum spim_return {
	/* Initialisation parameters are valid. */
	SPIM_OK = 0,
	/* Mode parameter is invalid. */
	SPIM_INVALID_SPI_MODE,
	/* Chip select idle level is invalid. */
	SPIM_INVALID_CS_IDLE_LEVEL,
	/* Data idle level is invalid. */
	SPIM_INVALID_DATA_IDLE_LEVEL,
	/* Chip select line parameter is invalid. */
	SPIM_INVALID_CS_LINE,
	/* Transfer size parameter is invalid. */
	SPIM_INVALID_SIZE,
	/* Read/write parameter is invalid. */
	SPIM_INVALID_READ_WRITE,
	/* Continue parameter is invalid. */
	SPIM_INVALID_CONTINUE,
	/* Invalid block index */
	SPIM_INVALID_BLOCK_INDEX,
	/* Extended error values */
	/* Invalid bit rate */
	SPIM_INVALID_BIT_RATE,
	/* Invalid CS hold value */
	SPIM_INVALID_CS_HOLD_VALUE,
	/* API function called before API is initialised */
	SPIM_API_NOT_INITIALISED,
	/* SPI driver initialisation failed */
	SPIM_DRIVER_INIT_ERROR,
	/* Invalid transfer description */
	SPIM_INVALID_TRANSFER_DESC,
	/* Timeout indicating an error on the bus*/
	SPIM_TIMEOUT_ERROR,
	/* Timeout indicating that a transaction
	 * is not finalized and the bus should be
	 * released in order to achieve this.
	 */
	SPIM_TIMEOUT_RELEASE

};

/* This type defines the SPI Mode.*/
enum spim_mode {
	/* Mode 0 (clock idle low, data valid on first clock transition). */
	SPIM_MODE_0 = 0,
	/* Mode 1 (clock idle low, data valid on second clock transition). */
	SPIM_MODE_1,
	/* Mode 2 (clock idle high, data valid on first clock transition). */
	SPIM_MODE_2,
	/* Mode 3 (clock idle high, data valid on second clock transition). */
	SPIM_MODE_3

};

/* This structure defines communication parameters for a slave device */
struct spim_device_parameters {
	/* Bit rate value.*/
	unsigned char bitrate;
	/*
	 * Chip select set up time.
	 * Time taken between chip select going active and activity occurring
	 * on the clock, calculated by dividing the desired set up time in ns
	 * by the Input clock period. (setup time / Input clock freq)
	 */
	unsigned char cs_setup;
	/*
	 * Chip select hold time.
	 * Time after the last clock pulse before chip select goes inactive,
	 * calculated by dividing the desired hold time in ns by the
	 * Input clock period (hold time / Input clock freq).
	 */
	unsigned char cs_hold;
	/*
	 * Chip select delay time (CS minimum inactive time).
	 * Minimum time after chip select goes inactive before chip select
	 * can go active again, calculated by dividing the desired delay time
	 * in ns by the Input clock period (delay time / Input clock freq).
	 */
	unsigned char cs_delay;
	/*
	 * Byte delay select:
	 * Selects whether or not a delay is inserted between bytes.
	 * 0 - Minimum inter-byte delay
	 * 1 - Inter-byte delay of
	 * (cs_hold/master_clk half period) * master_clk
	 */
	int inter_byte_delay;
	/* SPI Mode. */
	enum spim_mode spi_mode;
	/* Chip select idle level (0=low, 1=high, Others=invalid). */
	unsigned int cs_idle_level;
	/* Data idle level (0=low, 1=high, Others=invalid). */
	unsigned int data_idle_level;

};

typedef struct ImgSpi {
	/* Generic ineterface operations structure */
	SpiOps ops;
	/* SPIM instance device parameters */
	struct spim_device_parameters device_parameters;
	/* SPIM instance base address */
	u32 base;
	/* Chip select */
	unsigned int cs;
	/* Boolean property that is TRUE if API has been initialised */
	int initialised;
	/* GPIO instance */
	GpioOps *img_gpio;
} ImgSpi;

ImgSpi *new_imgtec_spi(uintptr_t reg_addr, unsigned int cs,
					GpioOps *img_gpio);

#endif /* __DRIVERS_BUS_SPI_IMGTEC_H__ */
