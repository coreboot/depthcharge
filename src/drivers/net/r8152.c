/*
 * Copyright 2018 Google Inc.
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

#include <usb/usb.h>

#include "base/init_funcs.h"
#include "drivers/net/mii.h"
#include "drivers/net/r8152.h"
#include "drivers/net/usb_eth.h"

static R8152Dev r8152_dev;

/*
 * The lower-level utilities
 */

static int read_registers(usbdev_t *dev, uint16_t value, uint16_t index,
			  uint16_t size, void *data)
{
	dev_req_t dev_req;
	dev_req.bmRequestType = Rtl8152ReqtRead;
	dev_req.bRequest = Rtl8152ReqGetRegs;
	dev_req.wValue = value;
	dev_req.wIndex = index;
	dev_req.wLength = size;

	return (dev->controller->control(dev, IN, sizeof(dev_req), &dev_req,
					 size, (uint8_t *)data)
		< 0);
}

static int write_registers(usbdev_t *dev, uint16_t value, uint16_t index,
			   uint16_t size, void *data)
{
	dev_req_t dev_req;
	dev_req.bmRequestType = Rtl8152ReqtWrite;
	dev_req.bRequest = Rtl8152ReqSetRegs;
	dev_req.wValue = value;
	dev_req.wIndex = index;
	dev_req.wLength = size;

	return (dev->controller->control(dev, OUT, sizeof(dev_req), &dev_req,
					 size, (uint8_t *)data)
		< 0);
}

static int ocp_read(usbdev_t *dev, uint16_t index, uint16_t size, void *data,
		    uint16_t type)
{
	uint16_t limit = 64;
	while (size) {
		if (size < limit)
			limit = size;

		if (read_registers(dev, index, type, limit, data))
			return 1;

		index += limit;
		data += limit;
		size -= limit;
	}
	return 0;
}

static int ocp_write(usbdev_t *dev, uint16_t index, uint16_t byteen,
		     uint16_t size, void *data, uint16_t type)
{
	uint16_t limit = 512;
	uint16_t byteen_start, byteen_end;

	byteen_start = byteen & ByteEnStartMask;
	byteen_end = byteen & ByteEnEndMask;

	if (write_registers(dev, index,
			    type | byteen_start | (byteen_start << 4), 4, data))
		return 1;

	index += 4;
	data += 4;
	size -= 4;

	if (size) {
		size -= 4;
		while (size) {
			if (size < limit)
				limit = size;

			if (write_registers(dev, index, type | ByteEnDword,
					    limit, data))
				return 1;

			index += limit;
			data += limit;
			size -= limit;
		}
		if (write_registers(dev, index,
				    type | byteen_end | (byteen_end >> 4), 4,
				    data))
			return 1;
	}
	return 0;
}

static int ocp_read_dword(usbdev_t *dev, uint16_t type, uint16_t index,
			  uint32_t *val)
{
	if (ocp_read(dev, index, sizeof(*val), val, type))
		return 1;
	*val = le32toh(*val);
	return 0;
}

static int ocp_write_dword(usbdev_t *dev, uint16_t type, uint16_t index,
			   uint32_t val)
{
	val = htole32(val);
	return ocp_write(dev, index, ByteEnDword, sizeof(val), &val, type);
}

static int ocp_dword_clrsetbits(usbdev_t *dev, uint16_t type, uint16_t index,
				uint32_t clr_mask, uint32_t set_mask)
{
	uint32_t data;
	if (ocp_read_dword(dev, type, index, &data))
		return 1;
	data = (data & ~clr_mask) | set_mask;
	return ocp_write_dword(dev, type, index, data);
}

static int ocp_dword_setbits(usbdev_t *dev, uint16_t type, uint16_t index,
			     uint32_t set_mask)
{
	return ocp_dword_clrsetbits(dev, type, index, 0, set_mask);
}

static int ocp_dword_clearbits(usbdev_t *dev, uint16_t type, uint16_t index,
			       uint32_t clr_mask)
{
	return ocp_dword_clrsetbits(dev, type, index, clr_mask, 0);
}

