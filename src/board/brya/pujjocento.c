// SPDX-License-Identifier: GPL-2.0

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
#define PDC_RTS5453_BYPASS		"GOOG0901"
#define PDC_RTS5453_TUSB546		"GOOG0F00"
#define PDC_RTS5453VB_BYPASS	"GOOG0N01"
#define PDC_RTS5453VB_TUSB546	"GOOG0O01"

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	switch (ec_pd_id) {
	case 0:
		/* bypass retimer */
		if (!strcmp(r->fw_name_str, PDC_RTS5453_BYPASS)) {
			*image_path = "rts5453_GOOG0901.bin";
			*hash_path = "rts5453_GOOG0901.hash";
		} else if (!strcmp(r->fw_name_str, PDC_RTS5453VB_BYPASS)) {
			*image_path = "rts5453vb_GOOG0N01.bin";
			*hash_path = "rts5453vb_GOOG0N01.hash";
		} else {
			*image_path = NULL;
			*hash_path = NULL;
		}
		break;
	case 1:
		/* TUSB546/1044 */
		if (!strcmp(r->fw_name_str, PDC_RTS5453_TUSB546)) {
			*image_path = "rts5453_GOOG0F00.bin";
			*hash_path = "rts5453_GOOG0F00.hash";
		} else if (!strcmp(r->fw_name_str, PDC_RTS5453VB_TUSB546)) {
			*image_path = "rts5453vb_GOOG0O01.bin";
			*hash_path = "rts5453vb_GOOG0O01.hash";
		} else {
			*image_path = NULL;
			*hash_path = NULL;
		}
		break;
	default:
		printf("Unknown ec_pd_id %d\n", ec_pd_id);
	}
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
	* Pujjocento/Pujjoteenlo's PDC FW 1.71.1 contains the wrong PID 0x5072.
	* To allow updating from 1.71.1 to newer versions,
	* register a fake chip with the wrong PID.
	*
	* Add rts5453vb_pid 0x5078 to allow PD controller RTS5452G-CG updates.
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
