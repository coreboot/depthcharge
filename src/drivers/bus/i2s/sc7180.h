/*
 * This file is part of the depthcharge project.
 *
 * Copyright (c) 2019-2020 Qualcomm Technologies
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_BUS_I2S_SC7180_H__
#define __DRIVERS_BUS_I2S_SC7180_H__

#include "drivers/sound/sound.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/sound/i2s.h"

/*  MI2S device id */
#define	LPASS_PRIMARY 0
#define	LPASS_SECONDARY 1
#define	LPASS_TERTIARY 2
#define	LPASS_QUATERNARY 3

typedef struct {
	uint32_t lpaif_cmd_rgcr;
	uint32_t lpaif_cfg_rgcr;
	uint32_t lpaif_m;
	uint32_t lpaif_n;
	uint32_t lpaif_d;
	uint8_t _reserved1[4];
	uint32_t ibit_cbcr;
	uint32_t ebit_cbcr;
	uint32_t clk_inv;
	uint32_t clk_detect;
	uint8_t _reserved2[0xfd8];
} LpassBclkCbcr;

typedef struct {
	uint32_t rddma_ctl;
	uint32_t rddma_base;
	uint32_t rddma_buf_len;
	uint8_t _reserved1[0x4];
	uint32_t rddma_per_len;
	uint8_t _reserved2[0x2c];
	uint32_t rddma_base_ext;
	uint8_t _reserved3[0xfbc];
} LpaifDmaRdReg;

typedef struct {
	uint32_t wrdma_ctl;
	uint32_t wrdma_base;
	uint32_t wrdma_buf_len;
	uint8_t _reserved1[0x4];
	uint32_t wrdma_per_len;
	uint8_t _reserved3[0xfec];
} LpaifDmaWrReg;

typedef struct {
	uint32_t i2s_ctl;
	uint8_t _reseved4[0x1fc];
	uint32_t pcm_i2s_sel;
	uint8_t _reserved1[0x2fc];
	uint32_t pcm_ctla;
	uint8_t  _reserved2[0x14];
	uint32_t pcm_tdm_ctl_a;
	uint32_t pcm_tdm_sample_width_a;
	uint32_t pcm_rpcm_slot_num_a;
	uint32_t pcm_tpcm_slot_num_a;
	uint8_t  _reserved3[0xad8];
} LpaifI2sReg;

typedef struct {
	uint32_t mode_mux;
	uint8_t _reserved[0xffc];
} LpaifModeMuxsel;

typedef struct {
	uint32_t dig_pll_mode;
	uint32_t dig_pll_l;
	uint32_t dig_pll_cal;
	uint32_t dig_pll_user_ctl;
	uint32_t dig_pll_user_ctl_u;
	uint8_t  _reservedl1[0x8];
	uint32_t dig_pll_config_ctl_u;
	uint8_t _reservedl[0x18];
	uint32_t dig_pll_opmode;
} LpassPllSet;

typedef struct {
	uint32_t irq_en;
	uint8_t reserved1[4];
	uint32_t irq_raw_stat;
	uint32_t irq_clear;
	uint8_t reserved2[0xff0];
} LpaifIrq;

