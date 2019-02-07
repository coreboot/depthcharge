// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Flash update interface for the Wilco Embedded Controller.
 */

#include <assert.h>
#include <libpayload.h>
#include <vboot_api.h>

#include "base/container_of.h"
#include "drivers/ec/wilco/ec.h"
#include "drivers/ec/wilco/flash.h"

/**
 * enum WilcoEcFlash - Information about flash protocol.
 * @EC_FLASH_CMD: Command code for flash commands.
 * @EC_PROTO_DATA_SIZE: Max number of bytes in EC protocol data payload.
 * @EC_FLASH_DATA_SIZE: Max number of bytes in flash payload.
 * @EC_FLASH_ERASE_SIZE: Flash erase block size.
 * @EC_FLASH_RETRY: Number of times to retry a flash command.
 */
enum WilcoEcFlash {
	EC_FLASH_CMD = 0xea,
	EC_PROTO_DATA_SIZE = 32,
	EC_FLASH_DATA_SIZE = 8,
	EC_FLASH_ERASE_SIZE = 4 * KiB,
	EC_FLASH_RETRY = 5,
};

enum ec_flash_cmd {
	EC_FLASH_CMD_START = 0,
	EC_FLASH_CMD_PAGE_ERASE = 1,
	EC_FLASH_CMD_READ_DATA = 3,
	EC_FLASH_CMD_WRITE_DATA  = 4,
	EC_FLASH_CMD_FINISH = 5,
	EC_FLASH_CMD_DIGEST = 6,
	EC_FLASH_CMD_HW_QUERY_START = 16,
	EC_FLASH_CMD_HW_QUERY_NEXT = 17,
};

/**
 * struct WilcoEcFlashQuery - Flash data structure returned by the EC.
 * @unused: Byte is unused in this command.
 * @device: Flash device to query.
 *
 * This wraps the WilcoEcFlashDevice structure with an unused
 * byte so it matches the underlying protocol.
 */
typedef struct __attribute__((packed)) WilcoEcFlashQuery {
	uint8_t unused;
	WilcoEcFlashDevice device;
} WilcoEcFlashQuery;

/**
 * struct WilcoEcFlashStart - EC command to initiate a flash update sequence.
 * @command: Flash command code %EC_FLASH_CMD.
 * @location: %EcFlashLocation for location of the flash device.
 * @type: %EcFlashType for the type of flash device.
 * @sub_type: %EcFlashSubType for the sub-type of flash device.
 * @unused: Field is not used.
 * @instance: %EcFlashInstance for selecting primary or failsafe copy.
 * @payload_size: Length of image to flash.
 * @version: Version of the new firmware.
 */
typedef struct __attribute__((packed)) WilcoEcFlashStart {
	uint8_t command;
	uint8_t location;
	uint8_t type;
	uint8_t sub_type;
	uint8_t unused;
	uint8_t instance;
	uint32_t payload_size;
	uint32_t version;
} WilcoEcFlashStart;

/**
 * struct WilcoEcFlashErase - EC command to erase a page of flash.
 * @command: Flash command code %EC_FLASH_CMD.
 * @address: Address to erase page at.
 */
typedef struct __attribute__((packed)) WilcoEcFlashErase {
	uint8_t command;
	uint32_t address;
} WilcoEcFlashErase;

/**
 * struct WilcoEcFlashWrite - EC command to write data to flash.
 * @command: Flash command code %EC_FLASH_CMD.
 * @address: Address to write data at.
 * @size: Number of bytes in data payload.
 * @data: Data to write, max of %EC_FLASH_DATA_SIZE.
 */
typedef struct __attribute__((packed)) WilcoEcFlashWrite {
	uint8_t command;
	uint32_t address;
	uint8_t size;
	uint8_t data[EC_FLASH_DATA_SIZE];
} WilcoEcFlashWrite;

static int wilco_ec_flash_query_start(WilcoEc *ec)
{
	uint8_t param = EC_FLASH_CMD_HW_QUERY_START;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = EC_FLASH_CMD,
		.request_data = &param,
		.request_size = sizeof(param),
	};

	return wilco_ec_mailbox(ec, &msg);
}

static int wilco_ec_flash_query_next(WilcoEc *ec, WilcoEcFlashQuery *query)
{
	uint8_t param = EC_FLASH_CMD_HW_QUERY_NEXT;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.command = EC_FLASH_CMD,
		.request_data = &param,
		.request_size = sizeof(param),
		.response_data = query,
		.response_size = sizeof(WilcoEcFlashQuery),
	};

	if (!query)
		return -1;

	return wilco_ec_mailbox(ec, &msg);
}

int wilco_ec_flash_find(WilcoEc *ec, WilcoEcFlashDevice *device,
			EcFlashLocation location, EcFlashType type,
			EcFlashInstance instance)
{
	WilcoEcFlashQuery query;
	int ret;

	/* Reset flash device query */
	ret = wilco_ec_flash_query_start(ec);
	if (ret < 0)
		return ret;

	for (;;) {
		/* Query for next flash device */
		ret = wilco_ec_flash_query_next(ec, &query);
		if (ret < 0)
			break;

		/* Check if it matches the requested device */
		if (query.device.location == location &&
		    query.device.type == type &&
		    query.device.instance == instance) {
			memcpy(device, &query.device, sizeof(*device));
			return 0;
		}
	}
	return ret;
}

