/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 *
 * Flash update interface for the Wilco Embedded Controller.
 */

#ifndef __DRIVERS_EC_WILCO_FLASH_H__
#define __DRIVERS_EC_WILCO_FLASH_H__

#include <stdint.h>

/**
 * enum EcFlashLocation - Location of flash device.
 * @EC_FLASH_LOCATION_MAIN: Located on mainboard.
 * @EC_FLASH_LOCATION_BASE: Located in the base, for a detachable device.
 * @EC_FLASH_LOCATION_DOCK: Located in the first attached dock.
 */
typedef enum EcFlashLocation {
	EC_FLASH_LOCATION_MAIN,
	EC_FLASH_LOCATION_BASE,
	EC_FLASH_LOCATION_DOCK
} EcFlashLocation;

/**
 * enum EcFlashType - Type of flash device.
 * @EC_FLASH_TYPE_EC: Embedded Controller device.
 * @EC_FLASH_TYPE_TCPC: Type-C Port Controller device.
 */
typedef enum EcFlashType {
	EC_FLASH_TYPE_EC,
	EC_FLASH_TYPE_TCPC
} EcFlashType;

/**
 * enum EcFlashSubType - Sub-types of flash devices.
 * @EC_FLASH_TYPE_EC_KB: EC uses the KB flash protocol.
 */
typedef enum EcFlashSubType {
	EC_FLASH_TYPE_EC_KB = 0x04
} EcFlashSubType;

/**
 * enum EcFlashInterface - Select between primary and failsafe image.
 * @EC_FLASH_INSTANCE_PRIMARY: Primary flash copy.
 * @EC_FLASH_INSTANCE_FAILSAFE: Failsafe/recovery flash copy.
 */
typedef enum EcFlashInstance {
	EC_FLASH_INSTANCE_PRIMARY,
	EC_FLASH_INSTANCE_FAILSAFE
} EcFlashInstance;

/**
 * enum EcFlashArgEcKbFlash - Image signature type for EC_FLASH_TYPE_EC_KB.
 * @EC_FLASH_ARG_UNSIGNED: Image is unsigned.
 * @EC_FLASH_ARG_SIGNED: Image is signed.
 */
typedef enum EcFlashArgEcKbFlash {
	EC_FLASH_ARG_UNSIGNED,
	EC_FLASH_ARG_SIGNED
} EcFlashArgEcKbFlash;

/**
 * struct WilcoEcFlashDevice - Information about device to be flashed.
 * @location: %EcFlashLocation for location of the flash device.
 * @type: %EcFlashType for the type of flash device.
 * @sub_type: %EcFlashSubType for the sub-type of flash device.
 * @sub_type_arg: Argument for type + sub-type, ex %EcFlashArgEcKbFlash.
 * @instance: %EcFlashInstance for selecting primary or failsafe copy.
 * @version: Version of the firmware to be flashed.
 */
typedef struct __attribute__((packed)) WilcoEcFlashDevice {
	uint8_t location;
	uint8_t type;
	uint8_t sub_type;
	uint8_t sub_type_arg;
	uint8_t instance;
	uint32_t version;
} WilcoEcFlashDevice;

/**
 * wilco_ec_flash_find() - Search EC flash inventory for specified device.
 * @ec: Wilco EC device.
 * @device: Flash device information returned from the EC. Allocated by caller.
 * @location: %EcFlashLocation to look for device in.
 * @type: %EcFlashType to find a matching device.
 * @instance: %EcFlashInstance to return.
 *
 * Return: 0 to indicate device was found and returned in WilcoEcFlashDevice,
 *         or negative error code on failure.
 */
int wilco_ec_flash_find(WilcoEc *ec, WilcoEcFlashDevice *device,
			EcFlashLocation location, EcFlashType type,
			EcFlashInstance instance);

/**
 * wilco_ec_flash_image() - Flash EC image.
 * @ec: Wilco EC device.
 * @device: Information about EC flash device.
 * @image: Image data.
 * @image_size: Number of bytes of image data.
 * @version: Version of new firmware image.
 *
 * Return: 0 to indicate success or negative error code on failure.
 */
int wilco_ec_flash_image(WilcoEc *ec, WilcoEcFlashDevice *device,
			 const uint8_t *image, int image_size,
			 uint32_t version);

#endif
