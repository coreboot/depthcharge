/*
 * (C) Copyright 2012 SAMSUNG Electronics
 * Jaehoon Chung <jh80.chung@samsung.com>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,  MA 02111-1307 USA
 *
 */

#include <common.h>
#include <dwmmc.h>
#include <fdtdec.h>
#include <libfdt.h>
#include <malloc.h>
#include <asm/arch/dwmmc.h>
#include <asm/arch/clk.h>
#include <asm/arch/gpio.h>
#include <asm/arch/pinmux.h>

#define DWMCI_CLKSEL		0x09C
#define DWMCI_SHIFT_0		0x0
#define DWMCI_SHIFT_1		0x1
#define DWMCI_SHIFT_2		0x2
#define DWMCI_SHIFT_3		0x3
#define DWMCI_SET_SAMPLE_CLK(x)	(x)
#define DWMCI_SET_DRV_CLK(x)	((x) << 16)
#define DWMCI_SET_DIV_RATIO(x)	((x) << 24)

#define	DWMMC_MAX_FREQ			52000000
#define	DWMMC_MIN_FREQ			400000
#define	DWMMC_MMC0_CLKSEL_VAL		0x03030001
#define	DWMMC_MMC2_CLKSEL_VAL		0x03020001

/*
 * Function used as callback function to initialise the
 * CLKSEL register for every mmc channel.
 */
static void exynos_dwmci_clksel(struct dwmci_host *host)
{
	dwmci_writel(host, DWMCI_CLKSEL, host->clksel_val);
}

unsigned int exynos_dwmci_get_clk(int dev_index)
{
	return get_mmc_clk(dev_index);
}

/*
 * This function adds the mmc channel to be registered with mmc core.
 * index -	mmc channel number.
 * regbase -	register base address of mmc channel specified in 'index'.
 * bus_width -	operating bus width of mmc channel specified in 'index'.
 * clksel -	value to be written into CLKSEL register in case of FDT.
 *		NULL in case od non-FDT.
 * removable - set to True if the device can be removed (like an SD card), to
 *             False if not (like ak eMMC drive)
 * pre_init -	Kick the mmc on startup so that it is ready sooner when we
 *		need it
 */
int exynos_dwmci_add_port(int index, u32 regbase, int bus_width,
			  u32 clksel, int removable, int pre_init)
{
	struct dwmci_host *host = NULL;
	unsigned int div;
	unsigned long freq, sclk;
	host = malloc(sizeof(struct dwmci_host));
	if (!host) {
		printf("dwmci_host malloc fail!\n");
		return 1;
	}
	/* request mmc clock vlaue of 52MHz.  */
	freq = 52000000;
	sclk = get_mmc_clk(index);
	div = DIV_ROUND_UP(sclk, freq);
	/* set the clock divisor for mmc */
	set_mmc_clk(index, div);

	host->name = "EXYNOS DWMMC";
	host->ioaddr = (void *)regbase;
	host->buswidth = bus_width;

	if (clksel) {
		host->clksel_val = clksel;
	} else {
		if (0 == index)
			host->clksel_val = DWMMC_MMC0_CLKSEL_VAL;
		if (2 == index)
			host->clksel_val = DWMMC_MMC2_CLKSEL_VAL;
	}

	host->clksel = exynos_dwmci_clksel;
	host->dev_index = index;
	host->mmc_clk = exynos_dwmci_get_clk;
	/* Add the mmc channel to be registered with mmc core */
	if (add_dwmci(host, DWMMC_MAX_FREQ, DWMMC_MIN_FREQ, removable,
		      pre_init)) {
		debug("dwmmc%d registration failed\n", index);
		return -1;
	}
	return 0;
}

int board_mmc_getcd(struct mmc *mmc)
{
	unsigned cdetect;

	if (mmc->block_dev.removable) {
		struct dwmci_host *host = mmc->priv;

		cdetect = dwmci_readl(host, DWMCI_CDETECT);
	} else {
		cdetect = 0; /* zero means present */
	}
	return !cdetect;
}

#ifdef CONFIG_OF_CONTROL
int exynos_dwmmc_init(const void *blob)
{
	int index, bus_width, removable;
	int node_list[DWMMC_MAX_CH_NUM];
	int err = 0, dev_id, flag, count, i;
	u32 clksel_val, base, timing[3];
	int pre_init;

	count = fdtdec_find_aliases_for_id(blob, "mmc",
				COMPAT_SAMSUNG_EXYNOS5_DWMMC, node_list,
				DWMMC_MAX_CH_NUM);

	for (i = 0; i < count; i++) {
		struct fdt_gpio_state gpio;
		int node = node_list[i];

		if (node <= 0)
			continue;

		/* Get enable GPIO */
		err = fdtdec_decode_gpio(blob, node, "enable-gpios", &gpio);
		/* If error, assume no enable GPIOs */
		if (!err) {
			debug("%s: Enabling GPIO %d\n",
				__func__, gpio.gpio);
			fdtdec_gpio_direction_output(&gpio, 1);
			/* TODO: Disable this if things fail later */
		}


		/* Extract device id for each mmc channel */
		dev_id = pinmux_decode_periph_id(blob, node);

		/* Get the bus width from the device node */
		bus_width = fdtdec_get_int(blob, node, "samsung,bus-width", 0);
		if (bus_width <= 0) {
			debug("DWMMC: Can't get bus-width\n");
			return -1;
		}
		if (8 == bus_width)
			flag = PINMUX_FLAG_8BIT_MODE;
		else
			flag = PINMUX_FLAG_NONE;

		/* config pinmux for each mmc channel */
		err = exynos_pinmux_config(dev_id, flag);
		if (err) {
			debug("DWMMC not configured\n");
			return err;
		}

		index = dev_id - PERIPH_ID_SDMMC0;

		/* Get the base address from the device node */
		base = fdtdec_get_addr(blob, node, "reg");
		if (!base) {
			debug("DWMMC: Can't get base address\n");
			return -1;
		}
		/* Extract the timing info from the node */
		err = fdtdec_get_int_array(blob, node, "samsung,timing",
					timing, 3);
		if (err) {
			debug("Can't get sdr-timings for divider\n");
			return -1;
		}

		removable = fdtdec_get_int(blob, node, "samsung,removable", 0);
		pre_init = fdtdec_get_bool(blob, node, "samsung,pre-init");

		clksel_val = (DWMCI_SET_SAMPLE_CLK(timing[0]) |
				DWMCI_SET_DRV_CLK(timing[1]) |
				DWMCI_SET_DIV_RATIO(timing[2]));
		/* Initialise each mmc channel */
		err = exynos_dwmci_add_port(index, base, bus_width,
					    clksel_val, removable, pre_init);
		if (err)
			debug("dwmmc Channel-%d init failed\n", index);
	}
	return 0;
}
#endif