static int ocp_read_word(usbdev_t *dev, uint16_t type, uint16_t index,
			 uint16_t *val)
{
	uint32_t data;
	uint8_t shift = index & 2;

	index = ALIGN_DOWN(index, 4);
	if (ocp_read(dev, index, sizeof(data), &data,
		     type | (ByteEnWord << shift)))
		return 1;
	data = le32toh(data);
	data >>= (shift * 8);
	*val = data & 0xffff;

	return 0;
}

static int ocp_write_word(usbdev_t *dev, uint16_t type, uint16_t index,
			  uint16_t val)
{
	uint32_t tmp = val;
	uint8_t shift = index & 2;

	tmp <<= (shift * 8);
	index = ALIGN_DOWN(index, 4);
	tmp = htole32(tmp);

	return ocp_write(dev, index, ByteEnWord << shift, sizeof(tmp), &tmp,
			 type);
}

static int ocp_word_clrsetbits(usbdev_t *dev, uint16_t type, uint16_t index,
			       uint16_t clr_mask, uint16_t set_mask)
{
	uint16_t data;
	if (ocp_read_word(dev, type, index, &data))
		return 1;
	data = (data & ~clr_mask) | set_mask;
	return ocp_write_word(dev, type, index, data);
}

static int ocp_word_setbits(usbdev_t *dev, uint16_t type, uint16_t index,
			    uint16_t set_mask)
{
	return ocp_word_clrsetbits(dev, type, index, 0, set_mask);
}

static int ocp_word_clearbits(usbdev_t *dev, uint16_t type, uint16_t index,
			      uint16_t clr_mask)
{
	return ocp_word_clrsetbits(dev, type, index, clr_mask, 0);
}

static int ocp_read_byte(usbdev_t *dev, uint16_t type, uint16_t index,
			 uint8_t *val)
{
	uint32_t data;
	uint8_t shift = index & 3;

	index = ALIGN_DOWN(index, 4);
	if (ocp_read(dev, index, sizeof(data), &data, type))
		return 1;
	data = le32toh(data);
	data >>= (shift * 8);
	*val = data & 0xff;

	return 0;
}

static int ocp_write_byte(usbdev_t *dev, uint16_t type, uint16_t index,
			  uint8_t val)
{
	uint32_t tmp = val;
	uint8_t shift = index & 3;

	tmp <<= (shift * 8);
	index = ALIGN_DOWN(index, 4);
	tmp = htole32(tmp);

	return ocp_write(dev, index, ByteEnByte << shift, sizeof(tmp), &tmp,
			 type);
}

static int ocp_byte_clrsetbits(usbdev_t *dev, uint16_t type, uint16_t index,
			       uint8_t clr_mask, uint8_t set_mask)
{
	uint8_t data;
	if (ocp_read_byte(dev, type, index, &data))
		return 1;
	data = (data & ~clr_mask) | set_mask;
	return ocp_write_byte(dev, type, index, data);
}

static int ocp_byte_setbits(usbdev_t *dev, uint16_t type, uint16_t index,
			    uint8_t set_mask)
{
	return ocp_byte_clrsetbits(dev, type, index, 0, set_mask);
}

static int ocp_byte_clearbits(usbdev_t *dev, uint16_t type, uint16_t index,
			      uint8_t clr_mask)
{
	return ocp_byte_clrsetbits(dev, type, index, clr_mask, 0);
}

static int ocp_reg_read(usbdev_t *dev, uint16_t addr, uint16_t *val)
{
	uint16_t ocp_base, ocp_index;

	ocp_base = addr & 0xf000;
	if (ocp_base != r8152_dev.ocp_base) {
		if (ocp_write_word(dev, McuTypePla, PlaOcpGphyBase, ocp_base))
			return 1;
		r8152_dev.ocp_base = ocp_base;
	}

	ocp_index = (addr & 0x0fff) | 0xb000;
	return ocp_read_word(dev, McuTypePla, ocp_index, val);
}

