/*
 * Copyright 2017 Google Inc.
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

#ifndef __DRIVERS_EC_PS8751_H__
#define __DRIVERS_EC_PS8751_H__

#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/vboot_auxfw.h"

typedef enum ParadeChipType {
	CHIP_PS8751,
	CHIP_PS8755,
	CHIP_PS8805,
	CHIP_PS8815,
	CHIP_PS8705,
} ParadeChipType;

/*
 * The base firmware consists of a bootloader+application and is
 * equivalent to the factory firmware image. The base firmware checks
 * for a valid alternate application image in a dedicated location and
 * boots into it. Only the ps8815 firmware supports this feature.
 */

typedef enum ParadeFwType {
	PARADE_FW_UNKNOWN,
	PARADE_FW_BASE,
	PARADE_FW_APP,
} ParadeFwType;

typedef struct Ps8751 {
	VbootAuxfwOps fw_ops;
	CrosECTunnelI2c *bus;
	int ec_pd_id;

	/* these are cached from chip regs */
	struct {
		uint16_t vendor;
		uint16_t product;
		uint16_t device;
		uint8_t fw_rev;
	} chip;
	uint8_t blob_hw_version;
	ParadeChipType chip_type;
	char chip_name[16];

	uint8_t last_a16_a23;	/* cached flash window addr bits */
	uint8_t last_a8_a15;	/* cached flash window addr bits */

	ParadeFwType fw_type;
	size_t fw_start;	/* selected FW start addr in flash */
	size_t fw_end;		/* selected FW end addr in flash */

	/**
	 * Initialize device context state for subsequent flash
	 * operations.
	 *
	 * @param me		device context
	 */
	void (*flash_start)(struct Ps8751 *me);

	/**
	 * Read data from flash. The actual number of bytes read may be
	 * less than requested due to device or implementation
	 * limitations.
	 *
	 * @param me		device context
	 * @param data		destination address of data to read
	 * @param count		number of bytes to read
	 * @param a24		flash address to read
	 * @return number of bytes read, -1 on error
	 */
	ssize_t (*flash_read)(struct Ps8751 *me,
			      uint8_t * const data,
			      const size_t count,
			      const uint32_t a24);

	/**
	 * Write data to flash. The actual number of bytes written may
	 * be less than requested due to device or implementation
	 * limitations.
	 *
	 * @param me		device context
	 * @param data		source address of data to write
	 * @param count		number of bytes to write
	 * @param a24		flash address to write
	 * @return number of bytes written, -1 on error
	 */
	ssize_t (*flash_write)(struct Ps8751 *me,
			       const uint8_t * const data,
			       const size_t count,
			       const uint32_t a24);

	/**
	 * Additional step to enable writes to flash.
	 *
	 * @param me		device context
	 * @return 0 if ok, -1 on error
	 */
	int (*flash_write_enable)(struct Ps8751 *me);

	/**
	 * Additional step to disable writes to flash.
	 *
	 * @param me		device context
	 * @return 0 if ok, -1 on error
	 */
	int (*flash_write_disable)(struct Ps8751 *me);
} Ps8751;

Ps8751 *new_ps8751(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8751_canary(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8755(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8805_a2(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8805_a3(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8815_a0(CrosECTunnelI2c *bus, int ec_pd_id);
Ps8751 *new_ps8815_a1(CrosECTunnelI2c *bus, int ec_pd_id);

#endif /* __DRIVERS_EC_PS8751_H__ */