struct Sc7180LpassReg {
	uint8_t _reserved1[0x400020];
	uint32_t qdsp_core_cbcr;
	uint8_t _reserved2[0x400038 - 0x400024];
	uint32_t qdsp_x0_cbcr;
	uint32_t qdsp_sleep_cbcr;
	uint8_t _reserved3[0x400048 - 0x400040];
	uint32_t qdsp_spdm_mon_cbcr;
	uint8_t _reserved4[0x701000- 0x40004c];
	uint32_t xpu2_client_cbcr;
	uint8_t _reserved5[0x71001c- 0x701004];
	uint32_t if1_ibit_cbcr;
	uint32_t if1_ebit_cbcr;
	uint8_t _reserved6[0x71101c - 0x710024];
	uint32_t if2_ibit_cbcr;
	uint32_t if2_ebit_cbcr;
	uint8_t _reserved7[0x71201c - 0x711024];
	uint32_t if3_ibit_cbcr;
	uint32_t if3_ebit_cbcr;
	uint8_t _reserved8[0x713000 - 0x712024];
	uint32_t sampling;
	uint8_t _reserved9[0x719018 - 0x713004];
	uint32_t lpaif_pcmoe;
	uint8_t _reserved10[0x71e000 - 0x71901c];
	uint32_t mem;
	uint32_t mem0;
	uint32_t mem1;
	uint32_t mem2;
	uint8_t reserved11[0x71e014 - 0x71e010];
	uint32_t bus_timeout_cbcr;
	uint8_t _reserved12[0x71f000 - 0x71e018];
	uint32_t bus;
	uint8_t _reserved13[0x720018 - 0x71f004];
	uint32_t ext_mclk0;
	uint8_t _reserved14[0x721018 - 0x72001c];
	uint32_t ext_mclk1;
	uint8_t _reserved15[0x7220cc - 0x72101c];
	uint32_t wsa_mclk2x;
	uint8_t _reserved16[0x7220d4 - 0x7220d0];
	uint32_t wsa_mclk;
	uint8_t _reserved17[0x7240cc - 0x7220d8];
	uint32_t rx_mclk_2x;
	uint8_t _reserved18[0x7240d4 - 0x7240d0];
	uint32_t rx_mclk;
	uint8_t _reserved19[0x72d004 - 0x7240d8];
	uint32_t gcc_dbg;
	uint8_t _reserved20[0x78200c - 0x72d008];
	uint32_t cpr;
	uint8_t _reserved21[0x783004 - 0x782010];
	uint32_t pdc_gds;
	uint8_t _reserved22[0x783090 - 0x783008];
	uint32_t pdc_hm_gdscr;
	uint8_t _reserved23[0x784004 - 0x783094];
	uint32_t sleep_cbcr_clk_off_ack;
	uint8_t _reserved24[0x786000 - 0x784008];
	uint32_t  qsm_x0;
	uint8_t _reserved25[0x78801c - 0x786004];
	uint32_t q6_x0;
	uint8_t _reserved26[0x78900c - 0x788020];
	uint32_t pdc_h;
	uint32_t csr_h;
	uint32_t audio_hm_h_cbcr;
	uint8_t reserved27[0x78901c - 0x789018];
	uint32_t q6_ahbm_cbcr;
	uint32_t q6_ahbs;
	uint8_t _reserved28[0x789028 - 0x789024];
	uint32_t va_mem0_cbcr;
	uint8_t _reserved29[0x789030 - 0x78902c];
	uint32_t ahb_timeout;
	uint32_t debug_h;
	uint32_t t32_apb_access;
	uint32_t aon_h;
	uint32_t ssc_h;
	uint8_t _reserved30[0x789048 - 0x789044];
	uint32_t bus_alt;
	uint32_t mcc_access;
	uint8_t _reserved31[0x789078 - 0x789050];
	uint32_t rsc_hclk;
	uint8_t _reserved32[0x789090 - 0x78907c];
	uint32_t audio_hm_gdscr;
	uint8_t _reserved33[0x78a00c - 0x789094];
	uint32_t at_cbcr;
	uint32_t q6_atbm;
	uint32_t pclk_dbg;
	uint8_t _reserved34[0x78a058 - 0x78a018];
	uint32_t debug_tsctr;
	uint8_t _reserved35[0x78a080 - 0x78a05c];
	uint32_t pll_lv_test;
	uint32_t debug_cbcr;
	uint8_t _reserved36[0x78e000 - 0x78a088];
	uint32_t va_xpu2_client_cbcr;
	uint8_t _reserved37[0x78f000 - 0x78e004];
	uint32_t q6_xpu2_client;
	uint32_t q6_xpu2_config_bcr;
	uint32_t q6_xpu2_config_cbcr;
	uint8_t _reserved38[0x790004 - 0x78f00c];
	uint32_t sleep_cbcr_add;
	uint8_t _reserved39[0x79000c - 0x790008];
	uint32_t ro_cbcr;
	uint32_t audio_hm_sleep;
	uint8_t _reserved40[0x79200c - 0x790014];
	uint32_t va_2x;
	uint8_t _reserved41[0x792014 - 0x792010];
	uint32_t va_cbcr;
	uint8_t _reserved42[0x79300c - 0x792018];
	uint32_t tx_mclk_2x;
	uint8_t _reserved43[0x793014 - 0x793010];
	uint32_t tx_mclk;
	uint8_t _reserved44[0x795008 - 0x793018];
	uint32_t vs_vddmx;
	uint8_t _reserved45[0x795018 - 0x79500c];
	uint32_t vs_vddcx;
	uint8_t _reserved46[0x796090 - 0x79501c];
	uint32_t ssc_gdscr;
	uint8_t  _reservedb[0xd01000 - 0x796094];
	LpassPllSet pll_config;
	uint8_t _reserved47[0xd0b008 - 0xd0103c];
	uint32_t audio_core_sampling;
	uint8_t _reserved48[0xd10000 - 0xd0b00c];
	LpassBclkCbcr bit_cbcr[3];
	uint8_t _reserved49[0xd15014 - 0xd13000];
	uint32_t core_avsync_atime;
	uint8_t _reserved50[0xd16098 - 0xd15018];
	uint32_t core_resampler;
	uint8_t _reserved51[0xd17014 - 0xd1609c];
	uint32_t aud_slimbus;
	uint32_t aud_slimbus_core;
	uint32_t aud_slimbus_npl;
	uint8_t _reserved52[0xd19014 - 0xd17020];
	uint32_t prim_data_oe;
	uint8_t _reserved53[0xd1c000 - 0xd19018];
	uint32_t avsync_stc;
	uint8_t _reserved54[0xd1e000 - 0xd1c004];
	uint32_t lpm_core;
	uint32_t lpm_mem0;
	uint8_t _reserved55[0xd1f000 - 0xd1e008];
	uint32_t core;
	uint8_t _reserved56[0xd20014 - 0xd1f004];
	uint32_t core_ext_mclk0;
	uint8_t _reserved57[0xd21014 - 0xd20018];
	uint32_t core_ext_mclk1;
	uint8_t _reserved58[0xd22014 - 0xd21018];
	uint32_t core_ext_mclk2;
	uint8_t _reserved59[0xd23000 - 0xd22018];
	uint32_t sysnoc_mport;
	uint8_t _reserved60[0xd24000 - 0xd23004];
	uint32_t sysnoc_sway;
	uint8_t _reserved61[0xd25000 - 0xd24004];
	uint32_t q6ss_axim2;
	uint8_t _reserved62[0xd3b000 - 0xd25004];
	uint32_t bus_timeout_core;
	uint8_t _reserved63[0xd40014 - 0xd3b004];
	uint32_t hw_af;
	uint32_t hw_af_noc_anoc;
	uint32_t hw_af_noc;
	uint8_t _reserved64[0xd4e008 - 0xd40020];
	uint32_t lpass_cc_debug;
	uint8_t _reserved65[0xd4e014 - 0xd4e00c];
	uint32_t lpass_cc_pll_test;
	uint8_t _reserved66[0xd9d000 - 0xd4e018];
	LpaifModeMuxsel lmm[3];
	uint8_t _reserved67[0xf01000 - 0xda0000];
	LpaifI2sReg i2s_reg[3];
	uint8_t reserverd68[0xf09000 - 0xf04000];
	LpaifIrq irq_reg[3];
	LpaifDmaRdReg dma_rd_reg[3];
	uint8_t reserved69[0xf18000 - 0xf0f000];
	LpaifDmaWrReg dma_wr_reg[3];
	uint8_t _reserved70[0x1000000 - 0xf1b000];
	uint32_t core_hm_gdscr;
	uint8_t _reserved71[0x1003000 - 0x1000004];
	uint32_t top_cc_agnoc_hs;
	uint8_t _reserved72[0x1004000 - 0x1003004];
	uint32_t top_cc_lpi_q6_axim_hs;
	uint8_t _reserved73[0x1005000 - 0x1004004];
	uint32_t top_hw_af_noc;
	uint8_t _reserved74[0x1006000 - 0x1005004];
	uint32_t top_cc_agnoc_ls;
	uint8_t _reserved75[0x1007000 - 0x1006004];
	uint32_t top_cc_agnoc_mpu;
	uint8_t _reserved76[0x1008000 - 0x1007004];
	uint32_t top_cc_lpi_sway_ahb;
	uint8_t _reserved77[0x1009000 - 0x1008004];
	uint32_t top_cc_lpass_core_sway_ahb;
};

typedef struct Sc7180LpassReg Sc7180LpassReg;

check_member(Sc7180LpassReg, sampling, 0x713000);
check_member(Sc7180LpassReg, bus_alt, 0x789048);
check_member(Sc7180LpassReg, avsync_stc, 0xd1c000);
check_member(Sc7180LpassReg, lpass_cc_pll_test, 0xd4e014);
check_member(Sc7180LpassReg, top_cc_lpass_core_sway_ahb, 0x1009000);


typedef struct {
	I2sOps ops;
	Sc7180LpassReg *sc7180_regs;
	uint8_t device_id;
	uint8_t initialized;
	uint32_t bclk_rate;
} Sc7180I2s;

Sc7180I2s *new_sc7180_i2s(uint32_t sample_rate, uint32_t channels,
				uint32_t bitwidth, uint8_t device_id,
				uintptr_t base_addr);

#endif /* __DRIVERS_BUS_I2S_SC7180_H__ */
