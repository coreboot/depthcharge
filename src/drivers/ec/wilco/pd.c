// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Depthcharge driver and Verified Boot callbacks for the Wilco
 * PD Firmware update interface.
 */

#include <assert.h>
#include <cbfs_core.h>
#include <endian.h>
#include <libpayload.h>
#include <swab.h>
#include <vboot_api.h>
#include <vb2_api.h>

#include "base/container_of.h"
#include "drivers/ec/wilco/ec.h"
#include "drivers/ec/wilco/flash.h"
#include "drivers/ec/wilco/pd.h"
#include "drivers/ec/vboot_aux_fw.h"

enum wilco_pd_flash {
	PD_FLASH_CMD = 0xea,
	PD_FLASH_DATA_SIZE = 8,
};

/**
 * WilcoPdFlashStart - EC command to initiate a PD flash update sequence.
 * @command: Flash start command code for selected chip.
 * @location: %EcFlashLocation for location of the flash device.
 * @type: %EcFlashType for the type of flash device.
 * @sub_type: %EcFlashSubType for the sub-type of flash device.
 * @unused: Field is not used, set to zero.
 * @instance: %EcFlashInstance for selecting primary copy.
 */
typedef struct __attribute__((packed)) WilcoPdFlashStart {
	uint8_t command;
	uint8_t location;
	uint8_t type;
	uint8_t sub_type;
	uint8_t unused;
	uint8_t instance;
} WilcoPdFlashStart;

/**
 * WilcoPdFlashWrite - EC command to write data to PD controller.
 * @command: Flash command code for selected chip.
 * @unused: Field is not used, set to zero.
 * @size: Number of bytes in data payload.
 * @data: Data to write, max of %PD_FLASH_DATA_SIZE.
 */
typedef struct __attribute__((packed)) WilcoPdFlashWrite {
	uint8_t command;
	uint32_t unused;
	uint8_t size;
	uint8_t data[PD_FLASH_DATA_SIZE];
} WilcoPdFlashWrite;

static VbError_t wilco_pd_check_hash(const VbootAuxFwOps *vbaux,
				     const uint8_t *hash, size_t hash_size,
				     VbAuxFwUpdateSeverity_t *severity)
{
	WilcoPd *pd = container_of(vbaux, WilcoPd, ops);
	WilcoEcFlashDevice device;
	uint32_t update_version;

	*severity = VB_AUX_FW_NO_UPDATE;

	if (hash_size != sizeof(uint32_t))
		return VBERROR_UNKNOWN;
	memcpy(&update_version, hash, hash_size);

	/* Find EC flash device on mainboard */
	if (wilco_ec_flash_find(pd->ec, &device,
				EC_FLASH_LOCATION_MAIN,
				EC_FLASH_TYPE_TCPC,
				EC_FLASH_INSTANCE_PRIMARY) < 0) {
		printf("%s: Unable to find PD device\n", __func__);
		return VBERROR_SUCCESS;
	}

	printf("%s: TCPC version 0x%08x, ", __func__, device.version);

	if (device.version == update_version) {
		printf("no update\n");
	} else {
		printf("update to version 0x%08x\n", update_version);
		*severity = VB_AUX_FW_SLOW_UPDATE;
	}
	return VBERROR_SUCCESS;
}

static int wilco_pd_flash_start(WilcoPd *pd, WilcoPdFlashStart *start)
{
	uint8_t result = 0;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = PD_FLASH_CMD,
		.request_data = start,
		.request_size = sizeof(WilcoPdFlashStart),
		.response_data = &result,
		.response_size = sizeof(result),
	};
	int ret;

	if (!start)
		return -1;

	ret = wilco_ec_mailbox(pd->ec, &msg);
	if (ret < 0 || !result) {
		printf("%s: failed\n", __func__);
		return -1;
	}

	return 0;
}

