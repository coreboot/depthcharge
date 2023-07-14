/*
 * Copyright (C) 2019 Intel Corporation
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

#include <arch/io.h>
#include "base/container_of.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/bus/i2s/intel_common/i2s.h"
#include "drivers/bus/i2s/intel_common/i2s-regs.h"
#include <libpayload.h>

#define LPE_SSP_FIFO_SIZE	16
#define DMYSTOP_BITS		2
#define SLOT_PADDING_MAX	31

/*
 * i2s_ensable -  enable SSP device
 * @regs: pointer to registers
 *
 * Writes SSP register to enable the device
 */
static void i2s_enable(I2sRegs *regs)
{
	set_SSCR0_reg(regs, SSE);
	set_SSTSA_reg(regs, TXEN);
}

/**
 * i2s_disable -  disable SSP device
 * @regs: pointer to registers
 *
 * Writes SSP register to disable the device
 */
static void i2s_disable(I2sRegs *regs)
{
	clear_SSCR0_reg(regs, SSE);
	clear_SSTSA_reg(regs, TXEN);
}

/*
 * calculate_sspsp - Calculate sspsp register
 * @settings: config settings
 *
 * Calculate sspsp register.
 */
static uint32_t calculate_sspsp(const I2sSettings *settings, int bps)
{
	uint32_t sspsp = 0;
	uint32_t frame_len;
	uint32_t frame_sample_bits;
	uint32_t frame_end_padding;
	uint32_t slot_end_padding;

	if (!settings->bclk_rate || !settings->fsync_rate) {
		printf("%s : invalid BCLK %d or FSYNC %d\n", __func__,
		       settings->bclk_rate, settings->fsync_rate);
		goto out;
	}

	if (settings->bclk_rate % settings->fsync_rate) {
		printf("%s : BCLK %d is not divisable by FSYNC %d\n", __func__,
		       settings->bclk_rate, settings->fsync_rate);
		goto out;
	}

	frame_len = settings->bclk_rate / settings->fsync_rate;
	frame_sample_bits = settings->frame_rate_divider_ctrl * bps;

	if (frame_len < frame_sample_bits) {
		printf("%s : frame_len %d < frame_sample_bits %d\n", __func__,
		       frame_len, frame_sample_bits);
		goto out;
	}

	frame_end_padding = frame_len - frame_sample_bits;

	/* I2S format */
	if (frame_len % 2) {
		printf("%s : frame_len %d is not divisible by 2\n", __func__,
		       frame_len);
		goto out;
	}

	frame_len /= 2;

	if (frame_end_padding % 2) {
		printf("%s : frame_end_padding %d is not divisible by 2\n",
		       __func__, frame_end_padding);
		goto out;
	}

	slot_end_padding = frame_end_padding / 2;

	if (slot_end_padding > SLOT_PADDING_MAX) {
		/* too big padding */
		printf("%s : slot_end_padding %d > %d\n", __func__,
		       slot_end_padding, SLOT_PADDING_MAX);
		goto out;
	}

	sspsp = SSPSP_reg(FSRT, NEXT_FRMS_ASS_WITH_LSB_PREVIOUS_FRM) |
		SSPSP_reg(SFRMWDTH, frame_len) |
		SSPSP_reg(DMYSTOP, slot_end_padding);

	slot_end_padding >>= DMYSTOP_BITS;
	sspsp |= SSPSP_reg(EDMYSTOP, slot_end_padding);

out:
	return sspsp;
}

/*
 * calculate_sscr0: Calculate sscr0 register
 * @settings: config settings
 * @bps: bits per sample
 *
 * Calculate sscr0 register
 */
