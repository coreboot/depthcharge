// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright 2021 Google LLC.  */

#include <assert.h>
#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/pci/pci.h"
#include "drivers/sound/amd_acp.h"
#include "drivers/sound/amd_acp_private.h"
#include "drivers/timer/timer.h"
#include "pci.h"

#if CONFIG_DRIVER_SOUND_AMD_ACP_V1
	#include "drivers/sound/amd_acp_v1.h"
#endif /* CONFIG_DRIVER_SOUND_AMD_ACP_V1 */
#if CONFIG_DRIVER_SOUND_AMD_ACP_V2
	#include "drivers/sound/amd_acp_v2.h"
#endif /* CONFIG_DRIVER_SOUND_AMD_ACP_V2 */

#define ACP_DEBUG 0

#if ACP_DEBUG
#define acp_debug(...) printf(__VA_ARGS__)
#else
#define acp_debug(...)                                                         \
	do { } while (0)
#endif

/*
 * Generates a square wave tone table that can be used in a ring buffer and play
 * for an arbitrary amount of time.
 */
unsigned int amd_i2s_square_wave(AmdAcpPrivate *acp, int16_t **data,
				 unsigned int frequency)
{
	/* 4 periods generates a tone table that works well in the ring buffer.
	 */
	const unsigned int periods = 4;
	unsigned int samples_per_period;
	unsigned int first_half, second_half;
	unsigned int max_hz = acp->lrclk / 2;
	unsigned int bytes_per_period;
	unsigned int total_bytes;

	if (frequency > max_hz) {
		printf("ERROR: frequency %u > max_hz %d\n", frequency, max_hz);
		return 0;
	}

	samples_per_period = acp->lrclk / frequency;
	first_half = samples_per_period /
		     2; /* Integer division will round this down */
	second_half = samples_per_period - first_half;

	bytes_per_period =
		samples_per_period * acp->channels * sizeof(uint16_t);

	total_bytes = bytes_per_period * periods;

	acp_debug("%s: lrclk: %u, frequency: %u, samples_per_period: %u, "
		  "first_half: %u, second_half: %u, bytes_per_period: %u, "
		  "periods: %u, total_bytes: %u, volume: %#x\n",
		  __func__, acp->lrclk, frequency, samples_per_period,
		  first_half, second_half, bytes_per_period, periods,
		  total_bytes, acp->volume);

	int16_t *samples = xmalloc(total_bytes);
	*data = samples;

	for (unsigned int period = 0; period < periods; ++period) {
		for (unsigned int sample = 0; sample < first_half; sample++) {
			for (unsigned int channel = 0; channel < acp->channels;
			     ++channel) {
				*samples++ = acp->volume;
			}
		}

		for (unsigned int sample = 0; sample < second_half; sample++) {
			for (unsigned int channel = 0; channel < acp->channels;
			     ++channel) {
				*samples++ = -acp->volume;
			}
		}
	}

	return total_bytes;
}

void acp_write32(AmdAcpPrivate *acp, uint32_t reg, uint32_t val)
{
	write32(acp->pci_bar + reg, val);
}

uint32_t acp_read32(AmdAcpPrivate *acp, uint32_t reg)
{
	return read32(acp->pci_bar + reg);
}

static int amd_acp_stop(SoundOps *me)
{
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.sound_ops);
	uint32_t reg;
	struct stopwatch sw;

	reg = read32(&acp->audio_buffer_ctrl->iter);

	write32(&acp->audio_buffer_ctrl->iter, reg & ~ACP_I2STDM_TXEN);

	stopwatch_init_usecs_expire(&sw, 1000);
	while (read32(&acp->audio_buffer_ctrl->iter) & ACP_I2STDM_TX_STATUS) {
		if (stopwatch_expired(&sw)) {
			printf("%s: ERROR: Timed out waiting channel to stop\n",
			       __func__);
			return -1;
		}
	}

	write32(&acp->audio_buffer_ctrl->iter, 0);
	write32(&acp->audio_buffer_ctrl->ier, 0);

	return acp_check_error_status(acp);
}

static int amd_acp_set_volume(struct SoundOps *me, uint32_t volume)
{
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.sound_ops);

	/* Convert volume to percentage of full-scale int16_t. */
	acp->volume = (volume * 0x7fff) / 100;

	return 0;
}

static uint8_t amd_acp_get_power_status(AmdAcpPrivate *acp)
{
	uint32_t reg = acp_read32(acp, ACP_PGFSM_STATUS);
	return reg & ACP_PGFSM_POWER_MASK;
}

static int amd_acp_power_on(AmdAcpPrivate *acp)
{
	uint32_t reg;
	struct stopwatch sw;

	reg = acp_read32(acp, ACP_PGFSM_CONTROL);

	if ((reg & ACP_PGFSM_CTRL) == 0) {
		acp_debug("%s: ACP is powered off, power on\n", __func__);
		acp_write32(acp, ACP_PGFSM_CONTROL, reg | ACP_PGFSM_CTRL);
	}

	stopwatch_init_usecs_expire(&sw, 1000);
	while (amd_acp_get_power_status(acp) != ACP_PGFSM_POWER_ON) {
		if (stopwatch_expired(&sw)) {
			printf("%s: ERROR: Timed out while power on ACP\n",
			       __func__);
			return -1;
		}
	}

	return 0;
}

