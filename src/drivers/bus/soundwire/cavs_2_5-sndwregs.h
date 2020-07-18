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
#define DSP_MEM_SNDW				0x2C000
#define DSP_MEM_SNDW_SNDWLCAP			(DSP_MEM_SNDW + 0x00)
#define DSP_MEM_SNDW_SNDWSC			0x07
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

#endif
