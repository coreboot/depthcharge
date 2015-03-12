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

#include <libpayload.h>
#include <arch/cpu.h>
#include "base/container_of.h"
#include "drivers/bus/spi/imgtec_spfi.h"
#include "drivers/bus/spi/imgtec_spfi_private.h"

/* Sets port parameters in port state register. */
static void  setparams(SpiOps *spi_ops, u32 port,
			struct spim_device_parameters *params)
{
	u32 spim_parameters = 0, port_state;
	u32 base = container_of(spi_ops, ImgSpi, ops)->base;

	port_state = read32(base + SPFI_PORT_STATE_REG_OFFSET);
	port_state &= ~((SPIM_PORT0_MASK>>port)|SPFI_PORT_SELECT_MASK);
	port_state |= params->cs_idle_level<<(SPIM_CS0_IDLE_SHIFT-port);
	port_state |=
		params->data_idle_level<<(SPIM_DATA0_IDLE_SHIFT-port);

	/* Clock idle level and phase */
	switch (params->spi_mode) {
	case SPIM_MODE_0:
		break;
	case SPIM_MODE_1:
		port_state |= (1 << (SPIM_CLOCK0_PHASE_SHIFT - port));
		break;
	case SPIM_MODE_2:
		port_state |= (1 << (SPIM_CLOCK0_IDLE_SHIFT - port));
		break;
	case SPIM_MODE_3:
		port_state |= (1 << (SPIM_CLOCK0_IDLE_SHIFT - port)) |
				 (1 << (SPIM_CLOCK0_PHASE_SHIFT - port));
		break;
	}
	/* Set port state register */
	write32(port_state, base + SPFI_PORT_STATE_REG_OFFSET);

	/* Set up values to be written to device parameter register */
	spim_parameters |= params->bitrate << SPIM_CLK_DIVIDE_SHIFT;
	spim_parameters |= params->cs_setup << SPIM_CS_SETUP_SHIFT;
	spim_parameters |= params->cs_hold << SPIM_CS_HOLD_SHIFT;
	spim_parameters |= params->cs_delay << SPIM_CS_DELAY_SHIFT;

	write32(spim_parameters,
		base + SPFI_PORT_0_PARAM_REG_OFFSET + 4 * port);
}

/* Checks the set bitrate */
static int check_bitrate(u32 rate)
{
	/* Bitrate must be 1, 2, 4, 8, 16, 32, 64, or 128 */
	switch (rate) {
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
	case 128:
		return SPIM_OK;
	default:
		return -SPIM_INVALID_BIT_RATE;
	}
	return -SPIM_INVALID_BIT_RATE;
}

/* Checks device parameters for errors */
static int check_device_params(struct spim_device_parameters *pdev_param)
{
	if (pdev_param->spi_mode > SPIM_MODE_3)
		return -SPIM_INVALID_SPI_MODE;
	if (check_bitrate(pdev_param->bitrate) != SPIM_OK)
		return -SPIM_INVALID_BIT_RATE;
	if (pdev_param->cs_idle_level > 1)
		return -SPIM_INVALID_CS_IDLE_LEVEL;
	if (pdev_param->data_idle_level > 1)
		return -SPIM_INVALID_DATA_IDLE_LEVEL;
	return SPIM_OK;
}

