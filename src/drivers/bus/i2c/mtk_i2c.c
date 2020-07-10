/*
 * Copyright 2015 Mediatek Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "base/container_of.h"
#include "drivers/bus/i2c/mtk_i2c.h"

#define MAX_TRANSFER_LEN 255

#define I2CTAG                "[I2C][LK] "

#if DEBUG_I2C
#define I2CLOG(fmt, arg...)   printf(I2CTAG fmt, ##arg)
#else
#define I2CLOG(fmt, arg...)
#endif /* CONFIG_DEBUG_I2C */

#define I2CERR(fmt, arg...)   printf(I2CTAG fmt, ##arg)

static inline void i2c_dma_reset(struct mtk_i2c_dma_regs *dma_regs)
{
	write32(&dma_regs->dma_rst, 0x1);
	udelay(50);
	write32(&dma_regs->dma_rst, 0x2);
	udelay(50);
	write32(&dma_regs->dma_rst, 0x0);
	udelay(50);
}

static inline void mtk_i2c_dump_info(MTKI2c *bus)
{
#if DEBUG_I2C
	struct mtk_i2c_regs *regs = bus->base;

	I2CLOG("I2C register:\nSLAVE_ADDR %x\nINTR_MASK %x\nINTR_STAT %x\n"
	       "CONTROL %x\nTRANSFER_LEN %x\nTRANSAC_LEN %x\nDELAY_LEN %x\n"
	       "TIMING %x\nSTART %x\nFIFO_STAT %x\nIO_CONFIG %x\nHS %x\n"
	       "DEBUGSTAT %x\nEXT_CONF %x\n",
		(read32(&regs->slave_addr)),
		(read32(&regs->intr_mask)),
		(read32(&regs->intr_stat)),
		(read32(&regs->control)),
		(read32(&regs->transfer_len)),
		(read32(&regs->transac_len)),
		(read32(&regs->delay_len)),
		(read32(&regs->timing)),
		(read32(&regs->start)),
		(read32(&regs->fifo_stat)),
		(read32(&regs->io_config)),
		(read32(&regs->hs)),
		(read32(&regs->debug_stat)),
		(read32(&regs->ext_conf)));
#endif
}

