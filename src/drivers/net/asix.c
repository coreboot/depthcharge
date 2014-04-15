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
#include "drivers/net/net.h"


/*
 * Utility functions.
 */

static int send_control_dev_req(usbdev_t *dev, dev_req_t *req, void *data)
{
	return (dev->controller->control(
		dev, req->data_dir == device_to_host ? IN : OUT,
		sizeof(dev_req_t), req, req->wLength, (uint8_t *)data) < 0);
}

static int asix_ctrl_read(usbdev_t *dev, uint8_t request, uint16_t value,
		    uint16_t index, uint16_t length, void *data)
{
	dev_req_t dev_req;
	dev_req.req_recp = dev_recp;
	dev_req.req_type = vendor_type;
	dev_req.data_dir = device_to_host;
	dev_req.bRequest = request;
	dev_req.wValue = value;
	dev_req.wIndex = index;
	dev_req.wLength = length;

	return send_control_dev_req(dev, &dev_req, data);
}

static int asix_ctrl_write(usbdev_t *dev, uint8_t request, uint16_t value,
		    uint16_t index, uint16_t length, void *data)
{
	dev_req_t dev_req;
	dev_req.req_recp = dev_recp;
	dev_req.req_type = vendor_type;
	dev_req.data_dir = host_to_device;
	dev_req.bRequest = request;
	dev_req.wValue = value;
	dev_req.wIndex = index;
	dev_req.wLength = length;

	return send_control_dev_req(dev, &dev_req, data);
}

static int asix_write_gpio(usbdev_t *dev, uint16_t value)
{
	if (asix_ctrl_write(dev, GpioRegWrite, value, 0, 0, NULL)) {
		printf("ASIX: Failed to set up GPIOs.\n");
		return 1;
	}
	return 0;
}

static int asix_sw_reset(usbdev_t *dev, uint8_t bits)
{
	if (asix_ctrl_write(dev, SoftResetRegWrite, bits, 0, 0, NULL)) {
		printf("ASIX: Software reset failed.\n");
		return 1;
	}
	return 0;
}

static int asix_phy_addr(usbdev_t *dev)
{
	uint8_t phy_addr[2];
	if (asix_ctrl_read(dev, PhyAddrRegRead, 0, 0, sizeof(phy_addr),
			phy_addr)) {
		printf("ASIX: Reading PHY address register failed.\n");
		return -1;
	}
	return phy_addr[1];
}

static int asix_write_rx_ctl(usbdev_t *dev, uint16_t val)
{
	if (asix_ctrl_write(dev, RxCtrlRegWrite, val, 0, 0, NULL)) {
		printf("ASIX: Setting the rx control reg failed.\n");
		return 1;
	}
	return 0;
}

static int asix_set_sw_mii(usbdev_t *dev)
{
	if (asix_ctrl_write(dev, SoftSerMgmtCtrlReg, 0, 0, 0, NULL)) {
		printf("ASIX: Failed to enabled software MII access.\n");
		return 1;
	}
	return 0;
}

static int asix_set_hw_mii(usbdev_t *dev)
{
	if (asix_ctrl_write(dev, HardSerMgmtCtrlReg, 0, 0, 0, NULL)) {
		printf("ASIX: Failed to enabled software MII access.\n");
		return 1;
	}
	return 0;
}

static int asix_mdio_read(usbdev_t *dev, int phy_id, int loc, uint16_t *val)
{
	if (asix_set_sw_mii(dev) ||
	    asix_ctrl_read(dev, PhyRegRead, phy_id, loc, sizeof(*val), val) ||
	    asix_set_hw_mii(dev)) {
		printf("ASIX: MDIO read failed.\n");
		return 1;
	}
	return 0;
}

static int asix_mdio_write(usbdev_t *dev, int phy_id, int loc, uint16_t val)
{
	val = htolew(val);
	if (asix_set_sw_mii(dev) ||
	    asix_ctrl_write(dev, PhyRegWrite, phy_id, loc, sizeof(val), &val) ||
	    asix_set_hw_mii(dev)) {
		printf("ASIX: MDIO write failed.\n");
		return 1;
	}
	return 0;
}

static int asix_wait_for_phy(usbdev_t *dev, int phy_id)
{
	int i;
	uint16_t bmsr;

	for (i = 0; i < 100; i++) {
		if (asix_mdio_read(dev, phy_id, MiiBmsr, &bmsr))
			return 1;
		if (bmsr)
			return 0;
		udelay(50);
	}

	return 1;
}

static int asix_restart_autoneg(usbdev_t *dev, int phy_id)
{
	uint16_t bmsr;
	if (asix_mdio_read(dev, phy_id, MiiBmsr, &bmsr))
		return 1;

	if (!(bmsr & BmsrAnegCapable)) {
		printf("ASIX: No AutoNeg, falling back to 100baseTX\n");
		return asix_mdio_write(dev, phy_id, MiiBmcr,
			BmcrSpeedSel | BmcrDuplexMode);
	} else {
		return asix_mdio_write(dev, phy_id, MiiBmcr,
			BmcrAutoNegEnable | BmcrRestartAutoNeg);
	}
}

static int asix_write_medium_mode(usbdev_t *dev, uint16_t mode)
{
	if (asix_ctrl_write(dev, MedModeRegWrite, mode, 0, 0, NULL)) {
		printf("ASIX: Failed to write medium mode %#x.\n", mode);
		return 1;
	}
	return 0;
}



/*
 * The main attraction.
 */

typedef struct AsixDev {
	endpoint_t *bulk_in;
	endpoint_t *bulk_out;
	uip_eth_addr mac_addr;
	int phy_id;
} AsixDev;