/* Function that carries out read/write operations */
static int spim_io(SpiOps *spi_ops, void *din, const void *dout,
				unsigned int bytes)
{
	unsigned int tx, rx;
	int both_hf, either_hf, incoming;
	u32 reg, status = 0, data;
	u64 start_timer;
	u32 base = container_of(spi_ops, ImgSpi, ops)->base;

	/* Set transaction register */
	reg = spi_write_reg_field(0, SPFI_TSIZE, bytes);
	write32(reg, base + SPFI_TRANSACTION_REG_OFFSET);
	/* Clear status */
	write32(0xffffffff, base + SPFI_INT_CLEAR_REG_OFFSET);

	/* Set control register */
	reg = read32(base + SPFI_CONTROL_REG_OFFSET);
	/* Enable SPFI */
	reg |= spi_write_reg_field(reg, SPFI_EN, 1);
	/* Set send DMA if write transaction exists */
	reg = spi_write_reg_field(reg, SPIM_SEND_DMA,
			(dout) ? 1 : 0);
	/* Set get DMA if read transaction exists */
	reg = spi_write_reg_field(reg, SPIM_GET_DMA,
			(din) ? 1 : 0);
	/* Same edge used for higher operational frequency */
	reg = spi_write_reg_field(reg, SPIM_EDGE_TX_RX, 1);

	write32(reg, base + SPFI_CONTROL_REG_OFFSET);

	tx = (dout) ? 0 : bytes;
	rx = (din) ? 0 : bytes;
	start_timer = timer_us(0);

	/* We send/receive as long as we still have data available */
	while ((tx < bytes) || (rx < bytes)) {

		/* If we exceed the allocated time we return with timeout */
		if (timer_us(start_timer) > SPI_TIMEOUT_VALUE_US)
			return -SPIM_TIMEOUT_ERROR;

		/* Store the current state of the INT status register */
		status = read32(base + SPFI_INT_STATUS_REG_OFFSET);

		/*
		 * Clear all statuses indicating the state of the FIFOs
		 * The stored status will be used in this iteration
		 */
		write32(SPFI_SDFUL_MASK | SPFI_SDHF_MASK | SPFI_GDHF_MASK,
			base + SPFI_INT_CLEAR_REG_OFFSET);

		/*
		 * Top up TX unless both RX and TX are half full,
		 * in which case RX needs draining more urgently
		 */
		both_hf = ((status & (SPFI_GDHF_MASK | SPFI_SDHF_MASK))
			== (SPFI_GDHF_MASK | SPFI_SDHF_MASK));
		if ((tx < bytes) && !(status & SPFI_SDFUL_MASK) &&
			(!(rx < bytes) || !both_hf)) {
			if ((bytes - tx) >= sizeof(u32)) {
				memcpy(&data, (u8 *)dout + tx, sizeof(u32));
				write32(data, base + SPFI_SEND_LONG_REG_OFFSET);
				tx += sizeof(u32);
			} else {
				data = *((u8 *)dout + tx);
				write32(data, base + SPFI_SEND_BYTE_REG_OFFSET);
				tx++;
			}
		}
		/*
		 * Drain RX unless neither RX or TX are half full,
		 * in which case TX needs filling more urgently.
		 */
		either_hf = (status & (SPFI_SDHF_MASK |
						SPFI_GDHF_MASK));
		incoming = (status & (SPFI_GDEX8BIT_MASK |
					SPFI_GDEX32BIT_MASK));
		if ((rx < bytes) && incoming && (!(tx < bytes) || either_hf)) {
			if (((bytes - rx) >= sizeof(u32)) &&
				(status & SPFI_GDEX32BIT_MASK)) {
				data = read32(base +
						SPFI_GET_LONG_REG_OFFSET);
				memcpy((u8 *)din + rx, &data, sizeof(u32));
				rx += sizeof(u32);
				write32(SPFI_GDEX32BIT_MASK,
					base + SPFI_INT_CLEAR_REG_OFFSET);
			} else if (status & SPFI_GDEX8BIT_MASK) {
				data  = (u8)read32(base +
					SPFI_GET_BYTE_REG_OFFSET);
				*((u8 *)din + rx) = data;
				rx++;
				write32(SPFI_GDEX8BIT_MASK, base +
					SPFI_INT_CLEAR_REG_OFFSET);
			}
		}

	}

	/*
	 * We wait for ALLDONE trigger to be set, signaling that it's ready
	 * to accept another transaction
	 */
	start_timer = timer_us(0);

	do {
		if (timer_us(start_timer) > SPI_TIMEOUT_VALUE_US)
			return -SPIM_TIMEOUT_ERROR;
		status = read32(base + SPFI_INT_STATUS_REG_OFFSET);
	} while (!(status & SPFI_ALLDONE_MASK));
	write32(SPFI_ALLDONE_MASK, base + SPFI_INT_CLEAR_REG_OFFSET);
	return SPIM_OK;
}

static void cs_change(SpiOps *spi_ops, int enable) {
	ImgSpi *bus = container_of(spi_ops, ImgSpi, ops);
	GpioOps *gpio_ops = bus->img_gpio;
	int cs_num = bus->cs;

	if (IMG_PLATFORM_ID() != IMG_PLATFORM_ID_SILICON) {
		/*
		 * Special register on FPGA to assert de-assert CS (0xB814908C),
		 * only implemented for SPFI interface 1, CS lines 0 and 1
		 */
		if (cs_num <=1) {
			write32((1 << (cs_num*8)), 0xB814908C);
			write32(((enable? 1 : 0) << ((cs_num*8) + 4)),
					0xB814908C);
		} else {
			printf("%s: Error: Unsuppored control of CS line.\n",
				__func__);
		}
	} else {
		gpio_ops->set(gpio_ops, !((unsigned int)enable));
	}
}


