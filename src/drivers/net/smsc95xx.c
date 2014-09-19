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
#include "drivers/net/smsc95xx.h"
#include "drivers/net/net.h"
#include "drivers/net/mii.h"

/*
 * The lower-level utilities
 */

static int smsc95xx_read_reg(usbdev_t *dev, uint32_t index, uint32_t *data)
{
	dev_req_t dev_req;
	dev_req.req_recp = dev_recp;
	dev_req.req_type = vendor_type;
	dev_req.data_dir = device_to_host;
	dev_req.bRequest = UsbVendorReqRead;
	dev_req.wValue = 0;
	dev_req.wIndex = index;
	dev_req.wLength = sizeof(*data);

	return (dev->controller->control(dev, IN, sizeof(dev_req), &dev_req,
					 dev_req.wLength,
					 (uint8_t *)data) < 0);
}

static int smsc95xx_write_reg(usbdev_t *dev, uint32_t index, uint32_t data)
{
	dev_req_t dev_req;
	dev_req.req_recp = dev_recp;
	dev_req.req_type = vendor_type;
	dev_req.data_dir = host_to_device;
	dev_req.bRequest = UsbVendorReqWrite;
	dev_req.wValue = 0;
	dev_req.wIndex = index;
	dev_req.wLength = sizeof(data);

	return (dev->controller->control(dev, OUT, sizeof(dev_req), &dev_req,
					 dev_req.wLength,
					 (uint8_t *)&data) < 0);
}

static int smsc95xx_wait_for_phy(usbdev_t *dev)
{
	uint32_t val;
	int i;

	for (i = 0; i < 100; i++) {
		smsc95xx_read_reg(dev, MiiAddrReg, &val);
		if (!(val & MiiBusy))
			return 0;
		udelay(100);
	}

	return 1;
}

static int smsc95xx_mdio_read(usbdev_t *dev, uint32_t loc, uint32_t *val)
{
	uint32_t addr;

	if (smsc95xx_wait_for_phy(dev)) {
		printf("SMSC95xx: MII is busy.\n");
		return 1;
	}

	addr = (Smsc95xxPhyId << 11) | (loc << 6) | MiiRead;
	smsc95xx_write_reg(dev, MiiAddrReg, addr);

	if (smsc95xx_wait_for_phy(dev)) {
		printf("SMSC95xx: Timed out reading MII.\n");
		return 1;
	}

	smsc95xx_read_reg(dev, MiiDataReg, val);
	return 0;
}

