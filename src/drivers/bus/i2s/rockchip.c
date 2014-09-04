/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/i2s/rockchip.h"
#include "drivers/bus/i2s/i2s.h"

typedef struct __attribute__((packed)) RockchipI2sRegs {
	uint32_t txcr;	/* I2S_TXCR, 0x00 */
	uint32_t rxcr;	/* I2S_RXCR, 0x04 */
	uint32_t ckr;	/* I2S_CKR, 0x08 */
	uint32_t fifolr;	/* I2S_FIFOLR, 0x0C */
	uint32_t dmacr;	/* I2S_DMACR, 0x10 */
	uint32_t intcr;	/* I2S_INTCR, 0x14 */
	uint32_t intsr;	/* I2S_INTSR, 0x18 */
	uint32_t xfer;	/* I2S_XFER, 0x1C */
	uint32_t clr;	/* I2S_CLR, 0x20 */
	uint32_t txdr;	/* I2S_TXDR, 0x24 */
	uint32_t rxdr;	/* I2S_RXDR, 0x28 */
} RockchipI2sRegs;

/* I2S_XFER */
#define I2S_RX_TRAN_BIT      (1 << 1)
#define I2S_TX_TRAN_BIT      (1 << 0)
#define I2S_TRAN_MASK	     (3 << 0)
/* I2S_TXCKR */
#define I2S_MCLK_DIV(x)         ((0xFF & x) << 16)
#define I2S_MCLK_DIV_MASK       ((0xFF) << 16)
#define I2S_RX_SCLK_DIV(x)      ((x & 0xFF) << 8)
#define I2S_RX_SCLK_DIV_MASK    ((0xFF) << 8)
#define I2S_TX_SCLK_DIV(x)      (x & 0xFF)
#define I2S_TX_SCLK_DIV_MASK    (0xFF)

#define I2S_DATA_WIDTH(w)               ((w & 0x1F) << 0)

static int rockchip_i2s_init(RockchipI2s *bus)
{
	RockchipI2sRegs *regs = bus->regs;
	uint32_t bps = bus->bits_per_sample;
	uint32_t lrf = bus->lr_frame_size;
	uint32_t chn = bus->channels;
	uint32_t mode = 0;

	printf("%s: %d, %d, %d\n", __func__,
		bus->bits_per_sample,
		bus->lr_frame_size, bus->channels);
	clrbits_le32(&regs->xfer, I2S_TX_TRAN_BIT);
	mode = readl(&regs->txcr) & ~(0x1F);
	switch (bus->bits_per_sample) {
	case 16:
		mode |= I2S_DATA_WIDTH(15);
		break;
	case 24:
		mode |= I2S_DATA_WIDTH(23);
		break;
	default:
		printf("%s: Invalid sample size input %d\n",
			__func__, bus->bits_per_sample);
		return 1;
	}
	writel(mode, &regs->txcr);

	mode = readl(&regs->ckr) & ~I2S_MCLK_DIV_MASK;
	mode |= I2S_MCLK_DIV((lrf / (bps * chn) - 1));

	mode &= ~I2S_TX_SCLK_DIV_MASK;
	mode |= I2S_TX_SCLK_DIV((bus->bits_per_sample * bus->channels - 1));
	writel(mode, &regs->ckr);
	return 0;
}

static int i2s_send_data(uint32_t *data, unsigned int length,
			    RockchipI2sRegs *regs)
{
	for (int i = 0; i < MIN(32, length); i++)
		writel(*data++, &regs->txdr);

	length -= MIN(32, length);

	setbits_le32(&regs->xfer, I2S_TRAN_MASK);//enable both tx and rx
	while (length) {
		if ((readl(&regs->fifolr) & 0x3f) < 0x3f) {
			writel(*data++, &regs->txdr);
			length--;
		}
	}
	while (readl(&regs->fifolr) & 0x3f)
		/* wait until FIFO empty */;
	clrbits_le32(&regs->xfer, I2S_TRAN_MASK);
	writel(0, &regs->clr);
	return 0;
}

static int rockchip_i2s_send(I2sOps *me, uint32_t *data, unsigned int length)
{
	RockchipI2s *bus = container_of(me, RockchipI2s, ops);
	RockchipI2sRegs *regs = bus->regs;

	if (!bus->initialized) {
		if (rockchip_i2s_init(bus))
			return -1;
		else
			bus->initialized = 1;
	}

	i2s_send_data(data, length, regs);

	return 0;
}

RockchipI2s *new_rockchip_i2s(uintptr_t regs, int bits_per_sample,
	int channels, int lr_frame_size)
{
	RockchipI2s *bus = xzalloc(sizeof(*bus));
	bus->ops.send = &rockchip_i2s_send;
	bus->regs = (RockchipI2sRegs *)regs;
	bus->bits_per_sample = bits_per_sample;
	bus->channels = channels;
	bus->lr_frame_size = lr_frame_size;
	return bus;
}
