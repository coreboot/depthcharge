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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_NET_SMSC95XX_H__
#define __DRIVERS_NET_SMSC95XX_H__

#include "drivers/net/usb_eth.h"

static const int Smsc95xxPhyId = 1;
static const int EthP8021Q = 0x8100;

enum {
	IntStsReg = 0x8,
	TxCfgReg = 0x10,
	HwCfgReg = 0x14,
	PmCtrlReg = 0x20,
	AfcCfgReg = 0x2c,
	E2PCmdReg = 0x30,
	E2PDataReg = 0x34,
	BurstCapReg = 0x38,
	IntEpCtrlReg = 0x68,
	BulkInDelayReg = 0x6c,
	MacCtrlReg = 0x100,
	AddrHReg = 0x104,
	AddrLReg = 0x108,
	MiiAddrReg = 0x114,
	MiiDataReg = 0x118,
	FlowReg = 0x11c,
	Vlan1Reg = 0x120,
	CoeCtrlReg = 0x130
};

enum {
	UsbVendorReqWrite = 0xa0,
	UsbVendorReqRead = 0xa1
};

enum {
	MiiRead = 0x00,
	MiiBusy = 0x01,
	MiiWrite = 0x02
};

enum {
	PhyIntSrc = 0x1d,
	PhyIntMask = 0x1e
};

enum {
	PhyIntMaskAnegComp = 0x0040,
	PhyIntMaskLinkDown = 0x0010,
	PhyIntMaskDefault = PhyIntMaskAnegComp | PhyIntMaskLinkDown
};

enum {
	E2PCmdRead = 0x00000000,
	E2PCmdAddr = 0x000001ff,
	E2PCmdTimeout = 0x00000400,
	E2PCmdBusy = 0x80000000
};

enum {
	MacCrPrms = 0x00040000,
	MacCrMcpas = 0x00080000,
	MacCrHpfilt = 0x00002000,
	MacCrTxEn = 0x00000008,
	MacCrRxEn = 0x00000004
};

enum {
	TxCoeEn = 0x00010000,
	RxCoeEn = 0x00000001
};

enum {
	HwCfgLrst = 0x00000008,
	HwCfgBir = 0x00001000,
	HwCfgRxdOff = 0x00000600
};

enum {
	RxStsEs = 0x00008000,
	RxStsFl = 0x3fff0000
};

enum {
	TxCmdAFirstSeg = 0x00002000,
	TxCmdALastSeg = 0x00001000
};

enum {
	AfcCfgHi = 0x00ff0000,
	AfcCfgLo = 0x0000ff00,
	AfcCfgBackDur = 0x00000f0,
	AfcCfgFcmult = 0x00000008,
	AfcCfgFcbrd = 0x00000004,
	AfcCfgFcadd = 0x00000002,
	AfcCfgFcany = 0x00000001,
	AfcCfgDefault = 0x00f830a1
};

static const int TxCfgOn = 0x4;
static const int EepromMacOffset = 0x01;
static const int PmCtrlPhyRst = 0x00000010;
static const int BulkInDelayDefault = 0x00002000;
static const int IntEpCtrlPhyInt = 0x00008000;

enum {
	RxUrbSize = 2048
};

typedef struct Smsc95xxDev {
	UsbEthDevice usb_eth_dev;
	endpoint_t *bulk_in;
	endpoint_t *bulk_out;
	uip_eth_addr mac_addr;
} Smsc95xxDev;

#endif /* __DRIVERS_NET_SMSC95XX_H__ */
