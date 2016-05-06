/*
 * Copyright 2016 Rockchip Electronics Co., Ltd.
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

struct rk3399_cru_reg {
	u32 apll_l_con[6];
	u32 reserved[2];
	u32 apll_b_con[6];
	u32 reserved1[2];
	u32 dpll_con[6];
	u32 reserved2[2];
	u32 cpll_con[6];
	u32 reserved3[2];
	u32 gpll_con[6];
	u32 reserved4[2];
	u32 npll_con[6];
	u32 reserved5[2];
	u32 vpll_con[6];
	u32 reserved6[0x0a];
	u32 clksel_con[108];
	u32 reserved7[0x14];
	u32 clkgate_con[35];
	u32 reserved8[0x1d];
	u32 softrst_con[21];
	u32 reserved9[0x2b];
	u32 glb_srst_fst_value;
	u32 glb_srst_snd_value;
	u32 glb_cnt_th;
	u32 misc_con;
	u32 glb_rst_con;
	u32 glb_rst_st;
	u32 reserved10[0x1a];
	u32 sdmmc_con[2];
	u32 sdio0_con[2];
	u32 sdio1_con[2];
};
static struct rk3399_cru_reg *cru_ptr = (void *)0xff760000;

void rkclk_configure_sdmmc(DwmciHost *host, unsigned int freq)
{
	int src_clk_div;

	dwmci_writel(host, DWMCI_CLKDIV, 0);
	src_clk_div = ALIGN_UP(host->src_hz / 2, freq) / freq;

	if (src_clk_div > 0x3f) {
		src_clk_div = (24000000 / 2 + freq - 1) / freq;
		writel(RK_CLRSETBITS(0x7ff, 5 << 8 | (src_clk_div - 1)),
				     &cru_ptr->clksel_con[16]);
	} else
		writel(RK_CLRSETBITS(0x7ff, 1 << 8 | (src_clk_div - 1)),
				     &cru_ptr->clksel_con[16]);
}

DwmciHost *new_rkdwmci_host(uintptr_t ioaddr, uint32_t src_hz,
				int bus_width, int removable,
				GpioOps *card_detect)
{
	DwmciHost *mmc;

	mmc = new_dwmci_host(ioaddr, src_hz, bus_width, removable,
			     card_detect, 0);

	/* rk3399 only sdmmc use dw mmc */
	mmc->set_clk = &rkclk_configure_sdmmc;

	return mmc;
}
