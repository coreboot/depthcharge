/*
 * Chromium OS mkbp driver
 *
 * Copyright 2012 Google Inc.
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_EC_CHROMEOS_MKBP_H__
#define __DRIVERS_EC_CHROMEOS_MKBP_H__

#include <stdint.h>

#include "drivers/ec/chromeos/commands.h"

typedef struct MkbpBusOps
{
	int (*init)(struct MkbpBusOps *me);

	/**
	 * Send a command to a MKBP device and return the reply.
	 *
	 * The device's internal input/output buffers are used.
	 *
	 * @param bus		MKBP bus ops
	 * @param cmd		Command to send (EC_CMD_...)
	 * @param cmd_version	Version of command to send (EC_VER_...)
	 * @param dout          Output data (may be NULL If dout_len=0)
	 * @param dout_len      Size of output data in bytes
	 * @param dinp          Returns pointer to response data
	 * @param din_len       Maximum size of response in bytes
	 * @return number of bytes in response, or -1 on error
	 */
	int (*send_command)(struct MkbpBusOps *me, uint8_t cmd,
			    int cmd_version,
			    const uint8_t *dout, int dout_len,
			    uint8_t **dinp, int din_len);
} MkbpBusOps;

int mkbp_set_bus(MkbpBusOps *bus);

extern MkbpBusOps *mkbp_bus;

/*
 * Hard-code the number of columns we happen to know we have right now.  It
 * would be more correct to call mkbp_info() at startup and determine the
 * actual number of keyboard cols from there.
 */
#define MKBP_KEYSCAN_COLS 13

/* Information returned by a key scan */
struct mkbp_keyscan {
	uint8_t data[MKBP_KEYSCAN_COLS];
};

/**
 * Read the ID of the MKBP device
 *
 * The ID is a string identifying the MKBP device.
 *
 * @param bus		MKBP bus ops
 * @param id		Place to put the ID
 * @param maxlen	Maximum length of the ID field
 * @return 0 if ok, -1 on error
 */
int mkbp_read_id(MkbpBusOps *bus, char *id, int maxlen);

/**
 * Read a keyboard scan from the MKBP device
 *
 * Send a message requesting a keyboard scan and return the result
 *
 * @param bus		MKBP bus ops
 * @param scan		Place to put the scan results
 * @return 0 if ok, -1 on error
 */
int mkbp_scan_keyboard(MkbpBusOps *bus, struct mkbp_keyscan *scan);

/**
 * Read which image is currently running on the MKBP device.
 *
 * @param bus		MKBP bus ops
 * @param image		Destination for image identifier
 * @return 0 if ok, <0 on error
 */
int mkbp_read_current_image(MkbpBusOps *bus, enum ec_current_image *image);

/**
 * Read the hash of the MKBP device firmware.
 *
 * @param bus		MKBP bus ops
 * @param hash		Destination for hash information
 * @return 0 if ok, <0 on error
 */
int mkbp_read_hash(MkbpBusOps *bus, struct ec_response_vboot_hash *hash);

/**
 * Send a reboot command to the MKBP device.
 *
 * Note that some reboot commands (such as EC_REBOOT_COLD) also reboot the AP.
 *
 * @param bus		MKBP bus ops
 * @param cmd		Reboot command
 * @param flags         Flags for reboot command (EC_REBOOT_FLAG_*)
 * @return 0 if ok, <0 on error
 */
int mkbp_reboot(MkbpBusOps *bus, enum ec_reboot_cmd cmd, uint8_t flags);

/**
 * Check if the MKBP device has an interrupt pending.
 *
 * Read the status of the external interrupt connected to the MKBP device.
 * If no external interrupt is configured, this always returns 1.
 *
 * @param bus		MKBP bus ops
 * @return 0 if no interrupt is pending
 */
int mkbp_interrupt_pending(MkbpBusOps *bus);

/**
 * Set up the Chromium OS matrix keyboard protocol
 *
 * @param blob		Device tree blob containing setup information
 * @return 0 if ok, <0 on error
 */
int mkbp_init(MkbpBusOps *bus);

/**
 * Read information about the keyboard matrix
 *
 * @param bus		MKBP bus ops
 * @param info		Place to put the info structure
 */
int mkbp_info(MkbpBusOps *bus, struct ec_response_mkbp_info *info);

/**
 * Read the host event flags
 *
 * @param bus		MKBP bus ops
 * @param events_ptr	Destination for event flags.  Not changed on error.
 * @return 0 if ok, <0 on error
 */
int mkbp_get_host_events(MkbpBusOps *bus, uint32_t *events_ptr);

/**
 * Clear the specified host event flags
 *
 * @param bus		MKBP bus ops
 * @param events	Event flags to clear
 * @return 0 if ok, <0 on error
 */
int mkbp_clear_host_events(MkbpBusOps *bus, uint32_t events);