static uint32_t calculate_sscr0(const I2sSettings *settings, int bps)
{
	uint32_t sscr0 = 0;

	if (bps > 16)
		sscr0 = SSCR0_reg(DSS, SSCR0_DataSize(bps - 16)) |
			SSCR0_reg(EDSS, EDSS_17_32_BITS);
	else
		sscr0 = SSCR0_reg(DSS, SSCR0_DataSize(bps)) |
			SSCR0_reg(EDSS, EDSS_4_16_BITS);

	sscr0 |= SSCR0_reg(MOD, settings->mode) |
		 SSCR0_reg(FRF, PSP_FORMAT) |
		 SSCR0_reg(SCR, SERIAL_CLOCK_RATE) |
		 SSCR0_reg(RIM, SSP_FIFO_INT_DISABLE) |
		 SSCR0_reg(TIM, SSP_FIFO_INT_DISABLE) |
		 SSCR0_reg(ECS, DIV_DISABLE) |
		 SSCR0_reg(NCS, NETWORK_CLOCK_DISABLE) |
		 SSCR0_reg(FRDC,
			SSCR0_SlotsPerFrm(settings->frame_rate_divider_ctrl));

	/* ECS bit set is needed for cAVS 2.0 or cAVS 2.5 */
	if (CONFIG(INTEL_COMMON_I2S_CAVS_2_0) ||
		CONFIG(INTEL_COMMON_I2S_CAVS_2_5) ||
		/* ACE must select divider clock for SCLK */
		CONFIG(INTEL_COMMON_I2S_ACE_1_x))
		sscr0 |= SSCR0_reg(ECS, DIV_ENABLE);
	return sscr0;
}

/**
 * calculate_sscr1 - Calculate sscr1 register.
 * @settings: config settings
 *
 * Calculate sscr1 register.
 */
static uint32_t calculate_sscr1(const I2sSettings *settings)
{
	uint32_t sscr1 = 0;

	sscr1 = SSCR1_reg(TTE, TXD_TRISTATE_ON) |
		SSCR1_reg(TTELP, TXD_TRISTATE_LAST_PHASE_ON) |
		SSCR1_reg(RSRE, 1) |
		SSCR1_reg(TSRE, 1) |
		SSCR1_reg(TRAIL, 1);
	return sscr1;
}

/*
 * calculate_ssioc- Calculate ssioc register.
 */
static uint32_t calculate_ssioc(void)
{
	uint32_t ssioc = 0;

	ssioc = SSIOC_reg(SCOE, SSP_ENABLE_CLOCK);
	return ssioc;
}

/*
 * calculate_sscr2 - Calculate sscr2 register.
 */
static uint32_t calculate_sscr2(void)

{
	uint32_t sscr2 = 0;

	sscr2 |= SSCR2_reg(SDFD, SSP_DMA_FINISH_DISABLE) |
		 SSCR2_reg(TURM1, TRANSMIT_UNDERRUN_MODE_1_ENABLE);

	return sscr2;
}

/**
 * ace_dsp_core_power_up - power up the DSP primary_core
 * @bus: I2s bus pointer
 *
 * The boot sequence follows kernel driver implementation at
 * sound/soc/sof/intel/mtl.c/mtl_dsp_pre_fw_run().
 *
 * Returns 0 for boot success, -1 for failure.
 */
