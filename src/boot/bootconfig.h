/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __BOOT_BOOTCONFIG_H__
#define __BOOT_BOOTCONFIG_H__

#define BOOTCONFIG_MAGIC "#BOOTCONFIG\n"
#define BOOTCONFIG_MAGIC_BYTES 12
#define BOOTCONFIG_SIZE_BYTES 4
#define BOOTCONFIG_CHECKSUM_BYTES 4
#define BOOTCONFIG_TRAILER_BYTES (BOOTCONFIG_MAGIC_BYTES + \
				  BOOTCONFIG_SIZE_BYTES + \
				  BOOTCONFIG_CHECKSUM_BYTES)

struct bootconfig {
	void *bootc_start;
	size_t bootc_size;
	size_t bootc_limit;
};

struct bootconfig_trailer {
	uint32_t params_size;
	uint32_t params_checksum;
	char magic[BOOTCONFIG_MAGIC_BYTES];
};

/**
 * Add new string to bootconfig parameters, update trailer structure.
 *
 * In case provided key already exists, update its value.
 *
 * @param bc - pointer to bootconfig structure
 * @param key - pointer to the string comprising key
 * @param value - pointer to the string comprising value for key
 *
 * @param return: New size of bootconfig params section after update,
 *                -1 on error
 */
int bootconfig_append_params(struct bootconfig *bc, char *key, char *value);

/**
 * Parse bootconfig section created at build time (usually on vendor_boot
 * partition) and based on this create eventual bootconfig structure at the
 * provided target address.
 *
 * @param bc - pointer to bootconfig structure
 * @param bootsection_params - pointer to the bootsection part of vendor image
 * @param bootsection_size - size in bytes of the bootsection
 *
 * @return Size of the whole bootconfig structure,
 *         -1 on error
 */
int bootconfig_init(struct bootconfig *bc, void *bootsection_params, size_t bootsection_size);

/*
 * Replace spaces in original string with new line characters. Update bootconfig
 * structure with the newly created string.
 *
 * @param bc - pointer to bootconfig structure
 * @param cmdline_string - pointer to the cmdline string which should be added
 *                         to bootconfig
 *
 * Return: New size of bootconfig params section after update
 *         -1 in case of errors
 */
int bootconfig_append_cmdline(struct bootconfig *bc, char *cmdline_string);

#endif /* __BOOT_BOOTCONFIG_H__ */
