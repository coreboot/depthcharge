// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, Intel Corporation.
 * Copyright 2020 Google LLC.
 */


#ifndef __DRIVERS_BUS_SOUNDWIRE_CAVS_2_5_SNDWREGS_H__
#define __DRIVERS_BUS_SOUNDWIRE_CAVS_2_5_SNDWREGS_H__

/* PCI device and function number for HDA */
#define AUDIO_DEV				0x1f
#define AUDIO_FUNC				3

/* Number of DSP cores */
#define AUDIO_DSP_CORES				4

/* Soundwire link numbers */
#define AUDIO_SNDW_LINK0			0
#define AUDIO_SNDW_LINK1			1
#define AUDIO_SNDW_LINK2			2
#define AUDIO_SNDW_LINK3			3

/* HDA memory space registers */
#define HDA_MEM_GCTL				0x8
#define HDA_MEM_GCTL_CRST			0x1
#define HDA_MEM_PPCTL				0x804
#define HDA_MEM_PPCTL_DSPEN			0x40000000

/* DSP memory space registers */
#define DSP_MEM_ADSPCS				0x04
#define DSP_MEM_SNDW_OFFSETS			0x10000
#define DSP_MEM_ADSPCS_SPA			0x00010000
#define DSP_MEM_ADSPCS_CPA			0x01000000
#define DSP_MEM_ADSPIC2				0x10
#define DSP_MEM_ADSPIC2_SNDW			0x20

/* SoundWire Shim Registers */
#if CONFIG(INTEL_COMMON_SOUNDWIRE_ACE_1_x)
#define DSP_MEM_SNDW				0x38000
#endif
#if CONFIG(INTEL_COMMON_SNDW_CAVS_2_5)
#define DSP_MEM_SNDW				0x2C000
#endif
#define DSP_MEM_SNDW_SNDWLCAP			(DSP_MEM_SNDW + 0x00)
#define DSP_MEM_SNDW_SNDWLCAP_SC		0x07
#define DSP_MEM_SNDW_SNDWLCAP_SC_BIT		0x0
#define DSP_MEM_SNDW_SNDWLCTL			(DSP_MEM_SNDW + 0x04)
#define DSP_MEM_SNDW_SNDWLCTL_SPA		0x1
#define DSP_MEM_SNDW_SNDWLCTL_CPA		0x100
#define DSP_MEM_SNDW_SNDWIPPTR			(DSP_MEM_SNDW + 0x08)
#define DSP_MEM_SNDW_SNDWIPPTR_PRT		0xFFFFF
#define DSP_MEM_SNDW_SNDWxIOCTL(x)		(DSP_MEM_SNDW + (0x60 * (x)) + 0x6C)
#define DSP_MEM_SNDW_SNDWxIOCTL_MIF		0x1
#define DSP_MEM_SNDW_SNDWxACTMCTL(x)		(DSP_MEM_SNDW + (0x60 * (x)) + 0x6E)
#define DSP_MEM_SNDW_SNDWxACTMCTL_DACTQE	0x1

#define SNDW_POLL_TIME_US			4000
#define SNDW_WAIT_PERIOD			1

/* ACE DSP specific */
#define MTL_HFDSSCS				0x1000
#define MTL_HFDSSCS_SPA_MASK			BIT(16)
#define MTL_HFDSSCS_CPA_MASK			BIT(24)
#define MTL_HFPWRCTL				0x1d18
#define MTL_HFPWRSTS				0x1d1c
#define MTL_HFPWRCTL_WPDSPHPxPG(x)		BIT(x)
#define MTL_HFPWRCTL_WPIOxPG(x)		BIT((x) + 8)
#define MTL_HFPWRCTL_WPHP0IO0_PG		\
	(MTL_HFPWRCTL_WPDSPHPxPG(0) | MTL_HFPWRCTL_WPIOxPG(1))
#define MTL_DSP2C0_CTL				0x178d04
#define MTL_DSP2C0_OSEL_SHIFT			24
#define MTL_DSP2C0_OSEL_MASK			GENMASK(25, 24)
#define MTL_DSP2C0_OSEL_HOST(x)		\
	(((x) & ~MTL_DSP2C0_OSEL_MASK) | 0x2 << MTL_DSP2C0_OSEL_SHIFT)
#define MTL_DSP2C0_CTL_SPA_MASK		BIT(0)
#define MTL_DSP2C0_CTL_CPA_MASK		BIT(8)

#endif
