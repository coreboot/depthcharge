// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>
#include "board/ocelot/include/variant.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/bus/soundwire/soundwire.h"

#define EC_SOC_INT_ODL		GPP_E08
#define MAX_PDC_PORT_NUM	2
#define PDC_FW_NAME_REVISION_INDEX	6
#define PDC_FW_NAME_VARIANT_INDEX	7

static const struct pdc_fw_map {
	const char *config_id;
	const char *image;
	const char *hash;
} pdc_fw_map[] = {
	{ "GOOG0N02", "rts5453vb_GOOG0N02.bin", "rts5453vb_GOOG0N02.hash" },
	{ "GOOG0O03", "rts5453vb_GOOG0O03.bin", "rts5453vb_GOOG0O03.hash" },
};

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE1 },
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

int gsc_irq_status(void)
{
	return pantherlake_get_gpe(GPE0_DW2_14); /* GPP_E14 */
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus = {
			.type = AUDIO_SNDW,
			.sndw.link = AUDIO_SNDW_LINK3,
		},
		.codec = {
			.type = AUDIO_RT721,
		},
	};

	return &config;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCI_DEV_I2C1,
	};

	return &config;
}

const int variant_get_ec_int(void)
{
	return EC_SOC_INT_ODL;
}

void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	assert(ec_pd_id >= 0 && ec_pd_id < MAX_PDC_PORT_NUM);
	*image_path = NULL;
	*hash_path = NULL;

	for (int i = 0; i < ARRAY_SIZE(pdc_fw_map); i++) {
		const struct pdc_fw_map *m = &pdc_fw_map[i];
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
