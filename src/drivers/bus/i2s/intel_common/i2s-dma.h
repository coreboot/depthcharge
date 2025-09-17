/*
 * Copyright (C) 2025 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_BUS_I2S_INTEL_COMMON_I2S_DMA_H__
#define __DRIVERS_BUS_I2S_INTEL_COMMON_I2S_DMA_H__

#include <stdint.h>

#include "drivers/bus/i2s/intel_common/i2s.h"
#include "drivers/sound/i2s.h"

/* DMA Stream structure for audio playback */
typedef struct {
	uint8_t  stream_id;         /* Stream ID (0-based) */
	uint32_t data_size;         /* Total buffer size in bytes */
	uint16_t bdl_entries;       /* Number of BDL entries */
	uint64_t bdl_address;       /* Physical address of BDL */
	uint16_t word_length;       /* Bits per sample (16, 24, 32) */
	uint8_t  num_of_channels;   /* Number of channels */
} AUDIO_STREAM;

/* HD Audio Buffer Descriptor List Entry (BDLE) structure */
typedef struct {
	uint64_t address;           /* 64-bit address, 128-byte aligned */
	uint32_t length;            /* Buffer length in bytes */
	uint32_t ioc      :  1;     /* Interrupt on Completion */
	uint32_t reserved : 31;     /* Reserved; must be 0 */
} __packed BUFFER_DESCRIPTOR_LIST_ENTRY;

/* Intel HDA/ACE DMA constants */
#define MAX_BDLE_BYTES				(64 * KiB)	/* Typical per-BDLE max size */

#define R_OSDxSTS(x)				(0x1E3 + (x * 0x20))
#define B_OSDxSTS_BCIS				BIT(2)

#define R_OSDxCTL_B0(x)				(0x1E0 + (x * 0x20))
#define B_OSDxCTL_B0_SRST			BIT(0)
#define B_OSDxCTL_B0_RUN			BIT(1)
#define B_OSDxCTL_B0_IOCE			BIT(2)
#define B_OSDxCTL_B0_FEIE			BIT(3)
#define B_OSDxCTL_B0_DEIE			BIT(4)

#define R_OSDxCTL_B2(x)				(0x1E2 + (x * 0x20))
#define N_OSDxCTL_B2_STRM			4

#define R_OSDxFMT(x)				(0x1F2 + (x * 0x20))
#define N_OSDxFMT_CHAN				0
#define V_OSDxFMT_CHAN_2CH			0x1
#define N_OSDxFMT_BITS				4
#define V_OSDxFMT_BITS_16_IN_16			0x1
#define V_OSDxFMT_BITS_24_IN_32			0x3
#define V_OSDxFMT_BITS_32_IN_32			0x4
#define N_OSDxFMT_DIV				8
#define V_OSDxFMT_DIV_BY_1			0x0
#define N_OSDxFMT_MULT				11
#define V_OSDxFMT_MULT_48_44_1_KHZ		0x0
#define N_OSDxFMT_BASE				14
#define V_OSDxFMT_BASE_48_KHZ			0x0
#define V_OSDxFMT_BASE_44_1_KHZ			0x1

#define R_OSDxCBL(x)				(0x1E8 + (x * 0x20))
#define R_OSDxLVI(x)				(0x1EC + (x * 0x20))

#define R_OSDxBDLPLBA(x)			(0x1F8 + (x * 0x20))
#define R_OSDxBDLPUBA(x)			(0x1FC + (x * 0x20))

#define R_OSDxLPIB(x)				(0x1E4 + (x * 0x20))

#define R_OPPHCxLLPL(x)				(0x8C0 + (x * 0x10))
#define R_OPPHCxLLPU(x)				(0x8C4 + (x * 0x10))
#define R_OPPHCxLDPL(x)				(0x8C8 + (x * 0x10))
#define R_OPPHCxLDPU(x)				(0x8CC + (x * 0x10))

#define R_OSDxFIFOS(x)				(0x1F0 + (x * 0x20))
#define R_OSDxMAXFIFOS(x)			(0x764 + (x * 0x8))

/* Stream ID Validation Registers */
#define R_HDALOSIDV				0xC48   /* HD Audio Link Output Stream ID Validation */
#define R_IDALOSIDV				0xC88   /* Intel Display Audio Link Output Stream ID Validation */
#define R_I2SLOSIDV				0xD08   /* I2S Link Output Stream ID Validation */
#define R_DMICLOSIDV				0xCC8   /* Digital Mic Link Output Stream ID Validation */
#define R_UAOLLOSIDV				0xD48   /* USB Audio Offload Link Output Stream ID Validation */
#define R_SNDWLOSIDV				0xD88   /* SoundWire Link Output Stream ID Validation */

#define R_OSDxDPIB(x)				(0x11E4 + (x * 0x20))

/* I2S PCM Stream 1 stream capabilities Register */
#define I2S1PCMSCAP_OFFSET			0x29010

/* I2S1PCMSxCM: Generic PCM Stream Channel Map Registers (streams 0-15) */
#define I2S1PCMS_BASE_OFFSET			0x29016  /* Base offset for stream 0 */
#define I2S1PCMSxCM_OFFSET(x)			(I2S1PCMS_BASE_OFFSET + ((x) * 0x4))

/* I2S1PCMSxCM bit fields (same for all streams) */
#define I2S1PCMSxCM_LCHAN_SHIFT			0
#define I2S1PCMSxCM_LCHAN_MASK			0xF
#define I2S1PCMSxCM_HCHAN_SHIFT			4
#define I2S1PCMSxCM_HCHAN_MASK			0xF
#define I2S1PCMSxCM_STRM_SHIFT			8
#define I2S1PCMSxCM_STRM_MASK			0x3F

/* Helper macros for I2S1PCMSxCM */
#define I2S1PCMSxCM_reg(regbit, value)		(((value) & I2S1PCMSxCM_##regbit##_MASK) << I2S1PCMSxCM_##regbit##_SHIFT)

/* 32-bit Processing Pipe Control Register */
DEFINE_REG(PPCTL, 0x0804)

/* Field definitions for PPCTL Register (32-bit) */
DEFINE_FIELD(PPCTL, PROCEN, 0xFFFFF, 0) /* Processing Enable bits [19:0] */

#define set_PPCTL_reg(reg_pointer, regbit)				\
	write_PPCTL(read_PPCTL(reg_pointer)			\
	| (PPCTL_##regbit##_MASK << PPCTL_##regbit##_SHIFT),	\
	reg_pointer);

#define clear_PPCTL_reg(reg_pointer, regbit)				\
	write_PPCTL((read_PPCTL(reg_pointer)			\
	& (~((PPCTL_##regbit##_MASK << PPCTL_##regbit##_SHIFT)))),  \
	reg_pointer);

/*
 * new_i2s_dma_structure - Allocate new I2s data structures.
 * @settings: Codec settings
 * @bps: Bits per sample
 */
I2s *new_i2s_dma_structure(const I2sSettings *settings, int bps, GpioOps *sdmode,
			   uintptr_t ssp_i2s_start_address);

#endif /* __DRIVERS_BUS_I2S_INTEL_COMMON_I2S_DMA_H__ */
