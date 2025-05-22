/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#include <assert.h>
#include <libpayload.h>

#include "base/fw_config.h"
#include "base/init_funcs.h"
#include "board/skywalker/include/variant.h"
#include "drivers/bus/spi/mtk.h"
#include "drivers/bus/i2c/mtk_i2c.h"
#include "drivers/bus/i2s/mtk_v3.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/ec/rts5453/rts5453.h"
#include "drivers/flash/mtk_snfc.h"
#include "drivers/gpio/mtk_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/psci.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/rt1015p.h"
#include "drivers/sound/rt5645.h"
#include "drivers/storage/mtk_mmc.h"
#include "drivers/storage/mtk_ufs.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

#define MAX_PATH_LEN 32

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	assert(ec_pd_id >= 0 && ec_pd_id < PDC_PORT_NUM);

	const char *config = rts545x_configs[ec_pd_id];
	static char image[PDC_PORT_NUM][MAX_PATH_LEN];
	static char hash[PDC_PORT_NUM][MAX_PATH_LEN];
	static const char *pattern = "rts5453_GOOG%s00.%s";

	snprintf(image[ec_pd_id], MAX_PATH_LEN, pattern, config, "bin");
	snprintf(hash[ec_pd_id], MAX_PATH_LEN, pattern, config, "hash");

	*image_path = image[ec_pd_id];
	*hash_path = hash[ec_pd_id];
}

static int tpm_irq_status(void)
{
	static GpioOps *tpm_int;
	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					      new_mtk_eint);
	assert(tpm_int);
	return gpio_get(tpm_int);
}

static void enable_usb_vbus(struct UsbHostController *usb_host)
{
	/*
	 * To avoid USB detection issue, assert GPIO AP_XHCI_INIT_DONE
	 * to notify EC to enable USB VBUS when xHCI is initialized.
	 */
	GpioOps *pdn = sysinfo_lookup_gpio("XHCI init done", 1,
					   new_mtk_gpio_output);
	if (pdn) {
		gpio_set(pdn, 1);
		/* After USB VBUS is enabled, delay 500ms for USB detection. */
		mdelay(500);
	}
}

static void setup_setup_rt1019(GpioOps *spk_en)
{
	MtkI2s *mtk_i2s = new_mtk_i2s(0x11050000, 2, 32 * KHz, 16, 16, AFE_TDM_OUT1);
	I2sSource *i2s_source = new_i2s_source(&mtk_i2s->ops, 32 * KHz, 2, 8000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);

	rt1015pCodec *codec = new_rt1015p_codec(spk_en);
	SoundRouteComponent *speaker_amp = &codec->component;

	list_insert_after(&speaker_amp->list_node, &sound_route->components);
	list_insert_after(&mtk_i2s->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);
	printf("%s done\n", __func__);
}

static void setup_codec_rt9123(GpioOps *spk_en)
{
	MtkI2s *mtk_i2s = new_mtk_i2s(0x11050000, 2, 32 * KHz, 16, 16, AFE_TDM_OUT1);
	I2sSource *i2s_source = new_i2s_source(&mtk_i2s->ops, 32 * KHz, 2, 8000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);

	gpio_set(spk_en, 1);

	sound_set_ops(&sound_route->ops);
	printf("%s done\n", __func__);
}

static void setup_codec_alc5645(void)
{
	MtkI2s *mtk_i2s = new_mtk_i2s(0x11050000, 2, 48000, 16, 16, AFE_TDM_OUT0);
	I2sSource *i2s_source = new_i2s_source(&mtk_i2s->ops, 48000, 2, 8000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);

	MTKI2c *i2c2 = new_mtk_i2c(0x11B20000, 0x11300400, I2C_APDMA_ASYNC);
	rt5645Codec *rt5645 = new_rt5645_codec(&i2c2->ops, 0x1A);

	list_insert_after(&rt5645->component.list_node,
			  &sound_route->components);
	list_insert_after(&mtk_i2s->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);
	printf("%s done\n", __func__);
}

static void sound_setup(void)
{
	GpioOps *rt9123_spk_en = sysinfo_lookup_gpio("rt9123_spk_en", 1,
						     new_mtk_gpio_output);
	GpioOps *alc5645_spk_en = sysinfo_lookup_gpio("alc5645_spk_en", 1,
						      new_mtk_gpio_output);
	GpioOps *rt1019_spk_en = sysinfo_lookup_gpio("rt1019_spk_en", 1,
						     new_mtk_gpio_output);

	if (rt9123_spk_en)
		setup_codec_rt9123(rt9123_spk_en);
	else if (alc5645_spk_en)
		setup_codec_alc5645();
	else if (rt1019_spk_en)
		setup_setup_rt1019(rt1019_spk_en);
	else
		printf("no amps found\n");
}

