/*
 * (C) Copyright 2012 Samsung Electronics Co. Ltd
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <fdtdec.h>
#include <mmc.h>
#include <asm/arch/clk.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gpio.h>
#include <asm/arch/mshc.h>
#include <asm/arch/pinmux.h>

/* support 4 mmc hosts */
enum {
	MAX_MMC_HOSTS	= 4
};

static struct mmc mshci_dev[MAX_MMC_HOSTS];
static struct mshci_host mshci_host[MAX_MMC_HOSTS];
static int num_devs;

#include <asm/arch/clock.h>
#include <asm/arch/periph.h>

/* Struct to hold mshci register and bus width */
struct fdt_mshci {
	struct s5p_mshci *reg;	/* address of registers in physical memory */
	int bus_width;		/* bus width  */
	int removable;		/* removable device? */
	enum periph_id periph_id;	/* Peripheral ID for this peripheral */
	struct fdt_gpio_state enable_gpio;	/* How to enable it */
};

/**
 * Set bits of MSHCI host control register.
 *
 * @param host	MSHCI host
 * @param bits	bits to be set
 * @return 0 on success, -1 on failure
 */
static int mshci_setbits(struct mshci_host *host, unsigned int bits)
{
	ulong start;

	setbits_le32(&host->reg->ctrl, bits);

	start = get_timer(0);
	while (readl(&host->reg->ctrl) & bits) {
		if (get_timer(start) > TIMEOUT_MS) {
			debug("Set bits failed\n");
			return -1;
		}
	}

	return 0;
}

/**
 * Reset MSHCI host control register.
 *
 * @param host	MSHCI host
 * @return 0 on success, -1 on failure
 */
static int mshci_reset_all(struct mshci_host *host)
{
	ulong start;

	/*
	* Before we reset ciu check the DATA0 line.  If it is low and
	* we resets the ciu then we might see some errors.
	*/
	start = get_timer(0);
	while (readl(&host->reg->status) & DATA_BUSY) {
		if (get_timer(start) > TIMEOUT_MS) {
			debug("Controller did not release"
				"data0 before ciu reset\n");
			return -1;
		}
	}

	if (mshci_setbits(host, CTRL_RESET)) {
		debug("Fail to reset card.\n");
		return -1;
	}
	if (mshci_setbits(host, FIFO_RESET)) {
		debug("Fail to reset fifo.\n");
		return -1;
	}
	if (mshci_setbits(host, DMA_RESET)) {
		debug("Fail to reset dma.\n");
		return -1;
	}

	return 0;
}

static void mshci_set_mdma_desc(u8 *desc_vir, u8 *desc_phy,
		unsigned int des0, unsigned int des1, unsigned int des2)
{
	struct mshci_idmac *desc = (struct mshci_idmac *)desc_vir;

	desc->des0 = des0;
	desc->des1 = des1;
	desc->des2 = des2;
	desc->des3 = (unsigned int)desc_phy + sizeof(struct mshci_idmac);
}

/*
 * Prepare the data to be transfer
 *
 * @param host		pointer to mshci_host
 * @param data		pointer to mmc_data
 *
 * Return		0 if success else -1
 */