static int ocp_reg_write(usbdev_t *dev, uint16_t addr, uint16_t val)
{
	uint16_t ocp_base, ocp_index;

	ocp_base = addr & 0xf000;
	if (ocp_base != r8152_dev.ocp_base) {
		if (ocp_write_word(dev, McuTypePla, PlaOcpGphyBase, ocp_base))
			return 1;
		r8152_dev.ocp_base = ocp_base;
	}

	ocp_index = (addr & 0x0fff) | 0xb000;
	return ocp_write_word(dev, McuTypePla, ocp_index, val);
}

static int ocp_reg_clrsetbits(usbdev_t *dev, uint16_t addr, uint16_t clr_mask,
			      uint16_t set_mask)
{
	uint16_t data;
	if (ocp_reg_read(dev, addr, &data))
		return 1;
	data = (data & ~clr_mask) | set_mask;
	return ocp_reg_write(dev, addr, data);
}

static int ocp_reg_setbits(usbdev_t *dev, uint16_t addr, uint16_t set_mask)
{
	return ocp_reg_clrsetbits(dev, addr, 0, set_mask);
}

static int ocp_reg_clearbits(usbdev_t *dev, uint16_t addr, uint16_t clr_mask)
{
	return ocp_reg_clrsetbits(dev, addr, clr_mask, 0);
}

static int get_version(usbdev_t *dev, uint8_t *version)
{
	uint32_t ocp_data;

	if (ocp_read_dword(dev, McuTypePla, PlaTcr0, &ocp_data)) {
		printf("Failed to read version.\n");
		return 1;
	}

	ocp_data = (ocp_data >> 16) & VersionMask;

	switch (ocp_data) {
	case 0x4c00:
		*version = RtlVersion01;
		break;
	case 0x4c10:
		*version = RtlVersion02;
		break;
	case 0x5c00:
		*version = RtlVersion03;
		break;
	case 0x5c10:
		*version = RtlVersion04;
		break;
	case 0x5c20:
		*version = RtlVersion05;
		break;
	case 0x5c30:
		*version = RtlVersion06;
		break;
	case 0x4800:
		*version = RtlVersion07;
		break;
	case 0x6000:
		*version = RtlVersion08;
		break;
	case 0x6010:
		*version = RtlVersion09;
		break;
	default:
		*version = RtlVersionUnknown;
		printf("Unknown version %04x\n", ocp_data);
		return 1;
	}
	printf("Version %d (ocp_data = %04x)\n", *version, ocp_data);
	return 0;
}

static int r8152_mdio_read(usbdev_t *dev, uint8_t loc, uint16_t *val)
{
	return ocp_reg_read(dev, OcpBaseMii + loc * 2, val);
}

static int r8152_mdio_write(usbdev_t *dev, uint8_t loc, uint16_t val)
{
	return ocp_reg_write(dev, OcpBaseMii + loc * 2, val);
}

static int sram_write(usbdev_t *dev, uint16_t addr, uint16_t data)
{
	if (ocp_reg_write(dev, OcpSramAddr, addr))
		return 1;
	if (ocp_reg_write(dev, OcpSramData, data))
		return 1;
	return 0;
}

static int r8153_wait_autoload_done(usbdev_t *dev)
{
	int i;
	uint16_t data;

	for (i = 0; i < 500; i++) {
		if (ocp_read_word(dev, McuTypePla, PlaBootCtrl, &data))
			return 1;
		if (data & AutoloadDone)
			return 0;
		mdelay(20);
	}

	return 1;
}

static int r8153_wait_for_phy_status(usbdev_t *dev, uint8_t desired)
{
	int i;
	uint16_t val;

	for (i = 0; i < 500; i++) {
		if (ocp_reg_read(dev, OcpPhyStatus, &val))
			return 1;
		val &= 0x07;
		if (desired) {
			if (val == desired)
				return 0;
		} else if (val == PhyStatLanOn || val == PhyStatPwrdn
			   || val == PhyStatExtInit) {
			return 0;
		}
		mdelay(20);
	}

	return 1;
}

static int r8153_power_cut_disable(usbdev_t *dev)
{
	if (ocp_word_clearbits(dev, McuTypeUsb, UsbPowerCut, PwrEn | Phase2En))
		return 1;

	if (ocp_word_clearbits(dev, McuTypeUsb, UsbMisc0, PcutStatus))
		return 1;

	return 0;
}

