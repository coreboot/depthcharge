/*
 * Copyright 2013 Google Inc.
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
#include "drivers/net/asix.h"
#include "drivers/net/mii.h"
#include "drivers/net/net.h"
#include "drivers/net/usb_eth.h"

static AsixDev asix_dev;

/*
 * Utility functions.
 */

static int asix_write_gpio(usbdev_t *dev, uint16_t value)
{
	if (usb_eth_write_reg(dev, GpioRegWrite, value, 0, 0, NULL)) {
		printf("ASIX: Failed to set up GPIOs.\n");
		return 1;
	}
	return 0;
}

static int asix_sw_reset(usbdev_t *dev, uint8_t bits)
{
	if (usb_eth_write_reg(dev, SoftResetRegWrite, bits, 0, 0, NULL)) {
		printf("ASIX: Software reset failed.\n");
		return 1;
	}
	return 0;
}

static int asix_phy_addr(usbdev_t *dev)
{
	uint8_t phy_addr[2];
	if (usb_eth_read_reg(dev, PhyAddrRegRead, 0, 0, sizeof(phy_addr),
			phy_addr)) {
		printf("ASIX: Reading PHY address register failed.\n");
		return -1;
	}
	return phy_addr[1];
}

static int asix_write_rx_ctl(usbdev_t *dev, uint16_t val)
{
	if (usb_eth_write_reg(dev, RxCtrlRegWrite, val, 0, 0, NULL)) {
		printf("ASIX: Setting the rx control reg failed.\n");
		return 1;
	}
	return 0;
}

static int asix_set_sw_mii(usbdev_t *dev)
{
	if (usb_eth_write_reg(dev, SoftSerMgmtCtrlReg, 0, 0, 0, NULL)) {
		printf("ASIX: Failed to enabled software MII access.\n");
		return 1;
	}
	return 0;
}

static int asix_set_hw_mii(usbdev_t *dev)
{
	if (usb_eth_write_reg(dev, HardSerMgmtCtrlReg, 0, 0, 0, NULL)) {
		printf("ASIX: Failed to enabled software MII access.\n");
		return 1;
	}
	return 0;
}

static int asix_mdio_read(NetDevice *dev, uint8_t loc, uint16_t *val)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	if (asix_set_sw_mii(usb_dev) ||
	    usb_eth_read_reg(usb_dev, PhyRegRead, asix_dev.phy_id,
			     loc, sizeof(*val), val) ||
	    asix_set_hw_mii(usb_dev)) {
		printf("ASIX: MDIO read failed (phy ID %#x, loc %#x).\n",
		       asix_dev.phy_id, loc);
		return 1;
	}
	return 0;
}

static int asix_mdio_write(NetDevice *dev, uint8_t loc, uint16_t val)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	val = htolew(val);
	if (asix_set_sw_mii(usb_dev) ||
	    usb_eth_write_reg(usb_dev, PhyRegWrite, asix_dev.phy_id,
			      loc, sizeof(val), &val) ||
	    asix_set_hw_mii(usb_dev)) {
		printf("ASIX: MDIO write failed (phy ID %#x, loc %#x.\n",
		       asix_dev.phy_id, loc);
		return 1;
	}
	return 0;
}

static int asix_wait_for_phy(NetDevice *dev)
{
	int i;
	uint16_t bmsr;

	for (i = 0; i < 100; i++) {
		if (asix_mdio_read(dev, MiiBmsr, &bmsr))
			return 1;
		if (bmsr)
			return 0;
		udelay(50);
	}

	return 1;
}

static int asix_write_medium_mode(usbdev_t *dev, uint16_t mode)
{
	if (usb_eth_write_reg(dev, MedModeRegWrite, mode, 0, 0, NULL)) {
		printf("ASIX: Failed to write medium mode %#x.\n", mode);
		return 1;
	}
	return 0;
}



/*
 * The main attraction.
 */

