/*
 * Copyright 2014 Google Inc.
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

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>
#include <usb/usb.h>

#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/net/mii.h"
#include "drivers/net/net.h"
#include "drivers/net/smsc95xx.h"
#include "drivers/net/usb_eth.h"

static Smsc95xxDev smsc_dev;

/*
 * The lower-level utilities
 */

static int smsc95xx_read_reg(usbdev_t *dev, uint16_t index, uint32_t *data)
{
	return usb_eth_read_reg(dev, UsbVendorReqRead, 0,
				index, sizeof(*data), data);
}

static int smsc95xx_write_reg(usbdev_t *dev, uint16_t index, uint32_t data)
{
	return usb_eth_write_reg(dev, UsbVendorReqWrite, 0,
				index, sizeof(data), &data);
}

static int smsc95xx_wait_for_phy(usbdev_t *dev)
{
	uint32_t val;
	int i;

	for (i = 0; i < 100; i++) {
		if (smsc95xx_read_reg(dev, MiiAddrReg, &val))
			return 1;
		if (!(val & MiiBusy))
			return 0;
		udelay(100);
	}

	return 1;
}

static int smsc95xx_mdio_read(NetDevice *dev, uint8_t loc, uint16_t *val)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;
	uint32_t addr;
	uint32_t data;

	if (smsc95xx_wait_for_phy(usb_dev)) {
		printf("SMSC95xx: MII is busy.\n");
		return 1;
	}

	addr = (Smsc95xxPhyId << 11) | (loc << 6) | MiiRead;
	if (smsc95xx_write_reg(usb_dev, MiiAddrReg, addr))
		return 1;

	if (smsc95xx_wait_for_phy(usb_dev)) {
		printf("SMSC95xx: Timed out reading MII.\n");
		return 1;
	}

	if (smsc95xx_read_reg(usb_dev, MiiDataReg, &data))
		return 1;
	*val = data & 0xFFFF;
	return 0;
}

static int smsc95xx_mdio_write(NetDevice *dev, uint8_t loc, uint16_t val)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;
	uint32_t addr;

	if (smsc95xx_wait_for_phy(usb_dev)) {
		printf("SMSC95xx: MII is busy.\n");
		return 1;
	}

	if (smsc95xx_write_reg(usb_dev, MiiDataReg, val))
		return 1;

	addr = (Smsc95xxPhyId << 11) | (loc << 6) | MiiWrite;
	if (smsc95xx_write_reg(usb_dev, MiiAddrReg, addr))
		return 1;

	if (smsc95xx_wait_for_phy(usb_dev)) {
		printf("SMSC95xx: Timed out writing MII.\n");
		return 1;
	}

	return 0;
}

static int smsc95xx_wait_eeprom(usbdev_t *dev)
{
	uint32_t val;
	int i;

	for (i = 0; i < 310; i++) {
		if (smsc95xx_read_reg(dev, E2PCmdReg, &val))
			return 1;
		if (!(val & E2PCmdBusy) || (val & E2PCmdTimeout))
			break;
		udelay(100);
	}

	if (val & (E2PCmdTimeout | E2PCmdBusy)) {
		printf("SMSC95xx: EEPROM read operation timeout.\n");
		if (val & E2PCmdTimeout) {
			/* Clear timeout so next operation doesn't see it */
			if (smsc95xx_write_reg(dev, E2PCmdReg, val))
				return 1;
		}
		return 1;
	}

	return 0;
}

static int smsc95xx_read_eeprom(usbdev_t *dev, uint32_t offset,
				uint32_t length, uint8_t *data)
{
	uint32_t val;
	int i;

	if (smsc95xx_wait_eeprom(dev))
		return 1;

	for (i = 0; i < length; i++) {
		val = E2PCmdBusy | E2PCmdRead | (offset & E2PCmdAddr);
		if (smsc95xx_write_reg(dev, E2PCmdReg, val))
			return 1;

		if (smsc95xx_wait_eeprom(dev))
			return 1;

		if (smsc95xx_read_reg(dev, E2PDataReg, &val))
			return 1;
		data[i] = val & 0xFF;
		offset++;
	}
	return 0;
}