static int smsc95xx_mdio_write(usbdev_t *dev, uint32_t loc, uint32_t val)
{
	uint32_t addr;

	if (smsc95xx_wait_for_phy(dev)) {
		printf("SMSC95xx: MII is busy.\n");
		return 1;
	}

	smsc95xx_write_reg(dev, MiiDataReg, val);

	addr = (Smsc95xxPhyId << 11) | (loc << 6) | MiiWrite;
	smsc95xx_write_reg(dev, MiiAddrReg, addr);

	if (smsc95xx_wait_for_phy(dev)) {
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
		smsc95xx_read_reg(dev, E2PCmdReg, &val);
		if (!(val & E2PCmdBusy) || (val & E2PCmdTimeout))
			break;
		udelay(100);
	}

	if (val & (E2PCmdTimeout | E2PCmdBusy)) {
		printf("SMSC95xx: EEPROM read operation timeout.\n");
		if (val & E2PCmdTimeout)
			/* Clear timeout so next operation doesn't see it */
			smsc95xx_write_reg(dev, E2PCmdReg, val);
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
		smsc95xx_write_reg(dev, E2PCmdReg, val);

		if (smsc95xx_wait_eeprom(dev))
			return 1;

		smsc95xx_read_reg(dev, E2PDataReg, &val);
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

static int smsc95xx_restart_autoneg(usbdev_t *dev)
{
	uint32_t bmsr;

	if (smsc95xx_mdio_read(dev, MiiBmsr, &bmsr))
		return 1;

	if (!(bmsr & BmsrAnegCapable)) {
		printf("SMSC95xx: No AutoNeg, falling back to 100baseTX.\n");
		return smsc95xx_mdio_write(dev, MiiBmcr,
					   BmcrSpeedSel | BmcrDuplexMode);
	} else {
		return smsc95xx_mdio_write(dev, MiiBmcr,
					   BmcrAutoNegEnable |
					   BmcrRestartAutoNeg);
	}
}

static int smsc95xx_phy_initialize(usbdev_t *dev)
{
	uint32_t read_buf;

	if (smsc95xx_mdio_write(dev, MiiBmcr, BmcrReset))
		return 1;
	if (smsc95xx_mdio_write(dev, MiiAnar,
				AdvertiseAll | AdvertiseCsma |
				AdvertisePauseCap | AdvertisePauseAsym))
		return 1;

	if (smsc95xx_mdio_read(dev, PhyIntSrc, &read_buf))
		return 1;
	if (smsc95xx_mdio_write(dev, PhyIntMask, PhyIntMaskDefault))
		return 1;
	if (smsc95xx_restart_autoneg(dev))
		return 1;
	printf("SMSC95xx: Done initializing PHY.\n");
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

typedef struct Smsc95xxDev {
	endpoint_t *bulk_in;
	endpoint_t *bulk_out;
	uip_eth_addr mac_addr;
} Smsc95xxDev;

/*
 * The higher-level commands
 */

static int smsc95xx_init(GenericUsbDevice *gen_dev)
{
	Smsc95xxDev smsc_dev;
	usbdev_t *usb_dev = gen_dev->dev;

	printf("SMSC95xx: Initializing\n");

	smsc_dev.bulk_in = &usb_dev->endpoints[1];
	smsc_dev.bulk_out = &usb_dev->endpoints[2];

	if (usb_dev->num_endp < 3 ||
	    smsc_dev.bulk_in->type != BULK ||
	    smsc_dev.bulk_out->type != BULK ||
	    smsc_dev.bulk_in->direction != IN ||
	    smsc_dev.bulk_out->direction != OUT) {
		printf("SMSC95xx: Problem with the endpoints.\n");
		return 1;
	}

	if (smsc95xx_init_reset(usb_dev))
		return 1;

	if (smsc95xx_set_cfg(usb_dev))
		return 1;

	if (smsc95xx_phy_initialize(usb_dev))
		return 1;

	if (smsc95xx_start(usb_dev))
		return 1;

	gen_dev->dev_data = xmalloc(sizeof(smsc_dev));
	memcpy(gen_dev->dev_data, &smsc_dev, sizeof(smsc_dev));

	printf("SMSC95xx: Done initializing\n");
	return 0;
}

static int smsc95xx_ready(NetDevice *net_dev, int *ready)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	uint32_t link_status = 0;
	if (smsc95xx_mdio_read(usb_dev, MiiBmsr, &link_status)) {
		printf("SMSC95xx: Failed to read BMSR.\n");
		return 1;
	}
	*ready = (link_status & BmsrLstatus);
	return 0;
}

static int smsc95xx_send(NetDevice *net_dev, void *buf, uint16_t len)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;
	Smsc95xxDev *smsc_dev = gen_dev->dev_data;

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

	if (usb_dev->controller->bulk(smsc_dev->bulk_out,
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
	Smsc95xxDev *smsc_dev = gen_dev->dev_data;

	uint32_t rx_status;
	uint32_t packet_len;
	static int32_t buf_size = 0;
	static uint8_t msg[RxUrbSize + sizeof(packet_len)];
	static int offset;

	if (offset >= buf_size) {
		offset = 0;
		buf_size = usb_dev->controller->bulk(smsc_dev->bulk_in,
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
	Smsc95xxDev *smsc_dev = gen_dev->dev_data;
	uint32_t addrh;
	uint32_t addrl;

	if (smsc95xx_read_eeprom(usb_dev, EepromMacOffset,
				 sizeof(uip_eth_addr),
				 (uint8_t *)&smsc_dev->mac_addr)) {
		printf("SMSC95xx: Failed to retrieve MAC address.\n");
		return NULL;
	} else {
		addrh = (smsc_dev->mac_addr.addr[5] << 8) |
			(smsc_dev->mac_addr.addr[4]);
		addrl = (smsc_dev->mac_addr.addr[3] << 24) |
			(smsc_dev->mac_addr.addr[2] << 16) |
			(smsc_dev->mac_addr.addr[1] << 8) |
			(smsc_dev->mac_addr.addr[0]);
		smsc95xx_write_reg(usb_dev, AddrHReg, addrh);
		smsc95xx_write_reg(usb_dev, AddrLReg, addrl);
		return &smsc_dev->mac_addr;
	}
}

/*
 * Connecting to the USB and network stacks
 */

static NetDevice smsc95xx_network_device = {
	.ready = &smsc95xx_ready,
	.recv = &smsc95xx_recv,
	.send = &smsc95xx_send,
	.get_mac = &smsc95xx_get_mac,
};

typedef struct Smsc95xxUsbId {
	uint16_t vendor_id;
	uint16_t product_id;
} Smsc95xxUsbId;

static const Smsc95xxUsbId supported_ids[] = {
	/* SMSC 9514 */
	{ 0x0424, 0xec00 },
};

static int smsc95xx_probe(GenericUsbDevice *dev)
{
	int i;
	if (smsc95xx_network_device.dev_data)
		return 0;

	device_descriptor_t *dd = (device_descriptor_t *)dev->dev->descriptor;
	for (i = 0; i < ARRAY_SIZE(supported_ids); i++) {
		if (dd->idVendor == supported_ids[i].vendor_id &&
		    dd->idProduct == supported_ids[i].product_id) {
			smsc95xx_network_device.dev_data = dev;
			if (smsc95xx_init(dev)) {
				return 0;
			} else {
				net_set_device(&smsc95xx_network_device);
				return 1;
			}
		}
	}
	return 0;
}

static void smsc95xx_remove(GenericUsbDevice *dev)
{
	if (net_get_device() == &smsc95xx_network_device)
		net_set_device(NULL);
	free(dev->dev_data);
}

static GenericUsbDriver smsc95xx_usb_driver = {
	.probe = &smsc95xx_probe,
	.remove = &smsc95xx_remove,
};

static int smsc95xx_driver_register(void)
{
	list_insert_after(&smsc95xx_usb_driver.list_node,
			  &generic_usb_drivers);
	return 0;
}

INIT_FUNC(smsc95xx_driver_register);