static int mshci_prepare_data(struct mshci_host *host, struct mmc_data *data)
{
	unsigned int i;
	unsigned int data_cnt;
	unsigned int des_flag;
	unsigned int blksz;
	ulong data_start, data_end;
	static struct mshci_idmac idmac_desc[0x10000];
	/* TODO(alim.akhtar@samsung.com): do we really need this big array? */

	struct mshci_idmac *pdesc_dmac;

	if (mshci_setbits(host, FIFO_RESET)) {
		debug("Fail to reset FIFO\n");
		return -1;
	}

	pdesc_dmac = idmac_desc;
	blksz = data->blocksize;
	data_cnt = data->blocks;

	for  (i = 0;; i++) {
		des_flag = (MSHCI_IDMAC_OWN | MSHCI_IDMAC_CH);
		des_flag |= (i == 0) ? MSHCI_IDMAC_FS : 0;
		if (data_cnt <= 8) {
			des_flag |= MSHCI_IDMAC_LD;
			mshci_set_mdma_desc((u8 *)pdesc_dmac,
			(u8 *)virt_to_phys(pdesc_dmac),
			des_flag, blksz * data_cnt,
			(unsigned int)(virt_to_phys(data->dest)) +
			(unsigned int)(i * 0x1000));
			break;
		}
		/* max transfer size is 4KB per descriptor */
		mshci_set_mdma_desc((u8 *)pdesc_dmac,
			(u8 *)virt_to_phys(pdesc_dmac),
			des_flag, blksz * 8,
			virt_to_phys(data->dest) +
			(unsigned int)(i * 0x1000));

		data_cnt -= 8;
		pdesc_dmac++;
	}

	data_start = (ulong)idmac_desc;
	data_end = (ulong)pdesc_dmac;
	flush_dcache_range(data_start, data_end + ARCH_DMA_MINALIGN);

	data_start = (ulong)data->dest;
	data_end  = (ulong)(data->dest + data->blocks * data->blocksize);
	flush_dcache_range(data_start, data_end);

	writel((unsigned int)virt_to_phys(idmac_desc), &host->reg->dbaddr);

	/* enable the Internal DMA Controller */
	setbits_le32(&host->reg->ctrl, ENABLE_IDMAC | DMA_ENABLE);
	setbits_le32(&host->reg->bmod, BMOD_IDMAC_ENABLE | BMOD_IDMAC_FB);

	writel(data->blocksize, &host->reg->blksiz);
	writel(data->blocksize * data->blocks, &host->reg->bytcnt);

	return 0;
}

static int mshci_set_transfer_mode(struct mshci_host *host,
	struct mmc_data *data)
{
	int mode = CMD_DATA_EXP_BIT;

	if (data->blocks > 1)
		mode |= CMD_SENT_AUTO_STOP_BIT;
	if (data->flags & MMC_DATA_WRITE)
		mode |= CMD_RW_BIT;

	return mode;
}

/*
 * Sends a command out on the bus.
 *
 * @param mmc	mmc device
 * @param cmd	mmc_cmd to be sent on bus
 * @param data	mmc data to be sent (optional)
 *
 * @return	return 0 if ok, else -1
 */
static int s5p_mshci_send_command(struct mmc *mmc, struct mmc_cmd *cmd,
		struct mmc_data *data)
{
	struct mshci_host *host = mmc->priv;

	int flags = 0, i;
	unsigned int mask;
	ulong start, data_start, data_end;

	/*
	 * If auto stop is enabled in the control register, ignore STOP
	 * command, as controller will internally send a STOP command after
	 * every block read/write
	 */
	if ((readl(&host->reg->ctrl) & SEND_AS_CCSD) &&
			(cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION))
		return 0;

	/*
	* We shouldn't wait for data inihibit for stop commands, even
	* though they might use busy signaling
	*/
	start = get_timer(0);
	while (readl(&host->reg->status) & DATA_BUSY) {
		if (get_timer(start) > COMMAND_TIMEOUT) {
			debug("timeout on data busy\n");
			return -1;
		}
	}

	if ((readl(&host->reg->rintsts) & (INTMSK_CDONE | INTMSK_ACD)) == 0) {
		uint32_t v = readl(&host->reg->rintsts);
		if (v)
			debug("there are pending interrupts 0x%x\n", v);
	}

	/* It clears all pending interrupts before sending a command*/
	writel(INTMSK_ALL, &host->reg->rintsts);

	if (data) {
		if (mshci_prepare_data(host, data)) {
			debug("fail to prepare data\n");
			return -1;
		}
	}

	writel(cmd->cmdarg, &host->reg->cmdarg);

