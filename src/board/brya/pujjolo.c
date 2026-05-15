// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include "board/brya/include/variant.h"
#include "drivers/ec/rts5453/rts5453.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/storage_common.h"

#define EC_PCH_INT_ODL		GPD2
#define MAX_PDC_PORT_NUM	2
#define PDC_FW_NAME_REVISION_INDEX	6
#define PDC_FW_NAME_VARIANT_INDEX	7

static const struct pdc_fw_map {
	const char *config_id;
	const char *image;
	const char *hash;
} pdc_fw_map[] = {
	{ "GOOG0901", "rts5453_GOOG0901.bin", "rts5453_GOOG0901.hash" },
	{ "GOOG0F01", "rts5453_GOOG0F01.bin", "rts5453_GOOG0F01.hash" },
	{ "GOOG0F02", "rts5453_GOOG0F02.bin", "rts5453_GOOG0F02.hash" },
	{ "GOOG0N01", "rts5453vb_GOOG0N01.bin", "rts5453vb_GOOG0N01.hash" },
	{ "GOOG0O01", "rts5453vb_GOOG0O01.bin", "rts5453vb_GOOG0O01.hash" },
	{ "GOOG0O02", "rts5453vb_GOOG0O02.bin", "rts5453vb_GOOG0O02.hash" },
};

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
					int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	assert(ec_pd_id >= 0 && ec_pd_id < MAX_PDC_PORT_NUM);
	*image_path = NULL;
	*hash_path = NULL;

	for (int i = 0; i < ARRAY_SIZE(pdc_fw_map); i++) {
		const struct pdc_fw_map *m = &pdc_fw_map[i];
		/*
		 * PDC FW 16.10.4 to 16.12.4 change revision fields
		 * (e.g. GOOG0O01 -> GOOG0O11). Ignore patching revision.
		 * Only need to match the first 6 chars and variant index.
		 */
		if (!strncmp(m->config_id, r->fw_name_str,
			     PDC_FW_NAME_REVISION_INDEX) &&
		    m->config_id[PDC_FW_NAME_VARIANT_INDEX] ==
			    r->fw_name_str[PDC_FW_NAME_VARIANT_INDEX]) {
			*image_path = m->image;
			*hash_path = m->hash;
			return;
		}
	}
	printf("Unknown PDC name (ec_pd_id %d): %s\n", ec_pd_id, r->fw_name_str);
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus = {
			.type = AUDIO_PWM,
			.pwm.pad = GPP_B14,
		}
	};

	return &config;
}

int cr50_irq_status(void)
{
	return alderlake_get_gpe(GPE0_DW0_13); /* GPP_A13 */
}

static const struct storage_config storage_configs[] = {
	{
		.media = STORAGE_UFS,
		.pci_dev = PCH_DEV_UFS1,
	},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCH_DEV_I2C0,
	};

	return &config;
}

const int variant_get_ec_int(void)
{
	return EC_PCH_INT_ODL;
}

void board_rts545x_register(void)
{
	/*
	 * Pujjolo/Pujjoquince's PDC FW might contain wrong PID 0x5072.
	 * Register it to allow updating to the newer versions.
	 *
	 * Add rts5453vb_pid 0x5078 to allow PD controller RTS5453P-VB updates.
	 */
	const uint32_t old_pid = 0x5072;
	const uint32_t rts5453vb_pid = 0x5078;
	static CrosEcAuxfwChipInfo rts5453_infos[] = {
		{
			.vid = CONFIG_DRIVER_EC_RTS545X_VID,
			.pid = old_pid,
			.new_chip_aux_fw_ops = new_rts545x_from_chip_info,
		},
		{
			.vid = CONFIG_DRIVER_EC_RTS545X_VID,
			.pid = rts5453vb_pid,
			.new_chip_aux_fw_ops = new_rts545x_from_chip_info,
		},
	};
	for (int i = 0; i < ARRAY_SIZE(rts5453_infos); i++)
		list_insert_after(&rts5453_infos[i].list_node, &ec_aux_fw_chip_list);
}