static int asix_init(GenericUsbDevice *gen_dev)
{
	usbdev_t *usb_dev = gen_dev->dev;

	if (usb_eth_init_endpoints(usb_dev, &asix_dev.bulk_in, 2,
				   &asix_dev.bulk_out, 3)) {
		printf("ASIX: Problem with the endpoints.\n");
		return 1;
	}

	if (asix_write_gpio(usb_dev, GpioGpo2En | GpioGpo2 | GpioRse))
		return 1;

	asix_dev.phy_id = asix_phy_addr(usb_dev);
	if (asix_dev.phy_id < 0)
		return 1;
	int embed_phy = (asix_dev.phy_id & 0x1f) == 0x10 ? 1 : 0;
	if (usb_eth_write_reg(usb_dev, SoftPhySelRegWrite, embed_phy, 0, 0,
			NULL)) {
		printf("ASIX: Failed to select PHY.\n");
		return 1;
	}

	if (asix_sw_reset(usb_dev, SoftResetPrl | SoftResetIppd))
		return 1;
	if (asix_sw_reset(usb_dev, 0))
		return 1;
	if (asix_sw_reset(usb_dev, embed_phy ? SoftResetIprl : SoftResetPrte))
		return 1;
	if (asix_write_rx_ctl(usb_dev, 0))
		return 1;

	if (asix_wait_for_phy(&asix_dev.usb_eth_dev.net_dev))
		return 1;

	if (mii_phy_initialize(&asix_dev.usb_eth_dev.net_dev))
		return 1;

	if (asix_write_medium_mode(usb_dev, MediumDefault))
		return 1;

	if (usb_eth_write_reg(usb_dev, IpgRegWrite, Ipg0Default | Ipg1Default,
			Ipg2Default, 0, NULL)) {
		printf("ASIX: Failed to write IPG values.\n");
		return 1;
	}

	if (asix_write_rx_ctl(usb_dev, RxCtrlDefault))
		return 1;

	return 0;
}

static int asix_send(NetDevice *net_dev, void *buf, uint16_t len)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	uint32_t packet_len;
	static uint8_t msg[CONFIG_UIP_BUFSIZE + sizeof(packet_len)];

	packet_len = ((len << 16) | (len << 0)) ^ 0xffff0000;
	packet_len = htolel(packet_len);
	memcpy(msg, &packet_len, sizeof(packet_len));
	if (len > sizeof(msg) - sizeof(packet_len)) {
		printf("ASIX: Packet size %u is too large.\n", len);
		return 1;
	}
	memcpy(msg + sizeof(packet_len), buf, len);

	if (len & 1)
		len++;

	if (usb_dev->controller->bulk(asix_dev.bulk_out,
			len + sizeof(packet_len), msg, 0) < 0) {
		printf("ASIX: Failed to send packet.\n");
		return 1;
	}

	return 0;
}

static int asix_recv(NetDevice *net_dev, void *buf, uint16_t *len, int maxlen)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	uint32_t packet_len;
	static int32_t buf_size = 0;
	static uint8_t msg[RxUrbSize + sizeof(packet_len)];
	static int offset;

	if (offset >= buf_size) {
		offset = 0;
		buf_size = usb_dev->controller->bulk(asix_dev.bulk_in,
				RxUrbSize, msg, 0);
		if (buf_size < 0)
			return 1;
	}

	if (!buf_size) {
		*len = 0;
		return 0;
	}

	memcpy(&packet_len, msg + offset, sizeof(packet_len));

	*len = (packet_len & 0x7ff);
	packet_len = (~packet_len >> 16) & 0x7ff;
	if (*len != packet_len) {
		buf_size = 0;
		offset = 0;
		printf("ASIX: Malformed packet length.\n");
		return 1;
	}
	if (packet_len & 1)
		packet_len++;
	if (packet_len > maxlen || offset + packet_len > buf_size) {
		buf_size = 0;
		offset = 0;
		printf("ASIX: Packet is too large.\n");
		return 1;
	}

	memcpy(buf, msg + offset + sizeof(packet_len), packet_len);
	offset += sizeof(packet_len) + packet_len;

	return 0;
}

static const uip_eth_addr *asix_get_mac(NetDevice *net_dev)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	if (usb_eth_read_reg(usb_dev, NodeIdRead, 0, 0, sizeof(uip_eth_addr),
			&asix_dev.mac_addr)) {
		printf("ASIX: Failed to retrieve MAC address.\n");
		return NULL;
	} else {
		return &asix_dev.mac_addr;
	}
}



/*
 * Code to plug the driver into the USB and network stacks.
 */

/* Supported usb ethernet dongles. */
static const UsbEthId asix_supported_ids[] = {
	/* Apple USB Ethernet Dongle */
	{ 0x05ac, 0x1402 },
	/* ASIX USB Ethernet Dongle */
	{ 0x0b95, 0x7720 },
	{ 0x0b95, 0x772a },
	{ 0x0b95, 0x772b },
	{ 0x0b95, 0x7e2b },
};

static AsixDev asix_dev = {
	.usb_eth_dev = {
		.init = &asix_init,
		.net_dev = {
			.ready = &mii_ready,
			.recv = &asix_recv,
			.send = &asix_send,
			.get_mac = &asix_get_mac,
			.mdio_read = &asix_mdio_read,
			.mdio_write = &asix_mdio_write,
		},
		.supported_ids = asix_supported_ids,
		.num_supported_ids = ARRAY_SIZE(asix_supported_ids),
	},
};

static int asix_driver_register(void)
{
	list_insert_after(&asix_dev.usb_eth_dev.list_node, &usb_eth_drivers);
	return 0;
}

INIT_FUNC(asix_driver_register);