	if (data)
		flags = mshci_set_transfer_mode(host, data);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY)) {
		/* this is out of SD spec */
		debug("wrong response type or response busy for cmd %d\n",
				cmd->cmdidx);
		return -1;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		flags |= CMD_RESP_EXP_BIT;
		if (cmd->resp_type & MMC_RSP_136)
			flags |= CMD_RESP_LENGTH_BIT;
	}

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= CMD_CHECK_CRC_BIT;
	flags |= (cmd->cmdidx | CMD_STRT_BIT | CMD_USE_HOLD_REG |
			CMD_WAIT_PRV_DAT_BIT);

	mask = readl(&host->reg->cmd);
	if (mask & CMD_STRT_BIT) {
		debug("cmd busy, current cmd: %d", cmd->cmdidx);
		return -1;
	}

	writel(flags, &host->reg->cmd);
	/* wait for command complete by busy waiting. */
	for (i = 0; i < COMMAND_TIMEOUT; i++) {
		mask = readl(&host->reg->rintsts);
		if (mask & INTMSK_CDONE) {
			if (!data)
				writel(mask, &host->reg->rintsts);
			break;
		}
	}
	/* timeout for command complete. */
	if (COMMAND_TIMEOUT == i) {
		debug("timeout waiting for status update\n");
		return TIMEOUT;
	}

	if (mask & INTMSK_RTO)
		return TIMEOUT;
	else if (mask & INTMSK_RE) {
		debug("response error: 0x%x cmd: %d\n", mask, cmd->cmdidx);
		return -1;
	}
	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			/* CRC is stripped so we need to do some shifting. */
			cmd->response[0] = readl(&host->reg->resp3);
			cmd->response[1] = readl(&host->reg->resp2);
			cmd->response[2] = readl(&host->reg->resp1);
			cmd->response[3] = readl(&host->reg->resp0);
		} else {
			cmd->response[0] = readl(&host->reg->resp0);
			debug("\tcmd->response[0]: 0x%08x\n", cmd->response[0]);
		}
	}

	if (data) {
		start = get_timer(0);
		while (!(mask & (DATA_ERR | DATA_TOUT | INTMSK_DTO))) {
			mask = readl(&host->reg->rintsts);
			if (get_timer(start) > COMMAND_TIMEOUT) {
				debug("timeout on data error\n");
				return -1;
			}
		}
		writel(mask, &host->reg->rintsts);
		if (data->flags & MMC_DATA_READ) {
			data_start = (ulong)data->dest;
			data_end = (ulong)data->dest +
					data->blocks * data->blocksize;
			invalidate_dcache_range(data_start, data_end);
		}
		/* make sure disable IDMAC and IDMAC_Interrupts */
		clrbits_le32(&host->reg->ctrl, DMA_ENABLE | ENABLE_IDMAC);
		if (mask & (DATA_ERR | DATA_TOUT)) {
			debug("error during transfer: 0x%x\n", mask);
			/* mask all interrupt source of IDMAC */
			writel(0, &host->reg->idinten);
			return -1;
		} else if (mask & INTMSK_DTO) {
			debug("mshci dma interrupt end\n");
		} else {
			debug("unexpected condition 0x%x\n", mask);
			return -1;
		}
		clrbits_le32(&host->reg->ctrl, DMA_ENABLE | ENABLE_IDMAC);
		/* mask all interrupt source of IDMAC */
		writel(0, &host->reg->idinten);
	}

	/* TODO(alim.akhtar@samsung.com): check why we need this delay */
	udelay(100);

	return 0;
}

/*
 * ON/OFF host controller clock
 *
 * @param host		pointer to mshci_host
 * @param val		to enable/disable clock
 *
 * Return	0 if ok else -1
 */
static int mshci_clock_onoff(struct mshci_host *host, int val)
{
	ulong start;

	if (val)
		writel(CLK_ENABLE, &host->reg->clkena);
	else
		writel(CLK_DISABLE, &host->reg->clkena);

	writel(0, &host->reg->cmd);
	writel(CMD_ONLY_CLK, &host->reg->cmd);

	/*
	 * wait till command is taken by CIU, when this bit is set
	 * host should not attempted to write to any command registers.
	 */
	start = get_timer(0);
	while (readl(&host->reg->cmd) & CMD_STRT_BIT) {
		if (get_timer(start) > COMMAND_TIMEOUT) {
			debug("Clock %s has failed.\n ", val ? "ON" : "OFF");
			return -1;
		}
	}

	return 0;
}

/*
 * change host controller clock
 *
 * @param host		pointer to mshci_host
 * @param clock		request clock
 *
 * Return	0 if ok else -1
 */
