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

#include "drivers/ec/chromeos/commands.h"
#include "drivers/ec/chromeos/message.h"

struct mkbp_dev;
extern struct mkbp_dev *mkbp_ptr;

/* Our configuration information */
struct mkbp_dev {
	int cmd_version_is_supported;   /* Device supports command versions */
	int optimise_flash_write;	/* Don't write erased flash blocks */

	uint8_t din[MSG_BYTES] __attribute__((aligned(sizeof(int64_t))));
	uint8_t dout[MSG_BYTES]	__attribute__((aligned(sizeof(int64_t))));
};

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
 * @param dev		MKBP device
 * @param id		Place to put the ID
 * @param maxlen	Maximum length of the ID field
 * @return 0 if ok, -1 on error
 */
int mkbp_read_id(struct mkbp_dev *dev, char *id, int maxlen);

/**
 * Read a keyboard scan from the MKBP device
 *
 * Send a message requesting a keyboard scan and return the result
 *
 * @param dev		MKBP device
 * @param scan		Place to put the scan results
 * @return 0 if ok, -1 on error
 */
int mkbp_scan_keyboard(struct mkbp_dev *dev, struct mkbp_keyscan *scan);

/**
 * Read which image is currently running on the MKBP device.
 *
 * @param dev		MKBP device
 * @param image		Destination for image identifier
 * @return 0 if ok, <0 on error
 */
int mkbp_read_current_image(struct mkbp_dev *dev, enum ec_current_image *image);

/**
 * Read the hash of the MKBP device firmware.
 *
 * @param dev		MKBP device
 * @param hash		Destination for hash information
 * @return 0 if ok, <0 on error
 */
int mkbp_read_hash(struct mkbp_dev *dev, struct ec_response_vboot_hash *hash);

/**
 * Send a reboot command to the MKBP device.
 *
 * Note that some reboot commands (such as EC_REBOOT_COLD) also reboot the AP.
 *
 * @param dev		MKBP device
 * @param cmd		Reboot command
 * @param flags         Flags for reboot command (EC_REBOOT_FLAG_*)
 * @return 0 if ok, <0 on error
 */
int mkbp_reboot(struct mkbp_dev *dev, enum ec_reboot_cmd cmd, uint8_t flags);

/**
 * Check if the MKBP device has an interrupt pending.
 *
 * Read the status of the external interrupt connected to the MKBP device.
 * If no external interrupt is configured, this always returns 1.
 *
 * @param dev		MKBP device
 * @return 0 if no interrupt is pending
 */
int mkbp_interrupt_pending(struct mkbp_dev *dev);

/**
 * Set up the Chromium OS matrix keyboard protocol
 *
 * @param blob		Device tree blob containing setup information
 * @return pointer to the mkbp device
 */
struct mkbp_dev *mkbp_init(void);

/**
 * Read information about the keyboard matrix
 *
 * @param dev		MKBP device
 * @param info		Place to put the info structure
 */
int mkbp_info(struct mkbp_dev *dev, struct ec_response_mkbp_info *info);

/**
 * Read the host event flags
 *
 * @param dev		MKBP device
 * @param events_ptr	Destination for event flags.  Not changed on error.
 * @return 0 if ok, <0 on error
 */
int mkbp_get_host_events(struct mkbp_dev *dev, uint32_t *events_ptr);

/**
 * Clear the specified host event flags
 *
 * @param dev		MKBP device
 * @param events	Event flags to clear
 * @return 0 if ok, <0 on error
 */
int mkbp_clear_host_events(struct mkbp_dev *dev, uint32_t events);

/**
 * Get/set flash protection
 *
 * @param dev		MKBP device
 * @param set_mask	Mask of flags to set; if 0, just retrieves existing
 *                      protection state without changing it.
 * @param set_flags	New flag values; only bits in set_mask are applied;
 *                      ignored if set_mask=0.
 * @param prot          Destination for updated protection state from EC.
 * @return 0 if ok, <0 on error
 */
int mkbp_flash_protect(struct mkbp_dev *dev,
		       uint32_t set_mask, uint32_t set_flags,
		       struct ec_response_flash_protect *resp);


/**
 * Run internal tests on the mkbp interface.
 *
 * @param dev		MKBP device
 * @return 0 if ok, <0 if the test failed
 */
