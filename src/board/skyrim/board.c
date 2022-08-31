// SPDX-License-Identifier: GPL-2.0
/* Copyright 2022 Google LLC.  */

#include <libpayload.h>

#include "base/fw_config.h"
#include "base/init_funcs.h"
#include "base/list.h"
#include "board/skyrim/include/variant.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/mendocino.h"
#include "drivers/sound/amd_acp.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/route.h"
#include "drivers/sound/rt1019.h"
#include "drivers/sound/rt5682s.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/tpm.h"
#include "pci.h"
#include "vboot/util/flag.h"

#define CR50_INT_18	18
#define EC_SOC_INT_ODL	84

__weak const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		/* Coreboot devicetree.cb: gpp_bridge_2 = 02.3 */
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV(0, 0x02, 0x3)},
		/* Coreboot devicetree.cb: gpp_bridge_1 = 02.2 */
		{ .media = STORAGE_RTKMMC, .pci_dev = PCI_DEV(0, 0x02, 0x02)},
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

/* Clock frequencies */
#define LRCLK			8000

/* Audio Configuration */
#define AUDIO_I2C_MMIO_ADDR	0xfedc4000
#define AUDIO_I2C_SPEED		400000
#define I2C_DESIGNWARE_CLK_MHZ	150

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(CR50_INT_18);

	return gpio_get(&tpm_gpio->ops);
}

static GpioOps *mkbp_int_ops(void)
{
	KernGpio *mkbp_int_gpio = new_kern_fch_gpio_input(EC_SOC_INT_ODL);
	/* Active-low, has to be inverted */
	return new_gpio_not(&mkbp_int_gpio->ops);
}

__weak enum audio_amp variant_get_audio_amp(void)
{
	return AUDIO_AMP_INVALID;
}

__weak unsigned int variant_get_en_spkr_gpio(void)
{
	return EN_SPKR;
}

static void setup_amd_acp_i2s(pcidev_t acp_pci_dev)
{
	enum audio_amp amp = variant_get_audio_amp();
	if (amp == AUDIO_AMP_INVALID) {
		printf("%s: Failed to get audio amp.\n", __func__);
		return;
	}

	AmdAcp *acp = new_amd_acp(acp_pci_dev, ACP_OUTPUT_PLAYBACK, LRCLK,
				  0x1FFF /* Volume */);

	SoundRoute *sound_route = new_sound_route(&acp->sound_ops);

	KernGpio *en_spkr_gpio =
		new_kern_fch_gpio_output(variant_get_en_spkr_gpio(), 0);

	/* Note: component order matters here. The first thing inserted is the
	 * last thing enabled. */

	/* If a warm reboot was performed, we need to clear out any settings the
	 * OS has made to the RT1019. */
	if (amp == AUDIO_AMP_RT1019) {
		DesignwareI2c *i2c = new_designware_i2c(
			(uintptr_t)AUDIO_I2C_MMIO_ADDR,
			AUDIO_I2C_SPEED, I2C_DESIGNWARE_CLK_MHZ);

		rt1019Codec *speaker_amp =
			new_rt1019_codec(&i2c->ops, AUD_RT1019_DEVICE_ADDR_R);

		list_insert_after(&speaker_amp->component.list_node,
				  &sound_route->components);

		GpioAmpCodec *en_spkr = new_gpio_amp_codec(&en_spkr_gpio->ops);

		/* We need to enable the speaker amp before we can send I2C
		 * transactions. */
		list_insert_after(&en_spkr->component.list_node,
				  &sound_route->components);
	} else if (amp == AUDIO_AMP_MAX98360) {
		/* Give the MAX98360A time to sample the BCLK */
		GpioAmpCodec *en_spkr =
			new_gpio_amp_codec_with_delay(&en_spkr_gpio->ops, 1000);

		/* Enable the speaker amp last since we have to delay to sample
		 * the clocks. */
		list_insert_after(&en_spkr->component.list_node,
				  &sound_route->components);
	} else {
		printf("%s: Unknown audio amp: %d\n", __func__, amp);
		return;
	}

	/* Enable the ACP so it can start generating the clocks. */
	list_insert_after(&acp->component.list_node, &sound_route->components);

	sound_set_ops(&sound_route->ops);
}

static int board_setup(void)
{
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, mkbp_int_ops());
	register_vboot_ec(&cros_ec->vboot);

	sysinfo_install_flags(NULL);
	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	pcidev_t acp_pci_dev;
	if (pci_find_device(AMD_PCI_VID, AMD_FAM17H_ACP_PCI_DID, &acp_pci_dev))
		setup_amd_acp_i2s(acp_pci_dev);

	/* Set up Dauntless on I2C3 */
	DesignwareI2c *i2c_ti50 = new_designware_i2c(
		AP_I2C3_ADDR, 400000, AP_I2C_CLK_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c_ti50->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	power_set_ops(&kern_power_ops);

	return 0;
}

INIT_FUNC(board_setup);
