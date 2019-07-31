/*
 * Copyright 2016 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <cbfs.h>
#include <endian.h>
#include <libpayload.h>
#include <vb2_api.h>

#include "base/container_of.h"
#include "drivers/ec/anx7688/anx7688.h"
#include "drivers/flash/cbfs.h"

/* ANX7688 FW address (8-bit: 0x50) */
#define CHIP_FW 0x28
/* ANX7688 TCPC address (8-bit: 0x58) */
#define CHIP_TCPC 0x2c

#define VENDOR 0x291f
#define PRODUCT 0x8876

#define RW_TIMEOUT_US 10000
/* Version can take up to 100ms to appear after reset */
#define VERSION_TIMEOUT_US 300000
/* Whole firmware can take up to 750ms to load */
#define WAIT_TIMEOUT_US 2000000

/* The hard-coded PD port id. This could be dynamic in the future. */
#define ANX7688_PD_ID 0

/* Wait for ANX7688 FW to fully boot (or stop loading due to FW corruption).
 * crc_ok returns information on whether ANX7688 FW loader CRC checks passed
 * (bits below all 1).
 *
 * This requires the bus to be unlocked!
 *
 * CHIP_FW 0xe7 provides the following information
 * Bit Name           Description
 * 6   BOOT_LOAD_DONE 1:boot data load completed and crc32 check ok
 * 5   CRC_OK         1:crc32 check ok when EEPROM data load completed
 * 4   LOAD_DONE      1:EEPROM data load completed
 */
static int anx7688_wait_fw(struct Anx7688 *me, int *crc_ok)
{
	uint8_t status;
	uint64_t start = timer_us(0);

	do {
		if (timer_us(start) > WAIT_TIMEOUT_US) {
			printf("%s: Timeout\n", __func__);
			return -1;
		}
		if (i2c_readb(&me->bus->ops, CHIP_FW, 0xE7, &status) < 0) {
			printf("%s: Read failure\n", __func__);
			return -1;
		}
	} while ((status & 0x10) != 0x10);

	/* Check if there was a CRC failure during load. */
	if (crc_ok)
		*crc_ok = ((status & 0x70) == 0x70);

	printf("%s: Waited for %lld us (%02x).\n", __func__,
		timer_us(start), status);

	return 0;
}

/* Detect if chip is indeed ANX7688 */
int anx7688_detect(struct Anx7688 *me)
{
	uint16_t vendor, product, device;

	if (i2c_readw(&me->bus->ops, CHIP_TCPC, 0x00, &vendor) < 0 ||
	    i2c_readw(&me->bus->ops, CHIP_TCPC, 0x02, &product) < 0 ||
	    i2c_readw(&me->bus->ops, CHIP_TCPC, 0x04, &device) < 0) {
		printf("ANX7688 vendor/product cannot be read\n");
		return 0;
	}

	printf("ANX7688 %04x/%04x/%04x detected\n", vendor, product, device);

	return (vendor == VENDOR && product == PRODUCT);
}

/* Returns -1 on error, 0 if the block does not match, 1 if it matches. */
static int verify_block(I2cOps *bus, int address, const uint8_t *block)
{
	uint8_t status;
	uint8_t buffer[16];
	uint64_t start;

	printf("V");

	/* Write address and read command */
	uint8_t cmdbuf[3] = {address >> 8, address & 0xFF, 0x06};

	if (i2c_writeblock(bus, CHIP_FW, 0xE0, cmdbuf, sizeof(cmdbuf)) < 0)
		return -1;

	start = timer_us(0);
	do {
		if (timer_us(start) > RW_TIMEOUT_US) {
			printf("%s: Timeout\n", __func__);
			return -1;
		}
		if (i2c_readb(bus, CHIP_FW, 0xE2, &status) < 0)
			return -1;
	} while ((status & 0x08) == 0);

	/* Read data */
	if (i2c_readblock(bus, CHIP_FW, 0xD0, buffer, 16) < 0)
		return -1;

	if (memcmp(buffer, block, 16) != 0)
		return 0;

	return 1;
}

static int write_block(I2cOps *bus, int address, const uint8_t *block)
{
	uint8_t status;
	uint64_t start;

	printf("W");

	/* Write data, address and write command */
	uint8_t cmdbuf[16+3];

	memcpy(cmdbuf, block, 16);
	cmdbuf[16] = address >> 8;
	cmdbuf[17] = address & 0xFF;
	cmdbuf[18] = 0x01;

	if (i2c_writeblock(bus, CHIP_FW, 0xD0, cmdbuf, sizeof(cmdbuf)) < 0)
		return -1;

	start = timer_us(0);
	do {
		if (timer_us(start) > RW_TIMEOUT_US) {
			printf("%s: Timeout\n", __func__);
			return -1;
		}
		if (i2c_readb(bus, CHIP_FW, 0xE2, &status) < 0)
			return -1;
	} while ((status & 0x08) == 0x00);

	/* Wait for internal programming cycle to complete */
	mdelay(5);

	return 0;
}

