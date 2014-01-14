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

#ifndef __DRIVERS_NET_ASIX_H__
#define __DRIVERS_NET_ASIX_H__

enum {
	SramRead = 0x2,
	SramWrite = 0x3,
	SoftSerMgmtCtrlReg = 0x6,
	PhyRegRead = 0x7,
	PhyRegWrite = 0x8,
	SerMgmtStatusReg = 0x9,
	HardSerMgmtCtrlReg = 0xa,
	SromReadReg = 0xb,
	SromWriteReg = 0xc,
	SromWriteEnableReg = 0xd,
	SromWriteDisableReg = 0xe,
	RxCtrlRegRead = 0xf,
	RxCtrlRegWrite = 0x10,
	IpgRegRead = 0x11,
	IpgRegWrite = 0x12,
	NodeIdRead = 0x13,
	NodeIdWrite = 0x14,
	MulticastFilterRegRead = 0x15,
	MulticastFilterRegWrite = 0x16,
	TestRegWrite = 0x17,
	PhyAddrRegRead = 0x19,
	MedStatusRegRead = 0x1a,
	MedModeRegWrite = 0x1b,
	MonModeStatusRegRead = 0x1c,
	MonModeRegWrite = 0x1d,
	GpioStatusRegRead = 0x1e,
	GpioRegWrite = 0x1f,
	SoftResetRegWrite = 0x20,
	SoftPhySelStatusRegRead = 0x21,
	SoftPhySelRegWrite = 0x22
};

enum {
	GpioGpo0En = 0x1 << 0,
	GpioGpo0 = 0x1 << 1,
	GpioGpo1En = 0x1 << 2,
	GpioGpo1 = 0x1 << 3,
	GpioGpo2En = 0x1 << 4,
	GpioGpo2 = 0x1 << 5,
	GpioRse = 0x1 << 7
};

enum {
	SoftResetRr = 0x1 << 0,
	SoftResetRt = 0x1 << 1,
	SoftResetPrte = 0x1 << 2,
	SoftResetPrl = 0x1 << 3,
	SoftResetBz = 0x1 << 4,
	SoftResetIprl = 0x1 << 5,
	SoftResetIppd = 0x1 << 6
};

enum {
	MiiBmcr = 0x0,
	MiiBmsr = 0x1,
	MiiPhyIdr1 = 0x2,
	MiiPhyIdr2 = 0x3,
	MiiAnar = 0x4,
	MiiAnlpar = 0x5,
	MiiAner = 0x6
};

enum {
	BmcrCollisionTest = 0x1 << 7,
	BmcrDuplexMode = 0x1 << 8,
	BmcrRestartAutoNeg = 0x1 << 9,
	BmcrIsolate = 0x1 << 10,
	BmcrPowerDown = 0x1 << 11,
	BmcrAutoNegEnable = 0x1 << 12,
	BmcrSpeedSel = 0x1 << 13,
	BmcrLoopback = 0x1 << 14,
	BmcrReset = 0x1 << 15
};

enum {
	BmsrErcap = 0x1 << 0,
	BmsrJcd = 0x1 << 1,
	BmsrLstatus = 0x1 << 2,
	BmsrAnegCapable = 0x1 << 3,
	BmsrRfault = 0x1 << 4,
	BmsrAnegComplete = 0x1 << 5,
	BmsrEstaten = 0x1 << 8,
	Bmsr100Half2 = 0x1 << 9,
	Bmsr100Full2 = 0x1 << 10,
	Bmsr10Half = 0x1 << 11,
	Bmsr10Full = 0x1 << 12,
	Bmsr100Half = 0x1<< 13,
	Bmsr100Full = 0x1 << 14,
	Bmsr100Base4 = 0x1 << 15
};

enum {
	AdvertiseSelect = 0x1f,
	AdvertiseCsma = 0x1,
	Advertise10Half = 0x1 << 5,
	Advertise10Full = 0x1 << 6,
	Advertise100Half = 0x1 << 7,
	Advertise100Full = 0x1 << 8,

	AdvertiseAll = Advertise10Half | Advertise10Full |
		       Advertise100Half | Advertise100Full
};

enum {
	MediumFd = 0x1 << 1,
	MediumAc = 0x1 << 2,
	MediumRfc = 0x1 << 4,
	MediumTfc = 0x1 << 5,
	MediumJfe = 0x1 << 6,
	MediumPf = 0x1 << 7,
	MediumRe = 0x1 << 8,
	MediumPs = 0x1 << 9,
	MediumSbp = 0x1 << 11,
	MediumSm = 0x1 << 12,
};

static const uint16_t MediumDefault =
	MediumPs | MediumFd | MediumAc | MediumRfc |
	MediumTfc | MediumJfe | MediumRe;

static const uint16_t Ipg0Default = 0x15;
static const uint16_t Ipg1Default = 0x0c;
static const uint16_t Ipg2Default = 0x12;

enum {
	RxCtrlSo = 0x1 << 7,
	RxCtrlAp = 0x1 << 5,
	RxCtrlAm = 0x1 << 4,
	RxCtrlAb = 0x1 << 3,
	RxCtrlAmall = 0x1 << 1,
	RxCtrlPro = 0x1 << 0,
};

static const uint16_t RxCtrlDefault = RxCtrlSo | RxCtrlAb;

enum {
	RxUrbSize = 2048
};

#endif /* __DRIVERS_NET_ASIX_H__ */