static int rtl_tally_reset(usbdev_t *dev)
{
	return ocp_word_setbits(dev, McuTypePla, PlaRsttally, TallyReset);
}

static int r8153_eee_disable(usbdev_t *dev)
{
	if (ocp_word_clearbits(dev, McuTypePla, PlaEeeCr, EeeRxEn | EeeTxEn))
		return 1;

	if (ocp_reg_clearbits(dev, OcpEeeCfg, Eee10En))
		return 1;

	return 0;
}

static int r8153_hw_phy_cfg(usbdev_t *dev)
{
	if (r8153_eee_disable(dev))
		return 1;

	if (ocp_reg_write(dev, OcpEeeAdv, 0))
		return 1;

	if (r8152_dev.version == RtlVersion03) {
		if (ocp_reg_clearbits(dev, OcpEeeCfg, CtapShortEn))
			return 1;
	}

	if (ocp_reg_setbits(dev, OcpPowerCfg, EeeClkdivEn))
		return 1;

	if (ocp_reg_setbits(dev, OcpDownSpeed, En10mBgoff))
		return 1;

	if (ocp_reg_setbits(dev, OcpPowerCfg, En10mPlloff))
		return 1;

	if (sram_write(dev, SramImpedance, 0x0b13))
		return 1;

	if (ocp_word_setbits(dev, McuTypePla, PlaPhyPwr, PfmPwmSwitch))
		return 1;

	if (sram_write(dev, SramLpfCfg, 0xf70f))
		return 1;

	if (sram_write(dev, Sram10mAmp1, 0x00af))
		return 1;
	if (sram_write(dev, Sram10mAmp2, 0x0208))
		return 1;

	return 0;
}

static int r8153_set_rx_mode(usbdev_t *dev)
{
	uint32_t tmp[2] = { 0xffffffff, 0xffffffff };

	if (ocp_write(dev, PlaMar, ByteEnDword, sizeof(tmp), tmp, McuTypePla))
		return 1;

	if (ocp_dword_setbits(dev, McuTypePla, PlaRcr, RcrAb | RcrApm | RcrAm))
		return 1;

	return 0;
}

static int r8152b_reset_packet_filter(usbdev_t *dev)
{
	if (ocp_word_clearbits(dev, McuTypePla, PlaFmc, FmcFcrMcuEn))
		return 1;
	if (ocp_word_setbits(dev, McuTypePla, PlaFmc, FmcFcrMcuEn))
		return 1;

	return 0;
}

static int rxdy_gated_disable(usbdev_t *dev)
{
	return ocp_word_clearbits(dev, McuTypePla, PlaMisc1, RxdyGatedEn);
}

static int rtl_enable(usbdev_t *dev)
{
	if (r8152b_reset_packet_filter(dev))
		return 1;

	if (ocp_byte_setbits(dev, McuTypePla, PlaCr, CrRe | CrTe))
		return 1;

	if (rxdy_gated_disable(dev))
		return 1;
	if (r8153_set_rx_mode(dev))
		return 1;

	return 0;
}

static int r8153_mac_clk_speed_disable(usbdev_t *dev)
{
	if (ocp_write_word(dev, McuTypePla, PlaMacPwrCtrl, 0))
		return 1;
	if (ocp_write_word(dev, McuTypePla, PlaMacPwrCtrl2, 0))
		return 1;
	if (ocp_write_word(dev, McuTypePla, PlaMacPwrCtrl3, 0))
		return 1;
	if (ocp_write_word(dev, McuTypePla, PlaMacPwrCtrl4, 0))
		return 1;
	return 0;
}