static int ace_dsp_core_power_up(I2s *bus)
{
	u32 val;

	/* Set the DSP subsystem to power up */
	write32(bus->lpe_bar4 + HFDSSCS,
		(read32(bus->lpe_bar4 + HFDSSCS) | HFDSSCS_SPA_MASK));

	/* Poll: first wait for unstable CPA (1 then 0 then 1) after SPA set */
	for (int i = 0; i < RETRY_COUNT; i++) {
		mdelay(1);
		if ((read32(bus->lpe_bar4 + HFDSSCS) &
				HFDSSCS_CPA_MASK) == HFDSSCS_CPA_MASK)
			break;
	}
	if ((read32(bus->lpe_bar4 + HFDSSCS) &
			HFDSSCS_CPA_MASK) != HFDSSCS_CPA_MASK) {
		printf("%s : failed to enable DSP subsystem\n", __func__);
		return -1;
	}

	/* Wake/Prevent gated-DSP0 from power gating */
	write32(bus->lpe_bar4 + HFPWRCTL,
		(read32(bus->lpe_bar4 + HFPWRCTL) | HFPWRCTL_WPDSPHPxPG(0)));

	for (int i = 0; i < RETRY_COUNT; i++) {
		if ((read32(bus->lpe_bar4 + HFPWRSTS) &
				HFPWRCTL_WPDSPHPxPG(0)) == HFPWRCTL_WPDSPHPxPG(0))
			break;
		mdelay(1);
	}
	if ((read32(bus->lpe_bar4 + HFPWRSTS) &
			HFPWRCTL_WPDSPHPxPG(0)) != HFPWRCTL_WPDSPHPxPG(0)) {
		printf("%s : failed to power up gated DSP domain\n", __func__);
		return -1;
	}

	/* Program Host CPU the owner of the IP & shim */
	val = read32(bus->lpe_bar4 + DSP2C0_CTL);
	write32(bus->lpe_bar4 + DSP2C0_CTL, (DSP2C0_OSEL_HOST(val) | DSP2C0_CTL_SPA_MASK));

	/* Poll: wait for unstable CPA (1 then 0 then 1) read first */
	for (int i = 0; i < RETRY_COUNT; i++) {
		mdelay(1);
		if ((read32(bus->lpe_bar4 + DSP2C0_CTL) &
				DSP2C0_CTL_CPA_MASK) == DSP2C0_CTL_CPA_MASK)
			break;
	}
	if ((read32(bus->lpe_bar4 + DSP2C0_CTL) &
			DSP2C0_CTL_CPA_MASK) != DSP2C0_CTL_CPA_MASK) {
		printf("%s : failed to power up primary core\n", __func__);
		return -1;
	}

	return 0;
}

/*
* Power on DSP and Enable SSP for data transmission
*/
static int enable_DSP_SSP(I2s *bus)
{
	/* Power On Audio Controller and wait till its powered on */
	write32(bus->lpe_bar0 + POWER_OFFSET, 0x1);
	for (int i = 0; i < RETRY_COUNT; i++) {
		if (read32(bus->lpe_bar0 + POWER_OFFSET) == 0x1)
			break;
		mdelay(1);
	}

	if (read32(bus->lpe_bar0 + POWER_OFFSET) != 0x1)
		return -1;

	/* Enable the ADSP bar fuctionality */
	write32(bus->lpe_bar0 + BAR_OFFSET, ENABLE_ADSP_BAR);
	for (int i = 0; i < RETRY_COUNT; i++) {
		if (read32(bus->lpe_bar0 + BAR_OFFSET) == ENABLE_ADSP_BAR)
			break;
		mdelay(1);
	}
	if (read32(bus->lpe_bar0 + BAR_OFFSET) != ENABLE_ADSP_BAR)
		return -1;

	if (CONFIG(INTEL_COMMON_I2S_ACE_1_x)) {
		if (ace_dsp_core_power_up(bus))
			return -1;
	} else {
		/* power on dsp core to access ssp registeres*/
		write32(bus->lpe_bar4 + DSP_POWER_OFFSET, DSP_POWER_ON);
		/* wait till the DSP powers on */
		for (int i = 0; i < RETRY_COUNT; i++) {
			if (read32(bus->lpe_bar4 + DSP_POWER_OFFSET) == DSP_POWERED_UP)
				break;
			mdelay(1);
		}
		if (read32(bus->lpe_bar4 + DSP_POWER_OFFSET) != DSP_POWERED_UP)
			return -1;
	}
/*
 * cAVS 1.5 version exposes the i2s registers used to configure
 * the clock and hence include the clock config.
 */
#if CONFIG(INTEL_COMMON_I2S_CAVS_1_5)
	/* setup the clock to disable dynamic clock gating of SSP */
	write32(bus->lpe_bar4 + CLOCK_GATING_OFFSET, DISABLE_CLOCK_GATING);
	for (int i = 0; i < RETRY_COUNT; i++) {
		if (read32(bus->lpe_bar4 + CLOCK_GATING_OFFSET) ==
						DISABLED_CLOCK_GATING)
			break;
		mdelay(1);
	}
	if (read32(bus->lpe_bar4 + CLOCK_GATING_OFFSET) != DISABLED_CLOCK_GATING)
		return -1;
#endif

#if CONFIG(INTEL_COMMON_I2S_CAVS_2_0) || CONFIG(INTEL_COMMON_I2S_CAVS_2_5)
	/* In cAVS 2.0 or cAVS 2.5, need to set I2S MDIV and NDIV */
	write32((bus->lpe_bar4 + (MNCSS_REG_BLOCK_START + MDIV_M_VAL(bus->ssp_port))), 1);
	write32((bus->lpe_bar4 + (MNCSS_REG_BLOCK_START + MDIV_N_VAL(bus->ssp_port))), 1);
#endif

#if CONFIG(INTEL_COMMON_I2S_CAVS_2_5) || CONFIG(INTEL_COMMON_I2S_ACE_1_x)
	/* SPA register should be set for each I2S port */
	write32((bus->lpe_bar4 + I2SLCTL),
	read32(bus->lpe_bar4 + I2SLCTL) | BIT(bus->ssp_port));
#endif

/* Tunning I2S signal BCLK 3.072Mhz for CS35L53 */
#if CONFIG(DRIVER_SOUND_CS35L53)
	write32((bus->lpe_bar4 + MNCSS_REG_BLOCK_START), 0x01 << 20);
#endif
	return 0;
}
/*
 * set_ssp_i2s_hw - Configure SSP driver according to settings.
 * @regs: pointer to registers
 * @settings: config settings
 * @bps: bits per sample
 *
 * Configure SSP driver according to settings.
 */