static int mshci_change_clock(struct mshci_host *host, uint clock)
{
	int div;
	u32 sclk_mshc;
	ulong start;

	if (clock == host->clock)
		return 0;

	/* If Input clock is higher than maximum mshc clock */
	if (clock > MAX_MSHCI_CLOCK) {
		debug("Input clock is too high\n");
		clock = MAX_MSHCI_CLOCK;
	}

	/* disable the clock before changing it */
	if (mshci_clock_onoff(host, CLK_DISABLE)) {
		debug("failed to DISABLE clock\n");
		return -1;
	}

	/* get the clock division */
	sclk_mshc = clock_get_periph_rate(host->peripheral);

	/* CLKDIV */
	for (div = 1 ; div <= 0xff; div++) {
		if (((sclk_mshc / 4) / (2 * div)) <= clock) {
			writel(div, &host->reg->clkdiv);
			break;
		}
	}

	writel(div, &host->reg->clkdiv);

	writel(0, &host->reg->cmd);
	writel(CMD_ONLY_CLK, &host->reg->cmd);

	/*
	 * wait till command is taken by CIU, when this bit is set
	 * host should not attempted to write to any command registers.
	 */
	start = get_timer(0);
	while (readl(&host->reg->cmd) & CMD_STRT_BIT) {
		if (get_timer(start) > COMMAND_TIMEOUT) {
			debug("Changing clock has timed out.\n");
			return -1;
		}
	}

	clrbits_le32(&host->reg->cmd, CMD_SEND_CLK_ONLY);

	if (mshci_clock_onoff(host, CLK_ENABLE)) {
		debug("failed to ENABLE clock\n");
		return -1;
	}

	host->clock = clock;

	return 0;
}

/*
 * Set ios for host controller clock
 *
 * This sets the card bus width and clksel
 */
static void s5p_mshci_set_ios(struct mmc *mmc)
{
	struct mshci_host *host = mmc->priv;

	debug("bus_width: %x, clock: %d\n", mmc->bus_width, mmc->clock);

	if (mmc->clock > 0 && mshci_change_clock(host, mmc->clock))
		debug("mshci_change_clock failed\n");

	if (mmc->bus_width == 8)
		writel(PORT0_CARD_WIDTH8, &host->reg->ctype);
	else if (mmc->bus_width == 4)
		writel(PORT0_CARD_WIDTH4, &host->reg->ctype);
	else
		writel(PORT0_CARD_WIDTH1, &host->reg->ctype);

	if (host->peripheral == PERIPH_ID_SDMMC0)
		writel(0x03030001, &host->reg->clksel);
	if (host->peripheral == PERIPH_ID_SDMMC2)
		writel(0x03020001, &host->reg->clksel);
}

/*
 * Fifo init for host controller
 */
static void mshci_fifo_init(struct mshci_host *host)
{
	int fifo_val, fifo_depth, fifo_threshold;

	fifo_val = readl(&host->reg->fifoth);

	fifo_depth = 0x80;
	fifo_threshold = fifo_depth / 2;

	fifo_val &= ~(RX_WMARK | TX_WMARK | MSIZE_MASK);
	fifo_val |= (fifo_threshold | (fifo_threshold << 16) | MSIZE_8);
	writel(fifo_val, &host->reg->fifoth);
}

/*
 * MSHCI host controller initiallization
 *
 * @param host		pointer to mshci_host
 *
 * Return	0 if ok else -1
 */
static int mshci_init(struct mshci_host *host)
{
	/* power on the card */
	writel(POWER_ENABLE, &host->reg->pwren);

	if (mshci_reset_all(host)) {
		debug("mshci_reset_all() failed\n");
		return -1;
	}

	mshci_fifo_init(host);

	/* clear all pending interrupts */
	writel(INTMSK_ALL, &host->reg->rintsts);

	/* interrupts are not used, disable all */
	writel(0, &host->reg->intmask);

	return 0;
}

static int s5p_mphci_init(struct mmc *mmc)
{
	struct mshci_host *host = (struct mshci_host *)mmc->priv;
	unsigned int ier;

	if (mshci_init(host)) {
		debug("mshci_init() failed\n");
		return -1;
	}

	/* enumerate at 400KHz */
	if (mshci_change_clock(host, 400000)) {
		debug("mshci_change_clock failed\n");
		return -1;
	}

	/* set auto stop command */
	ier = readl(&host->reg->ctrl);
	ier |= SEND_AS_CCSD;
	writel(ier, &host->reg->ctrl);

	/* set 1bit card mode */
	writel(PORT0_CARD_WIDTH1, &host->reg->ctype);

	writel(0xfffff, &host->reg->debnce);

	/* set bus mode register for IDMAC */
	writel(BMOD_IDMAC_RESET, &host->reg->bmod);

	writel(0x0, &host->reg->idinten);

	/* set the max timeout for data and response */
	writel(TMOUT_MAX, &host->reg->tmout);

	return 0;
}