static int r8153_init(usbdev_t *dev)
{
	uint16_t data;

	if (r8153_wait_autoload_done(dev))
		return 1;

	if (r8153_wait_for_phy_status(dev, 0))
		return 1;

	if (r8152_dev.version == RtlVersion03
	    || r8152_dev.version == RtlVersion04
	    || r8152_dev.version == RtlVersion05) {
		if (ocp_reg_write(dev, OcpAdcCfg,
				  AdcCfgCkadselL | AdcCfgAdcEn | AdcCfgEnEmiL))
			return 1;
	}

	if (r8152_mdio_read(dev, MiiBmcr, &data))
		return 1;
	data &= ~BmcrPowerDown;
	if (r8152_mdio_write(dev, MiiBmcr, data))
		return 1;

	if (r8153_wait_for_phy_status(dev, PhyStatLanOn))
		return 1;

	if (r8152_dev.version == RtlVersion05) {
		if (ocp_byte_clearbits(dev, McuTypePla, PlaDmyReg0, EcmAldps))
			return 1;
	}
	if (r8152_dev.version == RtlVersion05
	    || r8152_dev.version == RtlVersion06) {
		if (ocp_read_word(dev, McuTypeUsb, UsbBurstSize, &data))
			return 1;
		if ((data ? ocp_byte_setbits : ocp_byte_clearbits)(
			    dev, McuTypeUsb, UsbCsrDummy1, DynamicBurst))
			return 1;
	}

	if (ocp_byte_setbits(dev, McuTypeUsb, UsbCsrDummy2, Ep4FullFc))
		return 1;

	if (ocp_word_clearbits(dev, McuTypeUsb, UsbWdt11Ctrl, Timer11En))
		return 1;

	if (ocp_word_clearbits(dev, McuTypePla, PlaLedFeature, LedModeMask))
		return 1;

	if (ocp_write_byte(dev, McuTypeUsb, UsbLpmCtrl,
			   FifoEmpty1fb | RokExitLpm | LpmTimer500us))
		return 1;

	if (ocp_read_word(dev, McuTypeUsb, UsbAfeCtrl2, &data))
		return 1;
	data &= ~SenValMask;
	data |= SenValNormal | SelRxidle;
	if (ocp_write_word(dev, McuTypeUsb, UsbAfeCtrl2, data))
		return 1;

	if (ocp_write_word(dev, McuTypeUsb, UsbConnectTimer, 0x0001))
		return 1;

	if (r8153_power_cut_disable(dev))
		return 1;

	if (r8153_mac_clk_speed_disable(dev))
		return 1;

	if (ocp_word_clearbits(dev, McuTypeUsb, UsbUsbCtrl,
			       RxAggDisable | RxZeroEn))
		return 1;

	if (rtl_tally_reset(dev))
		return 1;

	if (rtl_enable(dev))
		return 1;

	if (r8153_hw_phy_cfg(dev))
		return 1;

	return 0;
}

static int r8153b_power_cut_disable(usbdev_t *dev)
{
	if (ocp_word_clearbits(dev, McuTypeUsb, UsbPowerCut, PwrEn))
		return 1;

	if (ocp_word_clearbits(dev, McuTypeUsb, UsbMisc0, PcutStatus))
		return 1;

	return 0;
}

static int r8153b_ups_disable(usbdev_t *dev)
{
	if (ocp_byte_clearbits(dev, McuTypeUsb, UsbPowerCut,
			       UpsEn | UpsPrewake))
		return 1;

	if (ocp_byte_clearbits(dev, McuTypeUsb, 0xcfff, 1 << 0))
		return 1;

	if (ocp_byte_clearbits(dev, McuTypeUsb, UsbMisc0, PcutStatus))
		return 1;

	return 0;
}

static int r8153b_queue_wake_disable(usbdev_t *dev)
{
	if (ocp_byte_clearbits(dev, McuTypePla, 0xd38a, 1 << 0))
		return 1;

	if (ocp_byte_clearbits(dev, McuTypePla, 0xd38c, 1 << 0))
		return 1;

	return 0;
}

static int rtl_runtime_suspend_disable(usbdev_t *dev)
{
	if (ocp_write_byte(dev, McuTypePla, PlaCrwecr, CrwecrConfig))
		return 1;

	if (ocp_word_clearbits(dev, McuTypePla, PlaConfig34, LinkOffWakeEn))
		return 1;

	if (ocp_write_byte(dev, McuTypePla, PlaCrwecr, CrwecrNormal))
		return 1;

	return 0;
}