static void rts545x_register(void)
{
	/*
	 * Skywalker's PDC FW 1.61.1 contains the wrong PID 0x5065.
	 * To allow updating from 1.61.1 to newer versions,
	 * register a fake chip with the wrong PID.
	 */
	const uint32_t old_pid = 0x5065;
	static CrosEcAuxfwChipInfo rts5453_info = {
		.vid = CONFIG_DRIVER_EC_RTS545X_VID,
		.pid = old_pid,
		.new_chip_aux_fw_ops = new_rts545x_from_chip_info,
	};
	list_insert_after(&rts5453_info.list_node, &ec_aux_fw_chip_list);
}

static void setup_ufs(void)
{
	MtkUfsCtlr *ufs_host = new_mtk_ufs_ctlr(0x112B0000, 0x112BD000);
	list_insert_after(&ufs_host->ufs.bctlr.list_node, &fixed_block_dev_controllers);
	printf("%s done\n", __func__);
}

static void setup_emmc(void)
{
	MtkMmcTuneReg emmc_tune_reg = {
		.msdc_iocon = 0x1 << 8,
		.pad_tune = 0x10 << 16,
	};
	MtkMmcHost *emmc = new_mtk_mmc_host(
		0x11230000, 0x11e70000, 400 * MHz, 200 * MHz, emmc_tune_reg, 8,
		0, NULL, MTK_MMC_V2);

	list_insert_after(&emmc->mmc.ctrlr.list_node, &fixed_block_dev_controllers);
	printf("%s done\n", __func__);
}

static int board_setup(void)
{
	sysinfo_install_flags(new_mtk_gpio_input);
	power_set_ops(&psci_power_ops);

	/* Set up TPM */
	MTKI2c *i2c3 = new_mtk_i2c(0x11D70000, 0x11300500, I2C_APDMA_ASYNC);
	GscI2c *tpm = new_gsc_i2c(&i2c3->ops, GSC_I2C_ADDR, &tpm_irq_status);
	tpm_set_ops(&tpm->base.ops);

	/* Set up EC */
	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	GpioOps *spi0_cs = new_gpio_not(new_mtk_gpio_output(PAD_SPIM0_CSB));
	MtkSpi *spi0 = new_mtk_spi(0x11010800, spi0_cs);
	CrosEcSpiBus *cros_ec_spi_bus = new_cros_ec_spi_bus(&spi0->ops);
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1, new_mtk_gpio_input);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_spi_bus->ops, ec_int);
	register_vboot_ec(&cros_ec->vboot);

	sound_setup();

	/* Set up NOR flash ops */
	MtkNorFlash *nor_flash = new_mtk_nor_flash(0x11018000);
	flash_set_ops(&nor_flash->ops);

	/* Set up storage */
	bool do_emmc_setup = false;
	bool do_ufs_setup = false;

	if (!fw_config_is_provisioned()) {
		do_emmc_setup = true;
		do_ufs_setup = true;
	} else if (fw_config_probe(FW_CONFIG(STORAGE, STORAGE_EMMC))) {
		do_emmc_setup = true;
	} else if (fw_config_probe(FW_CONFIG(STORAGE, STORAGE_UFS2X)) ||
		   fw_config_probe(FW_CONFIG(STORAGE, STORAGE_UFS3X))) {
		do_ufs_setup = true;
	}

	if (do_ufs_setup)
		setup_ufs();
	if (do_emmc_setup)
		setup_emmc();

	/* Set up SD card */
	MtkMmcTuneReg sd_card_tune_reg = {
		.msdc_iocon = 0x0,
		.pad_tune = 0x0,
	};
	GpioOps *card_detect_ops = sysinfo_lookup_gpio("SD card detect", 1,
						       new_mtk_gpio_input);
	if (!card_detect_ops) {
		printf("%s: SD card detect GPIO not found\n", __func__);
	} else {
		MtkMmcHost *sd_card = new_mtk_mmc_host(0x11240000,
						       0x11D800000,
						       200 * MHz,
						       25 * MHz,
						       sd_card_tune_reg,
						       4, 1, card_detect_ops,
						       MTK_MMC_V2);

		list_insert_after(&sd_card->mmc.ctrlr.list_node,
				  &removable_block_dev_controllers);
	}

	/* Set up USB */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0x11260000);
	set_usb_init_callback(usb_host, enable_usb_vbus);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	if (CONFIG(DRIVER_EC_RTS5453))
		rts545x_register();

	return 0;
}

INIT_FUNC(board_setup);