static int wilco_ec_flash_start(WilcoEc *ec, WilcoEcFlashStart *start)
{
	uint8_t result = 0;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.flags = WILCO_EC_FLAG_RAW_RESPONSE,
		.command = EC_FLASH_CMD,
		.request_data = start,
		.request_size = sizeof(WilcoEcFlashStart),
		.response_data = &result,
		.response_size = sizeof(result),
	};

	if (!start)
		return -1;

	if (wilco_ec_mailbox(ec, &msg) < 0 || !result) {
		printf("%s: failed\n", __func__);
		return -1;
	}
	return 0;
}

static int wilco_ec_flash_erase(WilcoEc *ec, size_t image_size)
{
	size_t page, page_count;
	WilcoEcFlashErase erase;
	uint8_t result = 0;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.flags = WILCO_EC_FLAG_RAW_RESPONSE,
		.command = EC_FLASH_CMD,
		.request_data = &erase,
		.request_size = sizeof(WilcoEcFlashErase),
		.response_data = &result,
		.response_size = sizeof(result),
	};
	int ret, try;

	/* Determine number of pages to erase based on image size */
	page_count = div_round_up(image_size, EC_FLASH_ERASE_SIZE);
	erase.command = EC_FLASH_CMD_PAGE_ERASE;

	printf("%s: erasing...", __func__);

	/* Erase each page */
	for (page = 0; page < page_count; ++page) {
		erase.address = page * EC_FLASH_ERASE_SIZE;

		/* Retry if erase command fails */
		for (try = 0; try < EC_FLASH_RETRY; ++try) {
			ret = wilco_ec_mailbox(ec, &msg);
			if (ret > 0 && result)
				break;
		}
		if (try == EC_FLASH_RETRY) {
			printf("failed at 0x%08x: ret=%d result=%d\n",
			       erase.address, ret, result);
			return -1;
		}
	}
	printf("done\n");
	return 0;
}

static int wilco_ec_flash_write(WilcoEc *ec, const uint8_t *image,
				size_t image_size)
{
	size_t block, block_count;
	WilcoEcFlashWrite write;
	uint8_t result;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.flags = WILCO_EC_FLAG_RAW_RESPONSE,
		.command = EC_FLASH_CMD,
		.request_data = &write,
		.request_size = sizeof(WilcoEcFlashWrite),
		.response_data = &result,
		.response_size = sizeof(result),
	};
	int ret, try;

	block_count = image_size >> 3;
	write.command = EC_FLASH_CMD_WRITE_DATA;
	write.size = EC_FLASH_DATA_SIZE;

	printf("%s: writing...", __func__);

	/* Write each block */
	for (block = 0; block < block_count; ++block) {
		/* Prepare block address and data */
		write.address = block * write.size;
		memcpy(write.data, image + write.address, write.size);

		/* Retry if write command fails */
		for (try = 0; try < EC_FLASH_RETRY; ++try) {
			ret = wilco_ec_mailbox(ec, &msg);
			if (ret > 0 && result)
				break;
		}
		if (try == EC_FLASH_RETRY) {
			printf("failed at 0x%08x\n", write.address);
			return -1;
		}
		if (!(block & 0xff))
			printf(".");
	}
	printf("done\n");
	return 0;
}

static int wilco_ec_flash_finish(WilcoEc *ec)
{
	uint8_t param = EC_FLASH_CMD_FINISH;
	uint8_t result = 0;
	WilcoEcMessage msg = {
		.type = WILCO_EC_MSG_LEGACY,
		.flags = WILCO_EC_FLAG_RAW_RESPONSE,
		.command = EC_FLASH_CMD,
		.request_data = &param,
		.request_size = sizeof(param),
		.response_data = &result,
		.response_size = sizeof(result),
	};
	int ret;

	ret = wilco_ec_mailbox(ec, &msg);
	if (ret < 0 || !result) {
		printf("%s: flash failed\n", __func__);
		return -1;
	}

	printf("%s: flash successful\n", __func__);
	return 0;
}

int wilco_ec_flash_image(WilcoEc *ec, WilcoEcFlashDevice *device,
			 const uint8_t *image, int image_size,
			 uint32_t version)
{
	WilcoEcFlashStart start = {
		.command = EC_FLASH_CMD_START,
		.location = device->location,
		.type = device->type,
		.sub_type = device->sub_type,
		.instance = device->instance,
		.payload_size = image_size,
		.version = version,
	};
	int ret;

	ret = wilco_ec_flash_start(ec, &start);
	if (ret < 0)
		return ret;

	ret = wilco_ec_flash_erase(ec, image_size);
	if (ret < 0)
		return ret;

	ret = wilco_ec_flash_write(ec, image, image_size);
	if (ret < 0)
		return ret;

	ret = wilco_ec_flash_finish(ec);
	if (ret < 0)
		return ret;

	return ret;
}