/* Claim the bus and prepare it for communication */
static int spi_claim_bus(SpiOps *spi_ops)
{
	int ret;
	ImgSpi *bus;
	u32 reg;

	if (!spi_ops) {
		printf("%s: Error: SpiOps not initialized.\n", __func__);
		return -SPIM_API_NOT_INITIALISED;
	}
	bus = container_of(spi_ops, ImgSpi, ops);
	if (bus->initialised)
		return SPIM_OK;

	/* Soft reset peripheral internals */
	write32(SPIM_SOFT_RESET_MASK, bus->base + SPFI_CONTROL_REG_OFFSET);
	/* Clear control register */
	write32(0, bus->base + SPFI_CONTROL_REG_OFFSET);

	/* Check device parameters */
	ret = check_device_params(&(bus->device_parameters));
	if (ret) {
		printf("%s: Error: incorrect device parameters.\n", __func__);
		return ret;
	}
	/* Set device parameters */
	setparams(spi_ops, bus->cs, &(bus->device_parameters));

	/* Port state register setup */
	reg = read32(bus->base +  SPFI_PORT_STATE_REG_OFFSET);
	reg = spi_write_reg_field(reg, SPFI_PORT_SELECT, bus->cs);
	write32(reg, bus->base + SPFI_PORT_STATE_REG_OFFSET);

	/* Set the control register */
	reg = (bus->device_parameters).inter_byte_delay ?
			SPIM_BYTE_DELAY_MASK : 0;
	/* Set up the transfer mode */
	reg = spi_write_reg_field(reg, SPFI_TRNSFR_MODE_DQ, SPIM_CMD_MODE_0);
	reg = spi_write_reg_field(reg, SPFI_TRNSFR_MODE, SPIM_DMODE_SINGLE);
	write32(reg, bus->base + SPFI_CONTROL_REG_OFFSET);
	bus->initialised = IMG_TRUE;

	/* Assert CS */
	cs_change(spi_ops, 1);

	return SPIM_OK;
}

/* Release the SPI bus */
static int spi_release_bus(SpiOps *spi_ops)
{
	ImgSpi *bus;

	if (!spi_ops) {
		printf("%s: Error: SpiOps not initialized.\n", __func__);
		return -1;
	}

	bus = container_of(spi_ops, ImgSpi, ops);
	bus->initialised = IMG_FALSE;

	/* De-assert CS */
	cs_change(spi_ops, 0);
	/* Soft reset peripheral internals */
	write32(SPIM_SOFT_RESET_MASK, bus->base +
		SPFI_CONTROL_REG_OFFSET);
	write32(0, bus->base + SPFI_CONTROL_REG_OFFSET);

	return SPIM_OK;
}

/* SPI transfer */
static int spi_xfer(SpiOps *spi_ops, void *din, const void *dout,
				unsigned int bytes)
{
	unsigned int to_transfer, bytes_trans = 0;
	int ret;

	if (!spi_ops) {
		printf("%s: Error: SpiOps not initialized.\n", __func__);
		return -SPIM_API_NOT_INITIALISED;
	}
	if (!dout && !din) {
		printf("%s: Error: both buffers are NULL.\n", __func__);
		return -SPIM_INVALID_TRANSFER_DESC;
	}

	/*
	 * Transfers with size bigger than
	 * SPIM_MAX_TANSFER_BYTES = 64KB - 1 (0xFFFF) are divided.
	 */
	while (bytes - bytes_trans) {
		if ((bytes - bytes_trans) > SPIM_MAX_TRANSFER_BYTES)
			to_transfer = SPIM_MAX_TRANSFER_BYTES;
		else
			to_transfer = bytes - bytes_trans;
		ret = spim_io(spi_ops, (din) ? (din + bytes_trans) : NULL,
					(dout) ? (dout + bytes_trans) : NULL,
					to_transfer);
		if (ret) {
			printf("%s: Error: Transfer failed!\n", __func__);
			return ret;
		}
		bytes_trans += to_transfer;
	}

	return SPIM_OK;
}

/* Initialize SPI slave */
ImgSpi *new_imgtec_spi(unsigned int port, unsigned int cs, GpioOps *img_gpio)
{
	ImgSpi *bus = NULL;
	struct spim_device_parameters *device_parameters;

	die_if(cs > SPIM_DEVICE4, "Error: unsupported chipselect.\n");

	bus = xzalloc(sizeof(*bus));
	bus->base = IMG_SPIM_BASE_ADDRESS(port);
	bus->cs = cs;

	device_parameters = &(bus->device_parameters);
	device_parameters->bitrate = 64;
	device_parameters->cs_setup = 0;
	device_parameters->cs_hold = 0;
	device_parameters->cs_delay = 0;
	device_parameters->spi_mode = SPIM_MODE_0;
	device_parameters->cs_idle_level = 1;
	device_parameters->data_idle_level = 0;

	bus->initialised = IMG_FALSE;

	bus->ops.start = &spi_claim_bus;
	bus->ops.stop = &spi_release_bus;
	bus->ops.transfer = &spi_xfer;

	bus->img_gpio = img_gpio;

	return bus;
}