static int amd_acp_clk_power_on(AmdAcpPrivate *acp)
{
	uint32_t reg;
	struct stopwatch sw;
	reg = acp_read32(acp, ACP_CONTROL);

	if ((reg & ACP_CONTROL_CLK_EN) == 0) {
		acp_debug("%s: ACP CLK is disabled, enable\n", __func__);
		acp_write32(acp, ACP_CONTROL, reg | ACP_CONTROL_CLK_EN);
	}

	stopwatch_init_usecs_expire(&sw, 1000);
	while (!(acp_read32(acp, ACP_STATUS) & ACP_CONTROL_CLK_ON)) {
		if (stopwatch_expired(&sw)) {
			printf("%s: ERROR: Timed out starting clock", __func__);
			return -1;
		}
	}

	return 0;
}

static int amd_acp_deassert_reset(AmdAcpPrivate *acp)
{
	uint32_t reg;
	struct stopwatch sw;

	// Force a soft reset
	reg = ACP_SOFT_RESET_AUD | ACP_SOFT_RESET_DMA | ACP_SOFT_RESET_DSP;
	acp_write32(acp, ACP_SOFT_RESET, reg);

	reg = acp_read32(acp, ACP_SOFT_RESET);
	if (reg) {
		acp_debug("%s: ACP is in reset, ACP_SOFT_RESET: %#x\n",
			__func__, reg);
		acp_write32(acp, ACP_SOFT_RESET, 0);
	} else {
		acp_debug("%s: Failed to soft reset ACP, ACP_SOFT_RESET: %#x\n",
			__func__, reg);
	}

	stopwatch_init_usecs_expire(&sw, 1000);
	while (acp_read32(acp, ACP_SOFT_RESET) != 0) {
		if (stopwatch_expired(&sw)) {
			printf("%s: ERROR: Timed out de-asserting reset",
			       __func__);
			return -1;
		}
	}

	return 0;
}

static int amd_acp_enable(SoundRouteComponentOps *me)
{
	uintptr_t bar;
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.component.ops);

	acp_debug("%s: Getting BAR for PCI %d:%d.%d\n",
		__func__, PCI_BUS(acp->pci_dev), PCI_SLOT(acp->pci_dev),
		PCI_FUNC(acp->pci_dev));

	if (get_pci_bar(acp->pci_dev, &bar)) {
		printf("%s: ERROR: Failed to get BAR for PCI %d:%d.%d\n",
		       __func__, PCI_BUS(acp->pci_dev), PCI_SLOT(acp->pci_dev),
		       PCI_FUNC(acp->pci_dev));
		return -1;
	}

	acp->pci_bar = (uint8_t *)bar;
	acp_debug("%s: Got BAR %p\n", __func__, acp->pci_bar);

	switch (acp->port) {
	case ACP_OUTPUT_PLAYBACK:
		acp->audio_buffer =
			(void *)(acp->pci_bar + ACP_I2S_TX_RINGBUFADDR);
		acp->audio_buffer_ctrl =
			(void *)(acp->pci_bar + ACP_I2STDM_IER);
		break;
	case ACP_OUTPUT_BT:
		acp->audio_buffer =
			(void *)(acp->pci_bar + ACP_BT_TX_RINGBUFADDR);
		acp->audio_buffer_ctrl = (void *)(acp->pci_bar + ACP_BTTDM_IER);
		break;
	default:
		printf("%s: ERROR: Unknown port %u\n", __func__, acp->port);
	}

	acp_clear_error_status(acp);

	if (amd_acp_power_on(acp))
		return -1;

	if (amd_acp_clk_power_on(acp))
		return -1;

	if (amd_acp_deassert_reset(acp))
		return -1;

	save_pin_config(acp);

	return acp_check_error_status(acp);
}

static int amd_acp_disable(SoundRouteComponentOps *me)
{
	AmdAcpPrivate *acp = container_of(me, AmdAcpPrivate, ext.component.ops);

	acp_debug("%s: start\n", __func__);

	restore_pin_config(acp);

	return 0;
}

AmdAcp *new_amd_acp(pcidev_t pci_dev, enum acp_output_port port, uint32_t lrclk,
		    int16_t volume)
{
	AmdAcpPrivate *acp = xzalloc(sizeof(*acp));

	acp->ext.component.ops.enable = &amd_acp_enable;
	acp->ext.component.ops.disable = &amd_acp_disable;

	acp->ext.sound_ops.start = &amd_acp_start;
	acp->ext.sound_ops.stop = &amd_acp_stop;
	acp->ext.sound_ops.set_volume = &amd_acp_set_volume;

	acp->port = port;
	acp->pci_dev = pci_dev;
	acp->lrclk = lrclk;
	acp->channels = 2;
	acp->volume = volume;

	return &acp->ext;
}