/**
 * Get/set flash protection
 *
 * @param bus		MKBP bus ops
 * @param set_mask	Mask of flags to set; if 0, just retrieves existing
 *                      protection state without changing it.
 * @param set_flags	New flag values; only bits in set_mask are applied;
 *                      ignored if set_mask=0.
 * @param prot          Destination for updated protection state from EC.
 * @return 0 if ok, <0 on error
 */
int mkbp_flash_protect(MkbpBusOps *bus,
		       uint32_t set_mask, uint32_t set_flags,
		       struct ec_response_flash_protect *resp);


/**
 * Run internal tests on the mkbp interface.
 *
 * @param bus		MKBP bus ops
 * @return 0 if ok, <0 if the test failed
 */
int mkbp_test(MkbpBusOps *bus);

/**
 * Update the EC RW copy.
 *
 * @param bus		MKBP bus ops
 * @param image		the content to write
 * @param imafge_size	content length
 * @return 0 if ok, <0 if the test failed
 */
int mkbp_flash_update_rw(MkbpBusOps *bus,
			 const uint8_t  *image, int image_size);

/* Internal interfaces */

/**
 * Dump a block of data for a command.
 *
 * @param name	Name for data (e.g. 'in', 'out')
 * @param cmd	Command number associated with data, or -1 for none
 * @param data	Data block to dump
 * @param len	Length of data block to dump
 */
void mkbp_dump_data(const char *name, int cmd, const uint8_t *data, int len);

/**
 * Calculate a simple 8-bit checksum of a data block
 *
 * @param data	Data block to checksum
 * @param size	Size of data block in bytes
 * @return checksum value (0 to 255)
 */
uint8_t mkbp_calc_checksum(const uint8_t *data, int size);

/**
 * Decode a flash region parameter
 *
 * @param argc	Number of params remaining
 * @param argv	List of remaining parameters
 * @return flash region (EC_FLASH_REGION_...) or -1 on error
 */
int mkbp_decode_region(int argc, char * const argv[]);

int mkbp_flash_erase(MkbpBusOps *bus, uint32_t offset, uint32_t size);

/**
 * Read data from the flash
 *
 * Read an arbitrary amount of data from the EC flash, by repeatedly reading
 * small blocks.
 *
 * The offset starts at 0. You can obtain the region information from
 * mkbp_flash_offset() to find out where to read for a particular region.
 *
 * @param bus		MKBP bus ops
 * @param data		Pointer to data buffer to read into
 * @param offset	Offset within flash to read from
 * @param size		Number of bytes to read
 * @return 0 if ok, -1 on error
 */
int mkbp_flash_read(MkbpBusOps *bus, uint8_t *data, uint32_t offset,
		    uint32_t size);

/**
 * Write data to the flash
 *
 * Write an arbitrary amount of data to the EC flash, by repeatedly writing
 * small blocks.
 *
 * The offset starts at 0. You can obtain the region information from
 * mkbp_flash_offset() to find out where to write for a particular region.
 *
 * Attempting to write to the region where the EC is currently running from
 * will result in an error.
 *
 * @param bus		MKBP bus ops
 * @param data		Pointer to data buffer to write
 * @param offset	Offset within flash to write to.
 * @param size		Number of bytes to write
 * @return 0 if ok, -1 on error
 */
int mkbp_flash_write(MkbpBusOps *bus, const uint8_t *data,
		     uint32_t offset, uint32_t size);

/**
 * Obtain position and size of a flash region
 *
 * @param bus		MKBP bus ops
 * @param region	Flash region to query
 * @param offset	Returns offset of flash region in EC flash
 * @param size		Returns size of flash region
 * @return 0 if ok, -1 on error
 */
int mkbp_flash_offset(MkbpBusOps *bus, enum ec_flash_region region,
		      uint32_t *offset, uint32_t *size);

/**
 * Read/write VbNvContext from/to a MKBP device.
 *
 * @param bus		MKBP bus ops
 * @param block		Buffer of VbNvContext to be read/write
 * @return 0 if ok, -1 on error
 */
int mkbp_read_vbnvcontext(MkbpBusOps *bus, uint8_t *block);
int mkbp_write_vbnvcontext(MkbpBusOps *bus, const uint8_t *block);

/**
 * Read the version information for the EC images
 *
 * @param bus		MKBP bus ops
 * @param versionp	This is set to point to the version information
 * @return 0 if ok, -1 on error
 */
int mkbp_read_version(MkbpBusOps *bus,
		       struct ec_response_get_version **versionp);

/**
 * Read the build information for the EC
 *
 * @param bus		MKBP bus ops
 * @param versionp	This is set to point to the build string
 * @return 0 if ok, -1 on error
 */
int mkbp_read_build_info(MkbpBusOps *bus, char **strp);

#endif
