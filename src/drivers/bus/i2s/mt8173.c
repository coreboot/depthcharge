/*
 * Copyright 2015 Mediatek Inc.
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
#include "drivers/bus/i2s/mt8173.h"
#include "drivers/bus/i2s/i2s.h"

#ifdef MT8173_I2S_DEBUG
#define I2S_LOG(args...)	printf(args)
#else
#define I2S_LOG(args...)
#endif

static uint32_t mtk_i2s_rate(uint32_t rate)
{
	switch (rate) {
	case 8000: return 0;
	case 11025: return 1;
	case 12000: return 2;
	case 16000: return 4;
	case 22050: return 5;
	case 24000: return 6;
	case 32000: return 8;
	case 44100: return 9;
	case 48000: return 10;
	case 88000: return 11;
	case 96000: return 12;
	case 174000: return 13;
	case 192000: return 14;
	default:
		die("Unsupported I2S bus rate: %u", rate);
	}
}

enum {
	BUFFER_PADDING_LENGTH = 2400,    /* 50ms@48KHz stereo */
};

static int mtk_i2s_init(MtkI2s *bus)
{
	Mt8173I2sRegs *regs = bus->regs;
	uint32_t val;
	uint32_t rate = mtk_i2s_rate(bus->rate);

	if (bus->initialized)
		return 0;

	bus->initialized = 1;

	/* set O3/O4 16bits */
	clrbits_le32(&regs->conn_24bit, (1 << 3) | (1 << 4));

	/* I05/I06 -> O03/O04 connection */
	setbits_le32(&regs->conn1, 1 << 21);
	setbits_le32(&regs->conn2, 1 << 6);

	/* set channel */
	if (bus->channels == 1)
		setbits_le32(&regs->dac_con1, 1 << 21);
	else
		clrbits_le32(&regs->dac_con1, 1 << 21);

	/* set rate */
	clrsetbits_le32(&regs->dac_con1, 0xf, rate);

	/* set I2S output */
	val = AFE_I2S_CON1_LOW_JITTER_CLOCK |
	      ((rate & AFE_I2S_RATE_MASK) << AFE_I2S_RATE_SHIFT) |
	      AFE_I2S_CON1_FORMAT_I2S | AFE_I2S_CON1_WLEN_32BITS |
	      AFE_I2S_CON1_ENABLE;
	writel(val, &regs->i2s_con1);

	/* enable AFE */
	setbits_le32(&regs->dac_con0, 1 << 0);

	return 0;
}

static int mtk_i2s_enable(SoundRouteComponentOps *me)
{
	MtkI2s *bus = container_of(me, MtkI2s, component.ops);

	return mtk_i2s_init(bus);
}

static int mtk_i2s_send(I2sOps *me, uint32_t *data, unsigned int length)
{
	MtkI2s *bus = container_of(me, MtkI2s, ops);
	Mt8173I2sRegs *regs = bus->regs;
	uint32_t *buffer;
	uintptr_t buf_size, buf_base, data_end;

	mtk_i2s_init(bus);

	buf_size = (length + BUFFER_PADDING_LENGTH) * sizeof(uint32_t);
	buffer = dma_memalign(16, buf_size); /* 16-byte aligned DMA buffer */
	buf_base = (uintptr_t)buffer;
	data_end = buf_base + length * sizeof(uint32_t) - 1;
	memset(buffer + length, 0, BUFFER_PADDING_LENGTH * sizeof(uint32_t));

	I2S_LOG("len = %d buf = %p base = %lx end = %lx data = %lx\n",
		length, buffer, buf_base, buf_base + buf_size - 1, data_end);

	memcpy((void *)buf_base, data, length * sizeof(uint32_t));
	writel(buf_base, &regs->dl1_base);
	writel(buf_base + buf_size - 1, &regs->dl1_end);

	/* enable DL1 */
	setbits_le32(&regs->dac_con0, 1 << 1);

	while (readl(&regs->dl1_cur) <= data_end)
		;  /* wait until HW read pointer pass the end of data */

	/* stop DL1 */
	clrbits_le32(&regs->dac_con0, 1 << 1);

	free(buffer);
	return 0;
}

MtkI2s *new_mtk_i2s(uintptr_t base, uint32_t channels, uint32_t rate)
{
	MtkI2s *bus = xzalloc(sizeof(*bus));

	bus->component.ops.enable = &mtk_i2s_enable;
	bus->ops.send = &mtk_i2s_send;
	bus->regs = (void *)base;
	bus->channels = channels;
	bus->rate = rate;
	return bus;
}