static int wilco_pd_flash_erase(WilcoPd *pd, size_t image_size)
{
	uint8_t param = pd->info->erase_cmd;
	uint8_t result = 0;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = PD_FLASH_CMD,
		.request_data = &param,
		.request_size = sizeof(param),
		.response_data = &result,
		.response_size = sizeof(result),
	};
	int ret;

	printf("%s: erase...", __func__);

	ret = wilco_ec_mailbox(pd->ec, &msg);
	if (ret < 0 || !result) {
		printf("failed: ret=%d result=%d\n",
		       ret, result);
		return -1;
	}
	printf("ok\n");
	return 0;
}

static int wilco_pd_flash_write(WilcoPd *pd, const uint8_t *image,
				size_t image_size)
{
	size_t block, block_count;
	uint8_t result;
	WilcoPdFlashWrite write;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = PD_FLASH_CMD,
		.request_data = &write,
		.request_size = sizeof(WilcoPdFlashWrite),
		.response_data = &result,
		.response_size = sizeof(result),
	};
	int ret;

	block_count = image_size / PD_FLASH_DATA_SIZE;
	write.command = pd->info->write_cmd;
	write.size = PD_FLASH_DATA_SIZE;

	printf("%s: write...", __func__);

	/* Write each block */
	for (block = 0; block < block_count; ++block) {
		/* Copy next block of data */
		memcpy(write.data, image + block * write.size, write.size);

		ret = wilco_ec_mailbox(pd->ec, &msg);
		if (ret < 0 || !result) {
			printf("failed at block %zd: ret=%d result=%d\n",
			       block, ret, result);
			return -1;
		}
		if (!(block & 0xf))
			printf(".");
	}
	printf("ok\n");
	return 0;
}

static int wilco_pd_flash_finish(WilcoPd *pd)
{
	uint8_t param = pd->info->verify_cmd;
	uint8_t result = 0;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = PD_FLASH_CMD,
		.request_data = &param,
		.request_size = sizeof(param),
		.response_data = &result,
		.response_size = sizeof(result),
	};
	int ret;

	/* Verify flash write */
	printf("%s: verify...", __func__);
	ret = wilco_ec_mailbox(pd->ec, &msg);
	if (ret < 0 || !result) {
		printf("failed\n");
		return -1;
	}
	printf("ok\n");

	/* Send update done command */
	printf("%s: done...", __func__);
	param = pd->info->done_cmd;
	ret = wilco_ec_mailbox(pd->ec, &msg);
	if (ret < 0 || !result) {
		printf("failed\n");
		return -1;
	}
	printf("ok\n");

	return 0;
}

static VbError_t wilco_pd_update_image(const VbootAuxFwOps *vbaux,
				       const uint8_t *image,
				       const size_t image_size)
{
	WilcoPd *pd = container_of(vbaux, WilcoPd, ops);
	WilcoPdFlashStart start = {
		.command = pd->info->start_cmd,
		.location = EC_FLASH_LOCATION_MAIN,
		.type = EC_FLASH_TYPE_TCPC,
		.sub_type = pd->info->flash_subtype,
		.instance = EC_FLASH_INSTANCE_PRIMARY,
	};
	int ret;

	ret = wilco_pd_flash_start(pd, &start);
	if (ret < 0)
		return ret;

	ret = wilco_pd_flash_erase(pd, image_size);
	if (ret < 0)
		return ret;

	ret = wilco_pd_flash_write(pd, image, image_size);
	if (ret < 0)
		return ret;

	ret = wilco_pd_flash_finish(pd);
	if (ret < 0)
		return ret;

	return VBERROR_SUCCESS;
}

WilcoPd *new_wilco_pd(WilcoEc *ec, WilcoPdFlashInfo *info,
		      const char *fw_image_name, const char *fw_hash_name)
{
	WilcoPd *pd = xzalloc(sizeof(*pd));

	pd->ec = ec;

	pd->ops.update_image = &wilco_pd_update_image;
	pd->ops.check_hash = &wilco_pd_check_hash;
	pd->ops.fw_image_name = fw_image_name;
	pd->ops.fw_hash_name = fw_hash_name;
	pd->info = info;

	return pd;
}
