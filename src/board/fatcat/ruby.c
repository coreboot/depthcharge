// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/fw_config.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/tas2563.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/rts5453/rts5453.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/sound/intel_audio_setup.h"
#include "drivers/sound/tas2563.h"
#define AUD_I2C_ADDR1	0x4e
#define AUD_I2C_ADDR2	0x4f
#define AUD_I2C_ADDR3	0x4c
#define AUD_I2C_ADDR4	0x4d


void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id,
				   struct ec_response_pd_chip_info_v2 *r)
{
	*image_path = "rts5453vb_GOOG0P00.bin";
	*hash_path = "rts5453vb_GOOG0P00.hash";
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	if (CONFIG(DRIVER_SOUND_I2S) &&
	    fw_config_probe(FW_CONFIG(AUDIO_AMPLIFIER, AUDIO_AMPLIFIER_TAS2563))) {
		config = (struct audio_config) {
			.bus = {
				.type = AUDIO_I2S,
				.i2s.address = SSP_I2S1_START_ADDRESS,
				.i2s.settings = &tas2563_settings,
			},
			.codec = {
				.type = AUDIO_TAS2563,
				.i2c[0].ctrlr = PCI_DEV_I2C0,
				.i2c[0].i2c_addr[0] = AUD_I2C_ADDR1,
				.i2c[0].i2c_addr[1] = AUD_I2C_ADDR2,
				.i2c[0].i2c_addr[2] = AUD_I2C_ADDR3,
				.i2c[0].i2c_addr[3] = AUD_I2C_ADDR4,
			},
		};
	} else {
		printf("Implement varaint audio config for other FW CONFIG options\n");
	}

	return &config;
}

static const struct storage_config storage_configs[] = {
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE5,
	},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

int gsc_irq_status(void)
{
	return pantherlake_get_gpe(GPE0_DW2_02); /* GPP_E02 */
}

void board_rts545x_register(void)
{
	/*
	 * Ruby's PDC FW 0.1.2 contains the wrong PID 0x5083.
	 * To allow updating from 0.1.2 to newer versions,
	 * register a fake chip with the wrong PID.
	 */
	const uint32_t old_pid = 0x5083;
	static CrosEcAuxfwChipInfo rts5453_info = {
		.vid = CONFIG_DRIVER_EC_RTS545X_VID,
		.pid = old_pid,
		.new_chip_aux_fw_ops = new_rts545x_from_chip_info,
	};
	list_insert_after(&rts5453_info.list_node, &ec_aux_fw_chip_list);
	printf("%s\n", __func__);
}