int mkbp_test(struct mkbp_dev *dev);

/**
 * Update the EC RW copy.
 *
 * @param dev		MKBP device
 * @param image		the content to write
 * @param imafge_size	content length
 * @return 0 if ok, <0 if the test failed
 */
int mkbp_flash_update_rw(struct mkbp_dev *dev,
			 const uint8_t  *image, int image_size);

/**
 * Return a pointer to the board's MKBP device
 *
 * This should be implemented by board files.
 *
 * @return pointer to MKBP device, or NULL if none is available
 */
struct mkbp_dev *board_get_mkbp_dev(void);


/* Internal interfaces */
int mkbp_bus_init(struct mkbp_dev *dev);

/**
 * Check whether the bus interface supports new-style commands.
 *
 * LPC has its own way of doing this, which involves checking LPC values
 * visible to the host. Do this, and update dev->cmd_version_is_supported
 * accordingly.
 *
 * @param dev		MKBP device to check
 */
int mkbp_bus_check_version(struct mkbp_dev *dev);

/**
 * Send a command to a MKBP device and return the reply.
 *
 * The device's internal input/output buffers are used.
 *
 * @param dev		MKBP device
 * @param cmd		Command to send (EC_CMD_...)
 * @param cmd_version	Version of command to send (EC_VER_...)
 * @param dout          Output data (may be NULL If dout_len=0)
 * @param dout_len      Size of output data in bytes
 * @param dinp          Returns pointer to response data
 * @param din_len       Maximum size of response in bytes
 * @return number of bytes in response, or -1 on error
 */
int mkbp_bus_command(struct mkbp_dev *dev, uint8_t cmd, int cmd_version,
		     const uint8_t *dout, int dout_len,
		     uint8_t **dinp, int din_len);

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

int mkbp_flash_erase(struct mkbp_dev *dev, uint32_t offset, uint32_t size);

/**
 * Read data from the flash
 *
 * Read an arbitrary amount of data from the EC flash, by repeatedly reading
 * small blocks.
 *
 * The offset starts at 0. You can obtain the region information from
 * mkbp_flash_offset() to find out where to read for a particular region.
 *
 * @param dev		MKBP device
 * @param data		Pointer to data buffer to read into
 * @param offset	Offset within flash to read from
 * @param size		Number of bytes to read
 * @return 0 if ok, -1 on error
 */
int mkbp_flash_read(struct mkbp_dev *dev, uint8_t *data, uint32_t offset,
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
 * @param dev		MKBP device
 * @param data		Pointer to data buffer to write
 * @param offset	Offset within flash to write to.
 * @param size		Number of bytes to write
 * @return 0 if ok, -1 on error
 */
int mkbp_flash_write(struct mkbp_dev *dev, const uint8_t *data,
		     uint32_t offset, uint32_t size);

/**
 * Obtain position and size of a flash region
 *
 * @param dev		MKBP device
 * @param region	Flash region to query
 * @param offset	Returns offset of flash region in EC flash
 * @param size		Returns size of flash region
 * @return 0 if ok, -1 on error
 */
int mkbp_flash_offset(struct mkbp_dev *dev, enum ec_flash_region region,
		      uint32_t *offset, uint32_t *size);

/**
 * Read/write VbNvContext from/to a MKBP device.
 *
 * @param dev		MKBP device
 * @param block		Buffer of VbNvContext to be read/write
 * @return 0 if ok, -1 on error
 */
int mkbp_read_vbnvcontext(struct mkbp_dev *dev, uint8_t *block);
int mkbp_write_vbnvcontext(struct mkbp_dev *dev, const uint8_t *block);

/**
 * Read the version information for the EC images
 *
 * @param dev		MKBP device
 * @param versionp	This is set to point to the version information
 * @return 0 if ok, -1 on error
 */
int mkbp_read_version(struct mkbp_dev *dev,
		       struct ec_response_get_version **versionp);

/**
 * Read the build information for the EC
 *
 * @param dev		MKBP device
 * @param versionp	This is set to point to the build string
 * @return 0 if ok, -1 on error
 */
int mkbp_read_build_info(struct mkbp_dev *dev, char **strp);

#endif