/* Verify, compare, then (possibly) write and verify again a 16-byte block. */
static int update_block(I2cOps *bus, int address, const uint8_t *block)
{
	int ret;

	ret = verify_block(bus, address, block);
	if (ret < 0)
		return ret;

	if (ret == 0) {
		ret = write_block(bus, address, block);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* Returns version (or 0x0000 on error) */
static uint16_t anx7688_verify(struct Anx7688 *me)
{
	uint16_t status;
	uint16_t version;
	uint64_t start = timer_us(0);

	/* Wait for version to appear. */
	do {
		if (timer_us(start) > VERSION_TIMEOUT_US) {
			printf("%s: Timeout waiting for version\n", __func__);
			return 0x0000;
		}

		i2c_readw(&me->bus->ops, CHIP_TCPC, 0x80, &version);
	} while (version == 0x0000);

	printf("ANX7688 version: %04x\n", version);

	/* Read load and CRC32 status */
	while(1) {
		if (timer_us(start) > WAIT_TIMEOUT_US) {
			printf("%s: Timeout waiting for CRC32\n", __func__);
			return 0x0000;
		}

		if (i2c_readw(&me->bus->ops, CHIP_TCPC, 0x90, &status) < 0)
			continue;

		uint8_t crc_status = (status >> 8) & 0x0f;
		uint8_t load_status = status & 0x0f;

		/* Check that load status == CRC status (CRC status is updated
		 * before load status, to avoid race condition). */
		if ((crc_status & load_status) != load_status) {
			printf("%s: Bad CRC (%04x)\n", __func__, status);
			/* FIXME: Current firmware 1.12 updates load status
			   before CRC status, so this might not be fatal. */
			//return 0x0000;
		}

		/* Fully loaded */
		if ((status & 0x0f0f) == 0x0f0f)
			break;
	}

	return version;
}

/* Update ANX7688 firmware.
 * Image file binary format (little-endian)
 * Header:
 *  - 2-byte version
 *  - 1-byte number of CRC addresses
 *  - 1-byte number of regions
 *  - List of CRC32 addresses (2-byte: address)
 *  - List of regions to flash:
 *    - 2-byte: start
 *    - 2-byte: end
 * Payload (flat binary)
 */
static vb2_error_t anx7688_update(struct Anx7688 *me,
				  const uint8_t *data, int len)
{
	int i, ret = 0;
	uint16_t address;

	/* Version takes bytes 0-1 */
	uint8_t num_crc32 = data[2];
	uint8_t num_regions = data[3];
	int offset = 4;
	const uint16_t *crc32data = (uint16_t *)&data[offset];
	offset += num_crc32*2;
	const uint16_t *regionsdata = (uint16_t *)&data[offset];
	offset += num_regions*4;
	const uint8_t *rawdata = &data[offset];
	int rawlen = len - offset;

	printf("Updating ANX7688 firmware...\n");
	uint64_t start = timer_us(0);

	/* Lock ANX7688. */
	switch (cros_ec_pd_control(ANX7688_PD_ID, PD_SUSPEND)) {
	case -EC_RES_BUSY:
		printf("ANX7688 PD_SUSPEND busy! Could be only power "
		       "source.\n");
		return VBERROR_PERIPHERAL_BUSY;
	case EC_RES_SUCCESS:
		/* Continue onward */
		break;
	default:
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	}

	/* Wait for ANX7688 FW load to complete. */
	ret = anx7688_wait_fw(me, NULL);
	if (ret < 0)
		goto out;

	/* For some reason, status register read above is unreliable, wait a
	 * bit more, to be safe. See crosbug.com/p/53754 */
	mdelay(1000);

	/* Reset OCM */
	ret = i2c_set_bits(&me->bus->ops, CHIP_FW, 0x05, 0x10);
	if (ret < 0)
		goto out;

	/* Write password */
	ret = i2c_set_bits(&me->bus->ops, CHIP_FW, 0x3F, 0x20);
	if (ret < 0)
		goto out;
	ret = i2c_set_bits(&me->bus->ops, CHIP_FW, 0x44, 0x81);
	if (ret < 0)
		goto out;
	ret = i2c_set_bits(&me->bus->ops, CHIP_FW, 0x66, 0x08);
	if (ret < 0)
		goto out;

	/* First update all the CRCs, to make corruption more likely in case
	 * of failed update. */
	for (i = 0; i < num_crc32; i++) {
		/* Align to 16 byte boundary */
		address = ALIGN_DOWN(le16toh(crc32data[i]), 16);
		assert(address+16 <= rawlen);
		ret = update_block(&me->bus->ops, address, &rawdata[address]);

		if (ret < 0)
			goto out;
	}

	for (i = num_regions-1; i >= 0; i--) {
		uint16_t start = ALIGN_DOWN(le16toh(regionsdata[2*i]), 16);
		uint16_t end = ALIGN_UP(le16toh(regionsdata[2*i+1]), 16);
		/* Update from high to low addresses, to make sure the boot
		 * block (0x0010-0x080F) stays corrupted in case of a partial
		 * update. */
		for (address = end-16; address >= start; address -= 16) {
			assert(address+16 <= rawlen);
			ret = update_block(&me->bus->ops,
					   address, &rawdata[address]);

			if (ret < 0)
				goto out;
		}
	}

out:
	cros_ec_pd_control(ANX7688_PD_ID, PD_RESET);
	cros_ec_pd_control(ANX7688_PD_ID, PD_RESUME);
	mdelay(100);

	if (ret < 0) {
		printf("ANX7688 FW update error!\n");
		return VBERROR_UNKNOWN;
	}

	printf("ANX7688 FW updated successfully (%lld ms)!\n",
	       timer_us(start)/1000);
	return VBERROR_SUCCESS;
}

vb2_error_t vboot_hash_image(VbootEcOps *vbec, enum VbSelectFirmware_t select,
			     const uint8_t **hash, int *hash_size)
{
	Anx7688 *me = container_of(vbec, Anx7688, vboot);
	/* Actually, hash is just the version */
	static uint16_t hash_version;

	/* Return version as hash */
	hash_version = anx7688_verify(me);

	*hash_size = sizeof(hash_version);
	*hash = (uint8_t *)&hash_version;

	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_running_rw(VbootEcOps *vbec, int *in_rw)
{
	Anx7688 *me = container_of(vbec, Anx7688, vboot);

	if (cros_ec_tunnel_i2c_protect_status(me->bus, in_rw)) {
		/* We probably need a new EC */
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_reboot_to_ro(VbootEcOps *vbec)
{
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_jump_to_rw(VbootEcOps *vbec)
{
	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_disable_jump(VbootEcOps *vbec)
{
	Anx7688 *me = container_of(vbec, Anx7688, vboot);

	if (cros_ec_tunnel_i2c_protect(me->bus)) {
		/* We probably need a new EC */
		return VBERROR_UNKNOWN;
	}

	if (cros_ec_pd_control(ANX7688_PD_ID, PD_CONTROL_DISABLE) < 0) {
		/* We probably need a new EC */
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_entering_mode(VbootEcOps *vbec,
				       enum VbEcBootMode_t vbm)
{
	/* Noop success (should not be called) */

	return VBERROR_SUCCESS;
}

static vb2_error_t vboot_update_image(VbootEcOps *vbec,
				      enum VbSelectFirmware_t select,
				      const uint8_t *image, int image_size)
{
	Anx7688 *me = container_of(vbec, Anx7688, vboot);
	int protected;

	if (cros_ec_tunnel_i2c_protect_status(me->bus, &protected)) {
		/* We probably need a new EC */
		return VBERROR_UNKNOWN;
	}

	if (protected)
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;

	/* Double-check we are really trying to update the right chip. */
	if (!anx7688_detect(me))
		return VBERROR_UNKNOWN;

	/* Force update */
	return anx7688_update(me, image, image_size);
}

static vb2_error_t vboot_protect(VbootEcOps *vbec,
				 enum VbSelectFirmware_t select)
{
	/* Noop success */

	return VBERROR_SUCCESS;
}

Anx7688 *new_anx7688(CrosECTunnelI2c *bus)
{
	Anx7688 *bridge = xzalloc(sizeof(*bridge));

	bridge->bus = bus;

	bridge->vboot.running_rw = vboot_running_rw;
	bridge->vboot.jump_to_rw = vboot_jump_to_rw;
	bridge->vboot.disable_jump = vboot_disable_jump;
	bridge->vboot.hash_image = vboot_hash_image;
	bridge->vboot.update_image = vboot_update_image;
	bridge->vboot.protect = vboot_protect;
	bridge->vboot.entering_mode = vboot_entering_mode;
	bridge->vboot.reboot_to_ro = vboot_reboot_to_ro;

	return bridge;
}
