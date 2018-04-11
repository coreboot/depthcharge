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

#ifndef __DRIVERS_NET_R8152_H__
#define __DRIVERS_NET_R8152_H__

#include <stddef.h>

#include "drivers/bus/usb/usb.h"
#include "drivers/net/usb_eth.h"

static const uint16_t Rtl8152ReqtRead = 0xc0;
static const uint16_t Rtl8152ReqtWrite = 0x40;

static const uint16_t Rtl8152ReqGetRegs = 0x05;
static const uint16_t Rtl8152ReqSetRegs = 0x05;

static const uint16_t ByteEnDword = 0xff;
static const uint16_t ByteEnWord = 0x33;
static const uint16_t ByteEnByte = 0x11;
static const uint16_t ByteEnStartMask = 0x0f;
static const uint16_t ByteEnEndMask = 0xf0;

enum {
	RtlVersionUnknown = 0,
	RtlVersion01,
	RtlVersion02,
	RtlVersion03,
	RtlVersion04,
	RtlVersion05,
	RtlVersion06,
	RtlVersion07,
	RtlVersion08,
	RtlVersion09
};

static const uint16_t VersionMask = 0x7cf0;

enum {
	PlaIdr = 0xc000,
	PlaRcr = 0xc010,
	PlaDmyReg0 = 0xc0b0,
	PlaFmc = 0xc0b4,
	PlaMar = 0xcd00,
	PlaLedFeature = 0xdd92,
	PlaBootCtrl = 0xe004,
	PlaEeeCr = 0xe040,
	PlaMacPwrCtrl = 0xe0c0,
	PlaMacPwrCtrl2 = 0xe0ca,
	PlaMacPwrCtrl3 = 0xe0cc,
	PlaMacPwrCtrl4 = 0xe0ce,
	PlaTcr0 = 0xe610,
	PlaRsttally = 0xe800,
	PlaCr = 0xe813,
	PlaCrwecr = 0xe81c,
	PlaConfig34 = 0xe820,
	PlaPhyPwr = 0xe84c,
	PlaMisc1 = 0xe85a,
	PlaOcpGphyBase = 0xe86c
};

/* PlaRcr */
enum {
	RcrApm = 0x00000002,
	RcrAm = 0x00000004,
	RcrAb = 0x00000008
};

/* PlaDmyReg0 */
static const uint8_t EcmAldps = 0x02;

/* PlaFmc */
static const uint16_t FmcFcrMcuEn = 0x0001;

/* PlaLedFeature */
static const uint16_t LedModeMask = 0x0700;

/* PlaBootCtrl */
static const uint16_t AutoloadDone = 0x0002;

/* PlaEeeCr */
enum {
	EeeRxEn = 0x0001,
	EeeTxEn = 0x0002
};

/* PlaRsttally */
static const uint16_t TallyReset = 0x0001;

/* PlaCr */
enum {
	CrRe = 0x08,
	CrTe = 0x04
};

/* PlaCrwecr */
enum {
	CrwecrNormal = 0x00,
	CrwecrConfig = 0xc0
};

/* PlaConfig34 */
enum {
	LinkOffWakeEn = 0x0008
};

/* PlaPhyPwr */
enum {
	PfmPwmSwitch = 0x0040
};

/* PlaMisc1 */
static const uint16_t RxdyGatedEn = 0x0008;

enum {
	UsbCsrDummy1 = 0xb464,
	UsbCsrDummy2 = 0xb466,
	UsbConnectTimer = 0xcbf8,
	UsbMscTimer = 0xcbfc,
	UsbBurstSize = 0xcfc0,
	UsbUsbCtrl = 0xd406,
	UsbLpmCtrl = 0xd41a,
	UsbPowerCut = 0xd80a,
	UsbMisc0 = 0xd81a,
	UsbAfeCtrl2 = 0xd824,
	UsbUpsFlags = 0xd848,
	UsbWdt11Ctrl = 0xe43c
};

/* UsbCsrDummy1 */
static const uint8_t DynamicBurst = 0x01;

/* UsbCsrDummy2 */
static const uint8_t Ep4FullFc = 0x01;

/* UsbUsbCtrl */
enum {
	RxAggDisable = 0x0010,
	RxZeroEn = 0x0080
};

/* UsbLpmCtrl */
static const uint8_t FifoEmpty1fb = 0x30;
static const uint8_t LpmTimer500us = 0x0c;
static const uint8_t RokExitLpm = 0x02;

/* UsbPowerCut */
enum {
	PwrEn = 0x0001,
	Phase2En = 0x0008,
	UpsEn = 1 << 4,
	UpsPrewake = 1 << 5
};

/* UsbMisc0 */
static const uint16_t PcutStatus = 0x0001;

/* UsbAfeCtrl2 */
static const uint16_t SenValMask = 0xf800;
static const uint16_t SenValNormal = 0xa000;
static const uint16_t SelRxidle = 0x0100;

/* UsbUpsFlags */
enum {
	UpsFlagsEnEee = 1 << 20
};

/* UsbWdt11Ctrl */
static const uint16_t Timer11En = 0x0001;

enum {
	McuTypeUsb = 0x0000,
	McuTypePla = 0x0100
};

enum {
	OcpBaseMii = 0xa400,
	OcpPhyStatus = 0xa420,
	OcpNctlCfg = 0xa42c,
	OcpPowerCfg = 0xa430,
	OcpEeeCfg = 0xa432,
	OcpSramAddr = 0xa436,
	OcpSramData = 0xa438,
	OcpDownSpeed = 0xa442,
	OcpEeeAdv = 0xa5d0,
	OcpAdcCfg = 0xbc06
};

/* OcpPhyStatus */
enum {
	PhyStatExtInit = 0x2,
	PhyStatLanOn = 0x3,
	PhyStatPwrdn = 0x5
};

enum {
	PgaReturnEn = 1 << 1
};

/* OcpPowerCfg */
enum {
	EeeClkdivEn = 0x8000,
	En10mPlloff = 0x0001
};

/* OcpEeeCfg */
enum {
	CtapShortEn = 0x0040,
	Eee10En = 0x0010
};

/* OcpDownSpeed */
enum {
	En10mBgoff = 0x0080
};

/* OcpAdcCfg */
enum {
	AdcCfgCkadselL = 0x0100,
	AdcCfgAdcEn = 0x0080,
	AdcCfgEnEmiL = 0x0040
};

enum {
	SramLpfCfg = 0x8012,
	Sram10mAmp1 = 0x8080,
	Sram10mAmp2 = 0x8082,
	SramImpedance = 0x8084
};

enum {
	RxUrbSize = 2048
};

typedef struct R8152Dev {
	UsbEthDevice usb_eth_dev;
	endpoint_t *bulk_in;
	endpoint_t *bulk_out;
	uip_eth_addr mac_addr;
	uint8_t version;
	uint16_t ocp_base;
} R8152Dev;

#endif /* __DRIVERS_NET_R8152_H__ */
