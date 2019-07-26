/*
 * Chromium OS EC driver
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

#ifndef __DRIVERS_EC_CROS_EC_H__
#define __DRIVERS_EC_CROS_EC_H__

#include <stdint.h>

#include "drivers/ec/cros/commands.h"
#include "drivers/gpio/gpio.h"

typedef struct CrosEcBusOps
{
	int (*init)(struct CrosEcBusOps *me);

	/**
	 * Send a command to a ChromeOS EC device and return the reply.
	 *
	 * The device's internal input/output buffers are used.
	 *
	 * @param bus		ChromeOS EC bus ops
	 * @param cmd		Command to send (EC_CMD_...)
	 * @param cmd_version	Version of command to send (EC_VER_...)
	 * @param dout          Output data (may be NULL If dout_len=0)
	 * @param dout_len      Size of output data in bytes
	 * @param dinp          Returns pointer to response data
	 * @param din_len       Maximum size of response in bytes
	 * @return number of bytes in response, or -1 on error
	 */
	int (*send_command)(struct CrosEcBusOps *me, uint8_t cmd,
			    int cmd_version,
			    const void *dout, uint32_t dout_len,
			    void *din, uint32_t din_len);

	int (*send_packet)(struct CrosEcBusOps *me,
			   const void *dout, uint32_t dout_len,
			   void *din, uint32_t din_len);

	/**
	 * Byte I/O functions.
	 *
	 * Read or write a sequence a bytes over the desired protocol.
	 * Used to support protocol variants - currently only implemented
	 * for LPC.
	 *
	 * @param data		Read / write data buffer
	 * @param port		I/O port
	 * @size		Number of bytes to read / write
	 */
	void (*read)(uint8_t *data, uint16_t port, int size);
	void (*write)(const uint8_t *data, uint16_t port, int size);
} CrosEcBusOps;

int cros_ec_set_bus(CrosEcBusOps *bus);
void cros_ec_set_interrupt_gpio(GpioOps *gpio);

/*
 * Hard-code the number of columns we happen to know we have right now.  It
 * would be more correct to call cros_ec_mkbp_info() at startup and determine
 * the actual number of keyboard cols from there.
 */
#define CROS_EC_KEYSCAN_COLS 13

/* Information returned by a key scan */
struct cros_ec_keyscan {
	uint8_t data[CROS_EC_KEYSCAN_COLS];
};

/**
 * Read the ID of the ChromeOS EC device
 *
 * The ID is a string identifying the ChromeOS EC device.
 *
 * @param id		Place to put the ID
 * @param maxlen	Maximum length of the ID field
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_id(char *id, int maxlen);

/**
 * Read the protocol info structure from the ChromeOS EC device
 *
 * @param devidx	Index of target device
 * @param info		Pointer to the ec_response_get_protocol_info structure
 *			to fill
 * @return 0 if ok, -1 on error
 */
int cros_ec_get_protocol_info(int devidx,
			      struct ec_response_get_protocol_info *info);

/**
 * Read a keyboard scan from the ChromeOS EC device
 *
 * Send a message requesting a keyboard scan and return the result
 *
 * @param scan		Place to put the scan results
 * @return 0 if ok, -1 on error
 */
int cros_ec_scan_keyboard(struct cros_ec_keyscan *scan);

/**
 * Read which image is currently running on the ChromeOS EC device.
 *
 * @param devidx	Index of target device
 * @param image		Destination for image identifier
 * @return 0 if ok, <0 on error
 */
int cros_ec_read_current_image(int devidx, enum ec_current_image *image);

/**
 * Read the hash of the ChromeOS EC device firmware.
 *
 * @param devidx	Index of target device
 * @param hash		Destination for hash information
 * @return 0 if ok, <0 on error
 */
int cros_ec_read_hash(int devidx, struct ec_response_vboot_hash *hash);

/**
 * Send a reboot command to the ChromeOS EC device.
 *
 * Note that some reboot commands (such as EC_REBOOT_COLD) also reboot the AP.
 *
 * @param devidx	Index of target device
 * @param cmd		Reboot command
 * @param flags         Flags for reboot command (EC_REBOOT_FLAG_*)
 * @return 0 if ok, <0 on error
 */
int cros_ec_reboot(int devidx, enum ec_reboot_cmd cmd, uint8_t flags);

/**
 * Check if the ChromeOS EC device has an interrupt pending.
 *
 * Read the status of the external interrupt connected to the ChromeOS EC
 * device. If no external interrupt is configured, this always returns 1.
 *
 * @return 0 if no interrupt is pending
 */
int cros_ec_interrupt_pending(void);

/**
 * Set up the Chromium OS matrix keyboard protocol
 *
 * @return 0 if ok, <0 on error
 */
int cros_ec_init(void);

/**
 * Read information about the keyboard matrix
 *
 * @param info		Place to put the info structure
 */