static void set_ssp_i2s_hw(I2sRegs *regs, const I2sSettings *settings, int bps)
{
	uint32_t sscr0;
	uint32_t sscr1;
	uint32_t sscr2;
	uint32_t sscr3;
	uint32_t sstsa;
	uint32_t ssrsa;
	uint32_t sspsp;
	uint32_t sspsp2 = 0;
	uint32_t sssr = 0;
	uint32_t ssioc;

	sscr0 = calculate_sscr0(settings, bps);
	sscr1 = calculate_sscr1(settings);
	sscr2 = calculate_sscr2();
	sscr3 = 0;
	sspsp = calculate_sspsp(settings, bps);
	sstsa = SSTSA_reg(TTSA, settings->ssp_active_tx_slots_map);
	ssrsa = SSRSA_reg(RTSA, settings->ssp_active_rx_slots_map);
	ssioc = calculate_ssioc();

	write_SSCR0(sscr0, regs);
	write_SSCR1(sscr1, regs);
	write_SSCR2(sscr2, regs);
	write_SSCR3(sscr3, regs);
	write_SSPSP(sspsp, regs);
	write_SSPSP2(sspsp2, regs);
	write_SSTSA(sstsa, regs);
	write_SSRSA(ssrsa, regs);
	write_SSIOC(ssioc, regs);

	/* Clear status */
	write_SSSR(sssr, regs);

	/* set the time out for the reception */
	write_SSTO(SSP_TIMEOUT, regs);

	printf("%s : sscr0 = 0x%08x, sscr1 = 0x%08x, sspsp = 0x%08x\n", __func__,
	       sscr0, sscr1, sspsp);
	printf("%s : sscr2 = 0x%08x, sspsp2 = 0x%08x, sscr3 = 0x%08x, ssioc = 0x%08x\n",
	       __func__, sscr2, sspsp2, sscr3, ssioc);
	printf("%s : ssrsa = 0x%08x, sstsa = 0x%08x\n", __func__, ssrsa, sstsa);
}

