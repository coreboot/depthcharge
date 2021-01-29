/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
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

#include <arch/io.h>
#include <libpayload.h>

#include "drivers/storage/dw_mmc.h"
#include "drivers/storage/rk_dwmmc.h"
#include "drivers/gpio/rockchip.h"

struct rk3288_cru_reg {
	u32 cru_apll_con[4];
	u32 cru_dpll_con[4];
	u32 cru_cpll_con[4];
	u32 cru_gpll_con[4];
	u32 cru_npll_con[4];
	u32 cru_mode_con;
	u32 reserved0[3];
	u32 cru_clksel_con[43];
	u32 reserved1[21];
	u32 cru_clkgate_con[19];
	u32 reserved2;
	u32 cru_glb_srst_fst_value;
	u32 cru_glb_srst_snd_value;
	u32 cru_softrst_con[12];
	u32 cru_misc_con;
	u32 cru_glb_cnt_th;
	u32 cru_glb_rst_con;
	u32 reserved3;
	u32 cru_glb_rst_st;
	u32 reserved4;
	u32 cru_sdmmc_con[2];
	u32 cru_sdio0_con[2];
	u32 cru_sdio1_con[2];
	u32 cru_emmc_con[2];
};

static struct rk3288_cru_reg *cru_ptr = (void *)0xff760000;

void rkclk_configure_emmc(DwmciHost *host, unsigned int freq)
{
	int src_clk_div;

	dwmci_writel(host, DWMCI_CLKDIV, 0);
	src_clk_div = ALIGN_UP(host->src_hz / 2, freq) / freq;

	if (src_clk_div > 0x3f) {
		src_clk_div = (24000000 / 2 + freq - 1) / freq;
		write32(&cru_ptr->cru_clksel_con[12],
		        RK_CLRSETBITS(0xff << 8, 2 << 14 | ((src_clk_div - 1) << 8)));
	} else
		write32(&cru_ptr->cru_clksel_con[12],
		        RK_CLRSETBITS(0xff << 8, 1 << 14 | ((src_clk_div - 1) << 8)));
}

void rkclk_configure_sdmmc(DwmciHost *host, unsigned int freq)
{
	int src_clk_div;

	dwmci_writel(host, DWMCI_CLKDIV, 0);
	src_clk_div = ALIGN_UP(host->src_hz / 2, freq) / freq;

	if (src_clk_div > 0x3f) {
		src_clk_div = (24000000 / 2 + freq - 1) / freq;
		write32(&cru_ptr->cru_clksel_con[11],
		        RK_CLRSETBITS(0xff, 2 << 6 | (src_clk_div - 1)));
	} else
		write32(&cru_ptr->cru_clksel_con[11],
		        RK_CLRSETBITS(0xff, 1 << 6 | (src_clk_div - 1)));
}

DwmciHost *new_rkdwmci_host(uintptr_t ioaddr, uint32_t src_hz,
				int bus_width, int removable,
				GpioOps *card_detect)
{
	DwmciHost *mmc;

	mmc = new_dwmci_host(ioaddr, src_hz, bus_width, removable,
			     card_detect, 0);
	if(removable)
		mmc->set_clk = &rkclk_configure_sdmmc;
	else
		mmc->set_clk = &rkclk_configure_emmc;
	return mmc;
}