int cros_ec_mkbp_info(struct ec_response_mkbp_info *info);

/**
 * Read the host event flags
 *
 * @param events_ptr	Destination for event flags.  Not changed on error.
 * @return 0 if ok, <0 on error
 */
int cros_ec_get_host_events(uint32_t *events_ptr);

/**
 * Clear the specified host event flags
 *
 * @param events	Event flags to clear
 * @return 0 if ok, <0 on error
 */
int cros_ec_clear_host_events(uint32_t events);

/**
 * Get/set flash protection
 *
 * @param devidx	Index of target device
 * @param set_mask	Mask of flags to set; if 0, just retrieves existing
 *                      protection state without changing it.
 * @param set_flags	New flag values; only bits in set_mask are applied;
 *                      ignored if set_mask=0.
 * @param prot          Destination for updated protection state from EC.
 * @return 0 if ok, <0 on error
 */
int cros_ec_flash_protect(int devidx, uint32_t set_mask, uint32_t set_flags,
			  struct ec_response_flash_protect *resp);

/**
 * Vboot Tell EC to enter a mode (recovery, dev, or normal).
 *
 * @param devidx	Index of target device
 * @param mode          recovery, dev, or normal
 * @return 0 if ok, <0 on error
 */
int cros_ec_entering_mode(int devidx, int mode);

/**
 * Run internal tests on the ChromeOS EC interface.
 *
 * @return 0 if ok, <0 if the test failed
 */
int cros_ec_test(void);

/**
 * Update the EC RW copy.
 *
 * @param devidx	Index of target device
 * @param image		the content to write
 * @param imafge_size	content length
 * @return 0 if ok, <0 if the test failed
 */
int cros_ec_flash_update_rw(int devidx, const uint8_t *image, int image_size);

/* Internal interfaces */

/**
 * Dump a block of data for a command.
 *
 * @param name	Name for data (e.g. 'in', 'out')
 * @param cmd	Command number associated with data, or -1 for none
 * @param data	Data block to dump
 * @param len	Length of data block to dump
 */
void cros_ec_dump_data(const char *name, int cmd, const void *data, int len);

/**
 * Calculate a simple 8-bit checksum of a data block
 *
 * @param data	Data block to checksum
 * @param size	Size of data block in bytes
 * @return checksum value (0 to 255)
 */
uint8_t cros_ec_calc_checksum(const void *data, int size);

/**
 * Decode a flash region parameter
 *
 * @param argc	Number of params remaining
 * @param argv	List of remaining parameters
 * @return flash region (EC_FLASH_REGION_...) or -1 on error
 */
int cros_ec_decode_region(int argc, char * const argv[]);

int cros_ec_flash_erase(int devidx, uint32_t offset, uint32_t size);

/**
 * Read data from the flash
 *
 * Read an arbitrary amount of data from the EC flash, by repeatedly reading
 * small blocks.
 *
 * The offset starts at 0. You can obtain the region information from
 * cros_ec_flash_offset() to find out where to read for a particular region.
 *
 * @param devidx	Index of target device
 * @param data		Pointer to data buffer to read into
 * @param offset	Offset within flash to read from
 * @param size		Number of bytes to read
 * @return 0 if ok, -1 on error
 */
int cros_ec_flash_read(int devidx, uint8_t *data, uint32_t offset,
		       uint32_t size);

/**
 * Write data to the flash
 *
 * Write an arbitrary amount of data to the EC flash, by repeatedly writing
 * small blocks.
 *
 * The offset starts at 0. You can obtain the region information from
 * cros_ec_flash_offset() to find out where to write for a particular region.
 *
 * Attempting to write to the region where the EC is currently running from
 * will result in an error.
 *
 * @param devidx	Index of target device
 * @param data		Pointer to data buffer to write
 * @param offset	Offset within flash to write to.
 * @param size		Number of bytes to write
 * @return 0 if ok, -1 on error
 */
int cros_ec_flash_write(int devidx, const uint8_t *data,
			uint32_t offset, uint32_t size);

/**
 * Obtain position and size of a flash region
 *
 * @param devidx	Index of target device
 * @param region	Flash region to query
 * @param offset	Returns offset of flash region in EC flash
 * @param size		Returns size of flash region
 * @return 0 if ok, -1 on error
 */
int cros_ec_flash_offset(int devidx, enum ec_flash_region region,
			 uint32_t *offset, uint32_t *size);

/**
 * Read/write VbNvContext from/to a ChromeOS EC device.
 *
 * @param block		Buffer of VbNvContext to be read/write
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_vbnvcontext(uint8_t *block);
int cros_ec_write_vbnvcontext(const uint8_t *block);

/**
 * Read the version information for the EC images
 *
 * @param versionp	This is the version information
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_version(struct ec_response_get_version *versionp);

/**
 * Read the build information for the EC
 *
 * @param versionp	This is the build string
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_build_info(char *strp);

#endif