static int asix_init(GenericUsbDevice *gen_dev)
{
	AsixDev asix_dev;
	usbdev_t *usb_dev = gen_dev->dev;

	// Map out the endpoints.
	asix_dev.bulk_in = &usb_dev->endpoints[2];
	asix_dev.bulk_out = &usb_dev->endpoints[3];

	if (!asix_dev.bulk_in || !asix_dev.bulk_out ||
			asix_dev.bulk_in->type != BULK ||
			asix_dev.bulk_out->type != BULK ||
			asix_dev.bulk_in->direction != IN ||
			asix_dev.bulk_out->direction != OUT) {
		printf("ASIX: Problem with the endpoints.\n");
		return 1;
	}

	if (asix_write_gpio(usb_dev, GpioGpo2En | GpioGpo2 | GpioRse))
		return 1;

	asix_dev.phy_id = asix_phy_addr(usb_dev);
	if (asix_dev.phy_id < 0)
		return 1;
	int embed_phy = (asix_dev.phy_id & 0x1f) == 0x10 ? 1 : 0;
	if (asix_ctrl_write(usb_dev, SoftPhySelRegWrite, embed_phy, 0, 0,
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
	if (asix_wait_for_phy(usb_dev, asix_dev.phy_id))
		return 1;

	if (asix_write_rx_ctl(usb_dev, 0))
		return 1;

	if (asix_mdio_write(usb_dev, asix_dev.phy_id, MiiBmcr, BmcrReset))
		return 1;
	if (asix_mdio_write(usb_dev, asix_dev.phy_id, MiiAnar,
			    AdvertiseAll | AdvertiseCsma))
		return 1;
	if (asix_restart_autoneg(usb_dev, asix_dev.phy_id))
		return 1;

	if (asix_write_medium_mode(usb_dev, MediumDefault))
		return 1;

	if (asix_ctrl_write(usb_dev, IpgRegWrite, Ipg0Default | Ipg1Default,
			Ipg2Default, 0, NULL)) {
		printf("ASIX: Failed to write IPG values.\n");
		return 1;
	}

	if (asix_write_rx_ctl(usb_dev, RxCtrlDefault))
		return 1;


	gen_dev->dev_data = xmalloc(sizeof(asix_dev));
	memcpy(gen_dev->dev_data, &asix_dev, sizeof(asix_dev));

	return 0;
}

static int asix_ready(NetDevice *net_dev, int *ready)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;
	AsixDev *asix_dev = gen_dev->dev_data;

	uint16_t link_status = 0;
	if (asix_mdio_read(usb_dev, asix_dev->phy_id, MiiBmsr, &link_status)) {
		printf("ASIX: Failed to read BMSR.\n");
		return 1;
	}
	*ready = (link_status & BmsrLstatus);
	return 0;
}

static int asix_send(NetDevice *net_dev, void *buf, uint16_t len)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;
	AsixDev *asix_dev = gen_dev->dev_data;

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

	if (usb_dev->controller->bulk(asix_dev->bulk_out,
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
	AsixDev *asix_dev = gen_dev->dev_data;

	uint32_t packet_len;
	static int32_t buf_size = 0;
	static uint8_t msg[RxUrbSize + sizeof(packet_len)];
	static int offset;

	if (offset >= buf_size) {
		offset = 0;
		buf_size = usb_dev->controller->bulk(asix_dev->bulk_in,
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
	AsixDev *asix_dev = gen_dev->dev_data;

	if (asix_ctrl_read(usb_dev, NodeIdRead, 0, 0, sizeof(uip_eth_addr),
			&asix_dev->mac_addr)) {
		printf("ASIX: Failed to retrieve MAC address.\n");
		return NULL;
	} else {
		return &asix_dev->mac_addr;
	}
}



/*
 * Code to plug the driver into the USB and network stacks.
 */

static NetDevice asix_network_device = {
	.ready = &asix_ready,
	.recv = &asix_recv,
	.send = &asix_send,
	.get_mac = &asix_get_mac,
};

typedef struct AsixUsbId {
	uint16_t vendor_id;
	uint16_t product_id;
} AsixUsbId;

/* Supported usb ethernet dongles. */
static const AsixUsbId supported_ids[] = {
	/* Apple USB Ethernet Dongle */
	{ 0x05ac, 0x1402 },
	/* ASIX USB Ethernet Dongle */
	{ 0x0b95, 0x7720 },
	{ 0x0b95, 0x772a },
	{ 0x0b95, 0x772b },
	{ 0x0b95, 0x7e2b },
};

static int asix_probe(GenericUsbDevice *dev)
{
	int i;
	if (asix_network_device.dev_data)
		return 0;

	device_descriptor_t *dd = (device_descriptor_t *)dev->dev->descriptor;
	for (i = 0; i < ARRAY_SIZE(supported_ids); i++) {
		if (dd->idVendor == supported_ids[i].vendor_id &&
		    dd->idProduct == supported_ids[i].product_id) {
			asix_network_device.dev_data = dev;
			if (asix_init(dev)) {
				return 0;
			} else {
				net_set_device(&asix_network_device);
				return 1;
			}
		}
	}
	return 0;
}

static void asix_remove(GenericUsbDevice *dev)
{
	if (net_get_device() == &asix_network_device)
		net_set_device(NULL);
	free(dev->dev_data);
}

static GenericUsbDriver asix_usb_driver = {
	.probe = &asix_probe,
	.remove = &asix_remove,
};

static int asix_driver_register(void)
{
	list_insert_after(&asix_usb_driver.list_node, &generic_usb_drivers);
	return 0;
}

INIT_FUNC(asix_driver_register);