static int smsc95xx_init_reset(usbdev_t *usb_dev)
{
	uint32_t read_buf;
	int timeout;

	if (smsc95xx_write_reg(usb_dev, HwCfgReg, HwCfgLrst))
		return 1;

	timeout = 0;
	do {
		if (smsc95xx_read_reg(usb_dev, HwCfgReg, &read_buf))
			return 1;
		udelay(50);
		timeout++;
		if (timeout >= 1000) {
			printf("SMSC95xx: Timeout waiting for Lite Reset.\n");
			return 1;
		}
	} while (read_buf & HwCfgLrst);

	if (smsc95xx_write_reg(usb_dev, PmCtrlReg, PmCtrlPhyRst))
		return 1;

	timeout = 0;
	do {
		if (smsc95xx_read_reg(usb_dev, PmCtrlReg, &read_buf))
			return 1;
		udelay(50);
		timeout++;
		if (timeout >= 1000) {
			printf("SMSC95xx: Timeout waiting for PHY reset.\n");
			return 1;
		}
	} while (read_buf & PmCtrlPhyRst);

	return 0;
}

static int smsc95xx_set_cfg(usbdev_t *usb_dev)
{
	uint32_t read_buf;

	if (smsc95xx_write_reg(usb_dev, BulkInDelayReg, BulkInDelayDefault))
		return 1;

	if (smsc95xx_read_reg(usb_dev, HwCfgReg, &read_buf))
		return 1;
	read_buf |= HwCfgBir;
	if (smsc95xx_write_reg(usb_dev, HwCfgReg, read_buf))
		return 1;

	if (smsc95xx_write_reg(usb_dev, IntStsReg, 0xFFFFFFFF))
		return 1;

	if (smsc95xx_write_reg(usb_dev, AfcCfgReg, AfcCfgDefault))
		return 1;

	if (smsc95xx_write_reg(usb_dev, Vlan1Reg, EthP8021Q))
		return 1;

	return 0;
}

static int smsc95xx_start(usbdev_t *usb_dev)
{
	uint32_t read_buf;

	if (smsc95xx_read_reg(usb_dev, MacCtrlReg, &read_buf))
		return 1;

	read_buf &= ~MacCrPrms;
	read_buf |= MacCrTxEn | MacCrRxEn;

	if (smsc95xx_write_reg(usb_dev, MacCtrlReg, read_buf))
		return 1;

	if (smsc95xx_write_reg(usb_dev, TxCfgReg, TxCfgOn))
		return 1;

	return 0;
}

/*
 * The higher-level commands
 */

static int smsc95xx_init(GenericUsbDevice *gen_dev)
{
	usbdev_t *usb_dev = gen_dev->dev;

	printf("SMSC95xx: Initializing\n");

	if (usb_eth_init_endpoints(usb_dev, &smsc_dev.bulk_in, 1,
				   &smsc_dev.bulk_out, 2)) {
		printf("SMSC95xx: Problem with the endpoints.\n");
		return 1;
	}

	if (smsc95xx_init_reset(usb_dev))
		return 1;

	if (smsc95xx_set_cfg(usb_dev))
		return 1;

	if (mii_phy_initialize(&smsc_dev.usb_eth_dev.net_dev))
		return 1;

	if (smsc95xx_start(usb_dev))
		return 1;

	printf("SMSC95xx: Done initializing\n");
	return 0;
}

static int smsc95xx_send(NetDevice *net_dev, void *buf, uint16_t len)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	uint32_t tx_cmd_a;
	uint32_t tx_cmd_b;
	static uint8_t msg[CONFIG_UIP_BUFSIZE +
			   sizeof(tx_cmd_a) + sizeof(tx_cmd_b)];

	if (len > sizeof(msg) - sizeof(tx_cmd_a) - sizeof(tx_cmd_b)) {
		printf("SMSC95xx: Packet size %u is too large.\n", len);
		return 1;
	}

	tx_cmd_a = htole32((uint32_t)len | TxCmdAFirstSeg | TxCmdALastSeg);
	tx_cmd_b = htole32((uint32_t)len);

	memcpy(msg, &tx_cmd_a, sizeof(tx_cmd_a));
	memcpy(msg + sizeof(tx_cmd_a), &tx_cmd_b, sizeof(tx_cmd_b));
	memcpy(msg + sizeof(tx_cmd_a) + sizeof(tx_cmd_b), buf, len);

	if (usb_dev->controller->bulk(smsc_dev.bulk_out,
				     len + sizeof(tx_cmd_a) + sizeof(tx_cmd_b),
				     msg, 0) < 0) {
		printf("SMSC95xx: Failed to send packet.\n");
		return 1;
	}

	return 0;
}