static int r8153b_eee_disable(usbdev_t *dev)
{
	if (r8153_eee_disable(dev))
		return 1;

	if (ocp_dword_clearbits(dev, McuTypeUsb, UsbUpsFlags, UpsFlagsEnEee))
		return 1;

	return 0;
}

static int r8153b_hw_phy_cfg(usbdev_t *dev)
{
	if (r8153b_eee_disable(dev))
		return 1;

	if (ocp_reg_write(dev, OcpEeeAdv, 0))
		return 1;

	if (ocp_reg_setbits(dev, OcpNctlCfg, PgaReturnEn))
		return 1;

	if (ocp_word_setbits(dev, McuTypePla, PlaPhyPwr, PfmPwmSwitch))
		return 1;

	return 0;
}

static int r8153b_init(usbdev_t *dev)
{
	uint16_t data;

	if (r8153_wait_autoload_done(dev))
		return 1;

	if (r8153_wait_for_phy_status(dev, 0))
		return 1;

	if (r8152_mdio_read(dev, MiiBmcr, &data))
		return 1;
	data &= ~BmcrPowerDown;
	if (r8152_mdio_write(dev, MiiBmcr, data))
		return 1;

	if (r8153_wait_for_phy_status(dev, PhyStatLanOn))
		return 1;

	if (ocp_write_word(dev, McuTypeUsb, UsbMscTimer, 0x0fff))
		return 1;

	if (r8153b_power_cut_disable(dev))
		return 1;

	if (r8153b_ups_disable(dev))
		return 1;

	if (r8153b_queue_wake_disable(dev))
		return 1;

	if (rtl_runtime_suspend_disable(dev))
		return 1;

	if (ocp_word_clearbits(dev, McuTypeUsb, UsbUsbCtrl,
			       RxAggDisable | RxZeroEn))
		return 1;

	if (rtl_tally_reset(dev))
		return 1;

	if (rtl_enable(dev))
		return 1;

	if (r8153b_hw_phy_cfg(dev))
		return 1;

	return 0;
}

/*
 * The higher-level commands
 */

static int rtl8152_init(GenericUsbDevice *gen_dev)
{
	usbdev_t *usb_dev = gen_dev->dev;

	printf("R8152: Initializing\n");

	if (usb_eth_init_endpoints(usb_dev, &r8152_dev.bulk_in, 1,
				   &r8152_dev.bulk_out, 2)) {
		printf("R8152: Problem with the endpoints.\n");
		return 1;
	}

	if (get_version(usb_dev, &r8152_dev.version)) {
		printf("R8152: Problem when getting version.\n");
		return 1;
	}

	// TODO(pihsun): Support RTL8152 and RTL8153b.
	switch (r8152_dev.version) {
	case RtlVersion01:
	case RtlVersion02:
	case RtlVersion07:
		printf("RTL8152 is not supported yet.");
		return 1;
	case RtlVersion03:
	case RtlVersion04:
	case RtlVersion05:
	case RtlVersion06:
		if (r8153_init(usb_dev))
			return 1;
		break;
	case RtlVersion08:
	case RtlVersion09:
		if (r8153b_init(usb_dev))
			return 1;
		break;
	}

	printf("R8152: Done initializing\n");
	return 0;
}

static int rtl8152_mdio_read(NetDevice *dev, uint8_t loc, uint16_t *val)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	return r8152_mdio_read(usb_dev, loc, val);
}

static int rtl8152_mdio_write(NetDevice *dev, uint8_t loc, uint16_t val)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	return r8152_mdio_write(usb_dev, loc, val);
}

enum { TxFs = 1u << 31, TxLs = 1u << 30 };

static int rtl8152_send(NetDevice *net_dev, void *buf, uint16_t len)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;
	uint32_t tx_desc[2];

	static uint8_t msg[CONFIG_UIP_BUFSIZE + sizeof(tx_desc)];

	if (len > sizeof(msg) - sizeof(tx_desc)) {
		printf("R8152: Packet size %u is too large.\n", len);
		return 1;
	}
	tx_desc[0] = htole32(len | TxFs | TxLs);
	tx_desc[1] = htole32(0);
	memcpy(msg, tx_desc, sizeof(tx_desc));
	memcpy(msg + sizeof(tx_desc), buf, len);

	return (usb_dev->controller->bulk(r8152_dev.bulk_out,
					  len + sizeof(tx_desc), msg, 0)
		< 0);
}

