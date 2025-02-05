// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <libpayload.h>

#include "drivers/bus/i2s/mtk_v3.h"

#define BUFFER_PADDING_LENGTH 2400	/* 50ms@48KHz stereo */

static int mtk_i2s_enable(SoundRouteComponentOps *me)
{
	MtkI2s *bus = container_of(me, MtkI2s, component.ops);

	return mtk_i2s_init(bus);
}

static size_t copy_to_buf(uint8_t *buf, size_t size,
			  const uint8_t **data, size_t *data_size)
{
	size_t update_size = MIN(size, *data_size);

	memcpy(buf, *data, update_size);
	*data_size -= update_size;
	*data += update_size;

	if (size > update_size)
		memset(buf + update_size, 0, size - update_size);

	return size - update_size;
}

static int mtk_i2s_send(I2sOps *me, uint32_t *data, unsigned int length)
{
	MtkI2s *bus = container_of(me, MtkI2s, ops);
	uintptr_t buf_base, buf_end, buf_mid, data_cur, data_base, data_end;
	size_t data_size, pad_remain, pad_size;
	const uint8_t *cur = (uint8_t *)data;

	if (mtk_i2s_init(bus))
		return -1;

	data_cur = data_base = 0;
	data_size = length * sizeof(*data);
	/* 0s are padded at the end of data in case the DMA rings to the
	 * beginning of the buffer before disabling DMA.
	 */
	pad_remain = BUFFER_PADDING_LENGTH * sizeof(*data);
	buf_base =  (uintptr_t)SRAM_BASE;
	buf_end = buf_base + SRAM_SIZE - 1;
	buf_mid = buf_base + SRAM_SIZE / 2 - 1;
	data_end = buf_base + data_size - 1;

	I2S_LOG("len = %d base = %#lx end = %#lx data = %#lx\n",
		length, buf_base, buf_end, data_end);

	pad_size = copy_to_buf((uint8_t *)buf_base, SRAM_SIZE, &cur,
			       &data_size);
	pad_remain -= MIN(pad_remain, pad_size);

	mtk_etdmout_enable(bus, true, buf_base, buf_end);

	/*
	 * To improve overall bandwidth, divide the buffer into 2 halves and use
	 * them as ping-pong buffers. While one half is being consumed by DMA,
	 * refill the other half.
	 */
	while(1) {
		/* Wait for the 1st half of buffer to be consumed. */
		while ((data_cur = mtk_etdmout_get_current_buf(bus)) <= buf_mid) {
			if (data_base + data_cur > data_end)
				break;
		}
		if (data_base + data_cur > data_end)
			break;

		if (pad_remain) {
			pad_size = copy_to_buf((uint8_t *)buf_base,
					       SRAM_SIZE / 2,
					       &cur, &data_size);
			pad_remain -= MIN(pad_remain, pad_size);
		}

		/* Wait for the 2nd half of buffer to be consumed. */
		while ((data_cur = mtk_etdmout_get_current_buf(bus)) >= buf_mid)
			if (data_base + data_cur > data_end)
				break;

		data_base += SRAM_SIZE;
		if (data_base + data_cur > data_end)
			break;

		if (pad_remain) {
			pad_size = copy_to_buf((uint8_t *)(buf_base +
					       SRAM_SIZE / 2),
					       SRAM_SIZE / 2,
					       &cur, &data_size);
			pad_remain -= MIN(pad_remain, pad_size);
		}
	}

	mtk_etdmout_enable(bus, false, buf_base, buf_end);
	return 0;
}

MtkI2s *new_mtk_i2s(uintptr_t base, uint32_t channels, uint32_t rate,
		    uint32_t word_len, uint32_t bit_len)
{
	MtkI2s *bus = xzalloc(sizeof(*bus));

	bus->component.ops.enable = &mtk_i2s_enable;
	bus->ops.send = &mtk_i2s_send;
	bus->regs = (void *)base;
	bus->channels = channels;
	bus->rate = rate;
	bus->word_len = word_len;
	bus->bit_len = bit_len;

	return bus;
}
