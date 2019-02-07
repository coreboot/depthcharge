/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 *
 * Firmware image layout for Wilco Embedded Controller.
 */

#ifndef __DRIVERS_EC_WILCO_IMAGE_H__
#define __DRIVERS_EC_WILCO_IMAGE_H__

#include <stdint.h>

/**
 * enum wilco_ec_image - Details about image layout
 * @EC_IMAGE_HEADER_TAG: Header tag that is always present.
 * @EC_IMAGE_HEADER_VER: Header version 0.
 * @EC_IMAGE_BLOCK_SIZE: Number of bytes in each firmware block.
 * @EC_IMAGE_TRAILER_SIZE: Length of trailer after image.
 * @EC_IMAGE_ENC_FLAG: Indicates if EC image uses encryption.
 * @EC_IMAGE_ENC_TRAILER_SIZE: Length of extra trailer if %EC_IMAGE_ENC_FLAG.
 */
enum wilco_ec_image {
	EC_IMAGE_HEADER_TAG = 0x4d434850,
	EC_IMAGE_HEADER_VER = 0,
	EC_IMAGE_BLOCK_SIZE = 64,
	EC_IMAGE_TRAILER_SIZE = 64,
	EC_IMAGE_ENC_FLAG = (1 << 15),
	EC_IMAGE_ENC_TRAILER_SIZE = 64,
};

/**
 * struct WilcoEcImageHeader - Header before start of firmware image.
 * @tag: Expected to be %EC_IMAGE_HEADER_TAG.
 * @version: Expected to be %EC_IMAGE_HEADER_VER.
 * @flags: Flags for firmware image.
 * @reserved1: Unused.
 * @firmware_size: Length of firmware in %EC_IMAGE_BLOCK_SIZE units.
 * @reserved2: Unused.
 */
typedef struct __attribute__((packed)) WilcoEcImageHeader {
	uint32_t tag;
	uint8_t version;
	uint16_t flags;
	uint8_t reserved1[9];
	uint16_t firmware_size;
	uint8_t reserved3[110];
} WilcoEcImageHeader;

#endif