static int rtl8152_recv(NetDevice *net_dev, void *buf, uint16_t *len,
			int maxlen)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	uint32_t rx_desc[6];
	int32_t packet_len;
	static int32_t buf_size = 0;
	static uint8_t msg[RxUrbSize + sizeof(rx_desc)];
	static int32_t offset;

	if (offset >= buf_size) {
		offset = 0;
		buf_size = usb_dev->controller->bulk(r8152_dev.bulk_in,
						     RxUrbSize, msg, 0);
		if (buf_size < 0) {
			printf("R8152: Bulk read error %#x\n", buf_size);
			return 1;
		}
	}

	if (!buf_size) {
		*len = 0;
		return 0;
	}

	memcpy(&rx_desc, msg + offset, sizeof(rx_desc));
	packet_len = le32toh(rx_desc[0]) & 0x7fff;
	packet_len -= 4;

	*len = packet_len;
	if ((packet_len > maxlen) || (offset + packet_len > buf_size)) {
		buf_size = 0;
		offset = 0;
		printf("R8152: Packet is too large.\n");
		return 1;
	}

	memcpy(buf, msg + offset + sizeof(rx_desc), packet_len);
	offset += sizeof(rx_desc) + packet_len + 4;
	offset = ALIGN_UP(offset, 8);

	return 0;
}

static const uip_eth_addr *rtl8152_get_mac(NetDevice *net_dev)
{
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)net_dev->dev_data;
	usbdev_t *usb_dev = gen_dev->dev;

	uint8_t data[8];

	if (ocp_read(usb_dev, PlaIdr, sizeof(data), data, McuTypePla))
		return NULL;

	memcpy(&r8152_dev.mac_addr, data, sizeof(uip_eth_addr));
	return &r8152_dev.mac_addr;
}


/*
 * Code to plug the driver into the USB and network stacks.
 */

/* Supported usb ethernet dongles. */
static const UsbEthId r8152_supported_ids[] = {
	/* Realtek */
	{ 0x0bda, 0x8050 },
	{ 0x0bda, 0x8152 },
	{ 0x0bda, 0x8153 },
	/* Microsoft */
	{ 0x045e, 0x07ab },
	{ 0x045e, 0x07c6 },
	/* Samsung */
	{ 0x04e8, 0xa101 },
	/* Lenovo */
	{ 0x17ef, 0x304f },
	{ 0x17ef, 0x3052 },
	{ 0x17ef, 0x3054 },
	{ 0x17ef, 0x3057 },
	{ 0x17ef, 0x3062 },
	{ 0x17ef, 0x3069 },
	{ 0x17ef, 0x7205 },
	{ 0x17ef, 0x720a },
	{ 0x17ef, 0x720b },
	{ 0x17ef, 0x720c },
	{ 0x17ef, 0x7214 },
	/* Linksys */
	{ 0x13b1, 0x0041 },
	/* Nvidia */
	{ 0x0955, 0x09ff },
	/* TP-Link */
	{ 0x2357, 0x0601 },
};

static R8152Dev r8152_dev = {
	.usb_eth_dev = {
		.init = &rtl8152_init,
		.net_dev = {
			.ready = &mii_ready,
			.recv = &rtl8152_recv,
			.send = &rtl8152_send,
			.get_mac = &rtl8152_get_mac,
			.mdio_read = &rtl8152_mdio_read,
			.mdio_write = &rtl8152_mdio_write,
		},
		.supported_ids = r8152_supported_ids,
		.num_supported_ids = ARRAY_SIZE(r8152_supported_ids),
	},
};

static int r8152_driver_register(void)
{
	list_insert_after(&r8152_dev.usb_eth_dev.list_node, &usb_eth_drivers);
	return 0;
}

INIT_FUNC(r8152_driver_register);
