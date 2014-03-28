/*
 * Copyright 2013 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2c/tegra.h"

static void tegra_i2c_reset(TegraI2c *bus)
{
	writel(bus->reset_bit, bus->set_reset_reg);
	udelay(1);
	writel(bus->reset_bit, bus->clear_reset_reg);
	bus->initialized = 0;
}

static int tegra_i2c_send_recv(TegraI2c *bus, int read,
			       uint32_t *headers, int header_words,
			       uint8_t *data, int data_len)
{
	TegraI2cRegs *regs = bus->regs;

	while (data_len) {
		uint32_t status = readl(&regs->fifo_status);
		int tx_empty = status & I2C_FIFO_STATUS_TX_FIFO_EMPTY_CNT_MASK;
		tx_empty >>= I2C_FIFO_STATUS_TX_FIFO_EMPTY_CNT_SHIFT;
		int rx_full = status & I2C_FIFO_STATUS_RX_FIFO_FULL_CNT_MASK;
		rx_full >>= I2C_FIFO_STATUS_RX_FIFO_FULL_CNT_SHIFT;

		while (header_words && tx_empty) {
			writel(*headers++, &regs->tx_packet_fifo);
			header_words--;
			tx_empty--;
		}

		if (!header_words) {
			if (read) {
				while (data_len && rx_full) {
					uint32_t word = readl(&regs->rx_fifo);
					int todo = MIN(data_len, sizeof(word));

					memcpy(data, &word, todo);
					data_len -= todo;
					data += sizeof(word);
					rx_full--;
				}
			} else {
				while (data_len && tx_empty) {
					uint32_t word;
					int todo = MIN(data_len, sizeof(word));

					memcpy(&word, data, todo);
					writel(word, &regs->tx_packet_fifo);
					data_len -= todo;
					data += sizeof(word);
					tx_empty--;
				}
			}
		}

		uint32_t transfer_status =
			readl(&regs->packet_transfer_status);

		if (transfer_status & I2C_PKT_STATUS_NOACK_ADDR) {
			printf("%s: The address was not acknowledged.\n",
			       __func__);
			tegra_i2c_reset(bus);
			return -1;
		} else if (transfer_status & I2C_PKT_STATUS_NOACK_DATA) {
			printf("%s: The data was not acknowledged.\n",
			       __func__);
			tegra_i2c_reset(bus);
			return -1;
		} else if (transfer_status & I2C_PKT_STATUS_ARB_LOST) {
			printf("%s: Lost arbitration.\n", __func__);
			tegra_i2c_reset(bus);
			return -1;
		}
	}

	return 0;
}

static int tegra_i2c_request(TegraI2c *bus, uint8_t chip, int cont,
			     int restart, int read, void *data, int data_len)
{
	uint32_t headers[3];

	if (restart && cont) {
		printf("%s: Repeat start and continue transfer are mutually "
		       "exclusive.\n", __func__);
		return -1;
	}

	headers[0] = (0 << IOHEADER_PROTHDRSZ_SHIFT) |
		     (1 << IOHEADER_PKTID_SHIFT) |
		     (bus->controller_id << IOHEADER_CONTROLLER_ID_SHIFT) |
		     IOHEADER_PROTOCOL_I2C | IOHEADER_PKTTYPE_REQUEST;

	headers[1] = (data_len - 1) << IOHEADER_PAYLOADSIZE_SHIFT;

	uint32_t slave_addr = (chip << 1) | (read ? 1 : 0);
	headers[2] = IOHEADER_I2C_REQ_ADDR_MODE_7BIT |
		     (slave_addr << IOHEADER_I2C_REQ_SLAVE_ADDR_SHIFT);
	if (read)
		headers[2] |= IOHEADER_I2C_REQ_READ;
	if (restart)
		headers[2] |= IOHEADER_I2C_REQ_REPEAT_START;
	if (cont)
		headers[2] |= IOHEADER_I2C_REQ_CONTINUE_XFER;

	return tegra_i2c_send_recv(bus, read, headers, ARRAY_SIZE(headers),
				   data, data_len);
}

static int i2c_transfer_segment(TegraI2c *bus, uint8_t chip,
				int restart, int read, void *buf, int len)
{
	const uint32_t max_payload =
		(IOHEADER_PAYLOADSIZE_MASK + 1) >> IOHEADER_PAYLOADSIZE_SHIFT;
	while (len) {
		int todo = MIN(len, max_payload);
		int cont = (todo < len);
		if (tegra_i2c_request(bus, chip,
				      cont, restart, read, buf, todo))
			// We should reset the controller here.
			return -1;
		len -= todo;
		buf += todo;
	}
	return 0;
}

static void i2c_init(TegraI2c *bus)
{
	TegraI2cRegs *regs = bus->regs;
	writel(I2C_CNFG_PACKET_MODE_EN, &regs->cnfg);
	bus->initialized = 1;
}

static int i2c_transfer(I2cOps *me, I2cSeg *segments, int seg_count)
{
	I2cSeg *seg = segments;
	TegraI2c *bus = container_of(me, TegraI2c, ops);

	if (!bus->initialized)
		i2c_init(bus);

	for (int i = 0; i < seg_count; seg++, i++) {
		if (i2c_transfer_segment(bus, seg->chip, i < seg_count - 1,
				       seg->read, seg->buf, seg->len))
			return -1;
	}
	return 0;
}

TegraI2c *new_tegra_i2c(void *regs, int controller_id, void *set_reset_reg,
			void *clear_reset_reg, uint32_t reset_bit)
{
	TegraI2c *bus = xzalloc(sizeof(*bus));
	bus->ops.transfer = &i2c_transfer;
	bus->regs = regs;
	bus->controller_id = controller_id;

	bus->set_reset_reg = set_reset_reg;
	bus->clear_reset_reg = clear_reset_reg;
	bus->reset_bit = reset_bit;

	return bus;
}