static uint32_t mtk_i2c_transfer(MTKI2c *bus, I2cSeg *seg, enum i2c_modes read)
{
	uint32_t ret_code = I2C_OK;
	uint16_t status;
	uint32_t time_out_val = 0;
	uint8_t  addr;
	uint32_t write_len = 0;
	uint32_t read_len = 0;
	uint8_t *write_buffer = NULL;
	uint8_t *read_buffer = NULL;
	uint64_t start;
	struct mtk_i2c_regs *regs;
	struct mtk_i2c_dma_regs *dma_regs;

	regs = bus->base;
	dma_regs = bus->dma_base;
	write_buffer = bus->write_buffer;
	read_buffer = bus->read_buffer;

	addr = seg[0].chip;

	switch (read) {
	case I2C_WRITE_MODE:
		assert(seg[0].len > 0 && seg[0].len <= MAX_TRANSFER_LEN);
		write_len = seg[0].len;
		break;

	case I2C_READ_MODE:
		assert(seg[0].len > 0 && seg[0].len <= MAX_TRANSFER_LEN);
		read_len = seg[0].len;
		break;

	/* Must use special write-then-read mode for repeated starts. */
	case I2C_WRITE_READ_MODE:
		assert(seg[0].len > 0 && seg[0].len <= MAX_TRANSFER_LEN);
		assert(seg[1].len > 0 && seg[1].len <= MAX_TRANSFER_LEN);
		write_len = seg[0].len;
		read_len = seg[1].len;
		break;
	}

	/* Clear interrupt status */
	write32(&regs->intr_stat, I2C_TRANSAC_COMP | I2C_ACKERR |
		I2C_HS_NACKERR);

	write32(&regs->fifo_addr_clr, 0x1);

	/* Enable interrupt */
	write32(&regs->intr_mask, I2C_HS_NACKERR | I2C_ACKERR |
		I2C_TRANSAC_COMP);

	switch (read) {
	case I2C_WRITE_MODE:
		memcpy(write_buffer, seg[0].buf, write_len);
		/* control registers */
		write32(&regs->control, ACK_ERR_DET_EN | DMA_EN | CLK_EXT |
			REPEATED_START_FLAG);

		/* Set transfer and transaction len */
		write32(&regs->transac_len, 1);
		write32(&regs->transfer_len, write_len);

		/* set i2c write slave address*/
		write32(&regs->slave_addr, addr << 1);

		/* Prepare buffer data to start transfer */
		write32(&dma_regs->dma_con, I2C_DMA_CON_TX);
		write32(&dma_regs->dma_tx_mem_addr, (uintptr_t)write_buffer);
		write32(&dma_regs->dma_tx_len, write_len);
		break;

	case I2C_READ_MODE:
		/* control registers */
		write32(&regs->control, ACK_ERR_DET_EN | DMA_EN | CLK_EXT |
			REPEATED_START_FLAG);

		/* Set transfer and transaction len */
		write32(&regs->transac_len, 1);
		write32(&regs->transfer_len, read_len);

		/* set i2c read slave address*/
		write32(&regs->slave_addr, (addr << 1 | 0x1));

		/* Prepare buffer data to start transfer */
		write32(&dma_regs->dma_con, I2C_DMA_CON_RX);
		write32(&dma_regs->dma_rx_mem_addr, (uintptr_t)read_buffer);
		write32(&dma_regs->dma_rx_len, read_len);
		break;

	case I2C_WRITE_READ_MODE:
		memcpy(write_buffer, seg[0].buf, write_len);
		/* control registers */
		write32(&regs->control, DIR_CHG | ACK_ERR_DET_EN | DMA_EN |
			CLK_EXT | REPEATED_START_FLAG);

		/* Set transfer and transaction len */
		write32(&regs->transfer_len, write_len);
		write32(&regs->transfer_aux_len, read_len);
		write32(&regs->transac_len, 2);

		/* set i2c write slave address*/
		write32(&regs->slave_addr, addr << 1);

		/* Prepare buffer data to start transfer */
		write32(&dma_regs->dma_con, I2C_DMA_CLR_FLAG);
		write32(&dma_regs->dma_tx_mem_addr, (uintptr_t)write_buffer);
		write32(&dma_regs->dma_tx_len, write_len);
		write32(&dma_regs->dma_rx_mem_addr, (uintptr_t)read_buffer);
		write32(&dma_regs->dma_rx_len, read_len);
		break;
	}

	write32(&dma_regs->dma_int_flag, I2C_DMA_CLR_FLAG);
	write32(&dma_regs->dma_en, I2C_DMA_START_EN);

	/* start transfer transaction */
	write32(&regs->start, 0x1);

	start = timer_us(0);

	/* polling mode : see if transaction complete */
	while (1) {
		status = read32(&regs->intr_stat);
		if (status & I2C_HS_NACKERR) {
			ret_code = I2C_TRANSFER_FAIL_HS_NACKERR;
			I2CERR("[i2c:%p transfer] transaction NACK error\n",
			       bus->base);
			break;
		} else if (status & I2C_ACKERR) {
			ret_code = I2C_TRANSFER_FAIL_ACKERR;
			I2CERR("[i2c:%p transfer] transaction ACK error\n",
			       bus->base);
			break;
		} else if (status & I2C_TRANSAC_COMP) {
			ret_code = I2C_OK;
			if (read == I2C_READ_MODE) {
				memcpy(seg[0].buf, read_buffer, read_len);
			} else if (read == I2C_WRITE_READ_MODE) {
				memcpy(seg[1].buf, read_buffer, read_len);
			}
			break;
		}

		if (timer_us(start) > I2C_TRANSFER_TIMEOUT_US) {
			ret_code = I2C_TRANSFER_FAIL_TIMEOUT;
			I2CERR("[i2c:%p transfer] transaction timeout:%d\n",
			       bus->base, time_out_val);
			break;
		}
	}

	write32(&regs->intr_stat, I2C_TRANSAC_COMP | I2C_ACKERR |
		I2C_HS_NACKERR);

	/* clear bit mask */
	write32(&regs->intr_mask, I2C_HS_NACKERR | I2C_ACKERR |
		I2C_TRANSAC_COMP);

	if (ret_code != I2C_OK) {
		mtk_i2c_dump_info(bus);

		/* reset the i2c controller for next i2c transfer. */
		write32(&regs->softreset, 0x1);

		i2c_dma_reset(dma_regs);
	}

	return ret_code;
}

static uint8_t mtk_i2c_should_combine(I2cSeg *seg, int left_count)
{
	if (left_count >= 2 && seg[0].read == 0 && seg[1].read == 1 &&
	    seg[0].chip == seg[1].chip)
		return 1;
	else
		return 0;
}

static int i2c_transfer(I2cOps *me, I2cSeg *segments, int seg_count)
{
	int ret = 0;
	int i;
	int read;

	MTKI2c *bus = container_of(me, MTKI2c, ops);

	for (i = 0; i < seg_count; i++) {
		if (mtk_i2c_should_combine(&segments[i], seg_count - i))
			read = I2C_WRITE_READ_MODE;
		else
			read = segments[i].read;

		ret = mtk_i2c_transfer(bus, &segments[i], read);

		if (ret)
			break;

		if (read == I2C_WRITE_READ_MODE)
			i++;
	}

	return ret;
}

MTKI2c *new_mtk_i2c(uintptr_t base, uintptr_t dma_base)
{
	MTKI2c *bus = xzalloc(sizeof(*bus));

	bus->ops.transfer = &i2c_transfer;
	bus->base = (struct mtk_i2c_regs *)base;
	bus->dma_base = (struct mtk_i2c_dma_regs *)dma_base;
	bus->write_buffer = dma_malloc(MAX_TRANSFER_LEN);
	bus->read_buffer = dma_malloc(MAX_TRANSFER_LEN);

	return bus;
}