static int smsc95xx_recv(NetDevice *net_dev, void *buf, uint16_t *len,
			 int maxlen)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	uint32_t rx_status;
	uint32_t packet_len;
	static int32_t buf_size = 0;
	static uint8_t msg[RxUrbSize + sizeof(packet_len)];
	static int offset;

	if (offset >= buf_size) {
		offset = 0;
		buf_size = usb_dev->controller->bulk(smsc_dev.bulk_in,
						     RxUrbSize, msg, 0);
		if (buf_size < 0) {
			printf("SMSC95xx: Bulk read error %#x\n", buf_size);
			return 1;
		}
	}

	if (!buf_size) {
		*len = 0;
		return 0;
	}

	memcpy(&rx_status, msg + offset, sizeof(rx_status));
	rx_status = le32toh(rx_status);
	packet_len = ((rx_status & RxStsFl) >> 16);

	if (rx_status & RxStsEs) {
		offset += sizeof(rx_status) + packet_len;
		printf("SMSC95xx: Error header %#x\n", rx_status);
		return 1;
	}

	*len = packet_len;
	if ((packet_len > maxlen) || (offset + packet_len > buf_size)) {
		buf_size = 0;
		offset = 0;
		printf("SMSC95xx: Packet is too large.\n");
		return 1;
	}

	memcpy(buf, msg + offset + sizeof(rx_status), packet_len);
	offset += sizeof(rx_status) + packet_len;

	return 0;
}

static const uip_eth_addr *smsc95xx_get_mac(NetDevice *net_dev)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;
	uint32_t addrh;
	uint32_t addrl;

	if (smsc95xx_read_eeprom(usb_dev, EepromMacOffset,
				 sizeof(uip_eth_addr),
				 (uint8_t *)&smsc_dev.mac_addr)) {
		printf("SMSC95xx: Failed to retrieve MAC address.\n");
		return NULL;
	} else {
		addrh = (smsc_dev.mac_addr.addr[5] << 8) |
			(smsc_dev.mac_addr.addr[4]);
		addrl = (smsc_dev.mac_addr.addr[3] << 24) |
			(smsc_dev.mac_addr.addr[2] << 16) |
			(smsc_dev.mac_addr.addr[1] << 8) |
			(smsc_dev.mac_addr.addr[0]);
		if (smsc95xx_write_reg(usb_dev, AddrHReg, addrh) ||
		    smsc95xx_write_reg(usb_dev, AddrLReg, addrl)) {
			printf("SMSC95xx: Failed to write MAC address.\n");
			return NULL;
		}
		return &smsc_dev.mac_addr;
	}
}

/*
 * Connecting to the USB and network stacks
 */

static const UsbEthId smsc95xx_supported_ids[] = {
	/* SMSC 9514 */
	{ 0x0424, 0xec00 },
};

static Smsc95xxDev smsc_dev = {
	.usb_eth_dev = {
		.init = &smsc95xx_init,
		.net_dev = {
			.ready = &mii_ready,
			.recv = &smsc95xx_recv,
			.send = &smsc95xx_send,
			.get_mac = &smsc95xx_get_mac,
			.mdio_read = &smsc95xx_mdio_read,
			.mdio_write = &smsc95xx_mdio_write,
		},
		.supported_ids = smsc95xx_supported_ids,
		.num_supported_ids = ARRAY_SIZE(smsc95xx_supported_ids),
	},
};

static int smsc95xx_driver_register(void)
{
	list_insert_after(&smsc_dev.usb_eth_dev.list_node,
			  &usb_eth_drivers);
	return 0;
}

INIT_FUNC(smsc95xx_driver_register);