static int s5p_mshci_initialize(struct fdt_mshci *config)
{
	struct mshci_host *mmc_host;
	struct mmc *mmc;

	if (num_devs == MAX_MMC_HOSTS) {
		debug("%s: Too many hosts\n", __func__);
		return -1;
	}

	/* set the clock for mshci controller */
	if (clock_set_mshci(config->periph_id)) {
		debug("clock_set_mshci failed\n");
		return -1;
	}

	mmc = &mshci_dev[num_devs];
	mmc_host = &mshci_host[num_devs];

	sprintf(mmc->name, "S5P MSHC%d", num_devs);
	num_devs++;

	mmc->priv = mmc_host;
	mmc->send_cmd = s5p_mshci_send_command;
	mmc->set_ios = s5p_mshci_set_ios;
	mmc->init = s5p_mphci_init;

	mmc->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_HC;

	if (config->bus_width == 8)
		mmc->host_caps |= MMC_MODE_8BIT;
	else
		mmc->host_caps |= MMC_MODE_4BIT;

	mmc->f_min = MIN_MSHCI_CLOCK;
	mmc->f_max = MAX_MSHCI_CLOCK;

	exynos_pinmux_config(config->periph_id,
			config->bus_width == 8 ? PINMUX_FLAG_8BIT_MODE : 0);
	fdtdec_setup_gpio(&config->enable_gpio);
	if (fdt_gpio_isvalid(&config->enable_gpio)) {
		int pin = config->enable_gpio.gpio;
		int err;

		err = gpio_direction_output(pin, 1); /* Power on */
		if (err) {
			debug("%s: Unable to power on MSHCI\n", __func__);
			return -1;
		}
		gpio_set_pull(pin, EXYNOS_GPIO_PULL_NONE);
		gpio_set_drv(pin, EXYNOS_GPIO_DRV_4X);
	}
	mmc_host->clock = 0;
	mmc_host->reg =  config->reg;
	mmc_host->peripheral =  config->periph_id;
	mmc_register(mmc);
	mmc->block_dev.removable = config->removable;
	debug("s5p_mshci: periph_id=%d, width=%d, reg=%p, enable=%d\n",
	      config->periph_id, config->bus_width, config->reg,
	      config->enable_gpio.gpio);

	return 0;
}

#ifdef CONFIG_OF_CONTROL
int fdtdec_decode_mshci(const void *blob, int node, struct fdt_mshci *config)
{
	config->bus_width = fdtdec_get_int(blob, node,
				"samsung,mshci-bus-width", 4);

	config->reg = (struct s5p_mshci *)fdtdec_get_addr(blob, node, "reg");
	if ((fdt_addr_t)config->reg == FDT_ADDR_T_NONE)
		return -FDT_ERR_NOTFOUND;
	config->periph_id = clock_decode_periph_id(blob, node);
	fdtdec_decode_gpio(blob, node, "enable-gpios", &config->enable_gpio);

	config->removable = fdtdec_get_bool(blob, node, "samsung,removable");

	return 0;
}
#endif

int s5p_mshci_init(const void *blob)
{
	struct fdt_mshci config;
	int ret = 0;

#ifdef CONFIG_OF_CONTROL
	int node_list[MAX_MMC_HOSTS];
	int node, i;
	int count;

	count = fdtdec_find_aliases_for_id(blob, "sdmmc",
			COMPAT_SAMSUNG_EXYNOS5_MSHCI, node_list,
					   MAX_MMC_HOSTS);
	debug("%s: %d nodes\n", __func__, count);
	for (i = 0; i < count; i++) {
		node = node_list[i];

		if (node < 0)
			continue;

		if (fdtdec_decode_mshci(blob, node, &config))
			return -1;

		/* TODO(sjg): Move to using peripheral IDs in this driver */
		if (s5p_mshci_initialize(&config)) {
			debug("%s: Failed to init MSHCI %d\n", __func__, i);
			ret = -1;
			continue;
		}
	}
#else
	config.width = CONFIG_MSHCI_BUS_WIDTH;
	config.reg = (struct s5p_mshci *)samsung_get_base_mshci();
	config.periph_id = CONFIG_MSHCI_PERIPH_ID;
	config.removable = 1;
	if (s5p_mshci_initialize(&config) {
		debug("%s: Failed to init MSHCI %d\n", __func__, i);
		ret = -1;
		continue;
	}
#endif
	return ret;
}