/*
 * i2s_init - Initialize I2s.
 * @bus: i2s config structure
 *
 * Initialize I2s.
 */
static int i2s_init(I2s *bus)
{
	if (enable_DSP_SSP(bus)) {
		printf("%s : Error during DSP initialization\n", __func__);
		return -1;
	}
	i2s_disable(bus->regs);
	set_ssp_i2s_hw(bus->regs,
		       bus->settings,
		       bus->bits_per_sample);
	return 0;
}

/*
 * i2s_send - Send audio samples to I2s controller.
 * @me: I2sOps structure
 * @data: Audio samples
 * @length: Number of samples
 *
 * Send audio samples to I2s controller.
*/
static int i2s_send(I2sOps *me, unsigned int *data, unsigned int length)
{
	int i;
	uint64_t last_activity;
	I2s *bus = container_of(me, I2s, ops);
	struct I2sRegs *i2s_reg = bus->regs;

	if (!bus->initialized) {
		if (i2s_init(bus))
			return -1;
		bus->initialized = 1;
	}

	if (length < LPE_SSP_FIFO_SIZE) {
		printf("%s : Invalid data size\n", __func__);
		return -1;
	}

	if (bus->sdmode_gpio)
		gpio_set(bus->sdmode_gpio, 1);

	for (i = 0; i < LPE_SSP_FIFO_SIZE; i++)
		write32(&i2s_reg->ssdr, *data++);

	i2s_enable(bus->regs);
	length -= LPE_SSP_FIFO_SIZE;

	last_activity = timer_us(0);
	while (length > 0) {
		if (read_SSSR(bus->regs) & TX_FIFO_NOT_FULL) {
			write32(&i2s_reg->ssdr, *data++);
			length--;
			last_activity = timer_us(0);
		} else {
			if (timer_us(last_activity) > 100000) {
				i2s_disable(bus->regs);
				if (bus->sdmode_gpio)
					gpio_set(bus->sdmode_gpio, 0);
				printf("I2S Transfer Timeout\n");
				return -1;
			}
		}
	}

	mdelay(1);
	if (bus->sdmode_gpio)
		gpio_set(bus->sdmode_gpio, 0);
	i2s_disable(bus->regs);
	return 0;
}

/*
 * new_i2s_structure - Allocate new I2s data structures.
 * @settings: Codec settigns
 * @bps: Bits per sample
 * @sdmode: Codec shut-down and mode pin
 * @ssp_i2s_start_address: I2S register start address.
 *
 * Allocate new I2s data structures.
 */
I2s *new_i2s_structure(const I2sSettings *settings, int bps, GpioOps *sdmode,
	uintptr_t ssp_i2s_start_address)
{
	I2s *bus = xzalloc(sizeof(*bus));
	pcidev_t lpe_pcidev = PCI_DEV(0, AUDIO_DEV, AUDIO_FUNC);

	bus->lpe_bar0 = (void *)(uintptr_t)(pci_read_config32(lpe_pcidev, REG_BAR0) & (~0xf));
	bus->lpe_bar4 = (void *)(uintptr_t)(pci_read_config32(lpe_pcidev, REG_BAR4) & (~0xf));
	bus->ops.send = &i2s_send;
	bus->regs = (I2sRegs *)(bus->lpe_bar4 + ssp_i2s_start_address);
	bus->settings = settings;
	bus->bits_per_sample = bps;
	bus->sdmode_gpio = sdmode;
#if CONFIG(INTEL_COMMON_I2S_CAVS_2_0) || CONFIG(INTEL_COMMON_I2S_CAVS_2_5) ||\
	CONFIG(INTEL_COMMON_I2S_ACE_1_x)
	bus->ssp_port = (ssp_i2s_start_address - SSP_I2S0_START_ADDRESS) / 0x1000;
#endif

	return bus;
}
