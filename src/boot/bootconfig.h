// SPDX-License-Identifier: GPL-2.0

#ifndef __BOOT_BOOTCONFIG_H__
#define __BOOT_BOOTCONFIG_H__

#define BOOTCONFIG_MAGIC "#BOOTCONFIG\n"
#define BOOTCONFIG_MAGIC_BYTES 12
#define BOOTCONFIG_SIZE_BYTES 4
#define BOOTCONFIG_CHECKSUM_BYTES 4
#define BOOTCONFIG_TRAILER_BYTES (BOOTCONFIG_MAGIC_BYTES + \
				  BOOTCONFIG_SIZE_BYTES + \
				  BOOTCONFIG_CHECKSUM_BYTES)

/**
 * Add new string to bootconfig parameters, update trailer structure.
 *
 * In case provided key already exists, update its value.
 *
 * @param key - pointer to the string comprising key
 * @param value - pointer to the string comprising value for key
 * @param bootc_start - pointer to the bootconfig section
 * @param bootc_size - current size of bootconfig structure
 * @param buffer_size - size allocated for bootconfig structure
 *
 * @param return: New size of bootconfig params section after update,
 *                -1 on error
 */
int append_bootconfig_params(char *key, char *value, void *bootc_start,
			     size_t bootc_size, size_t buffer_size);

/**
 * Parse bootconfig section created at build time (usually on vendor_boot
 * partition) and based on this create eventual bootconfig structure at the
 * provided target address.
 *
 * @param bootc_start - Start address of bootconfig section in memory (usually right
 *               after ramdisks)
 * @param bootc_params - pointer to the buffer with bootconfig parameters
 * @param params_size - size in bytes of bootc_params
 *
 * @return Size of the whole bootconfig structure,
 *         -1 on error
 */
int parse_build_time_bootconfig(void *bootc_start, void *bootc_params,
				size_t params_size);

/*
 * Replace spaces in original string with new line characters. Update bootconfig
 * structure with the newly created string.
 *
 * @param cmdline_string - pointer to the cmdline string which should be added
 *                         to bootconfig
 * @param bootc_start - pointer to the bootconfig section
 * @param bootc_size - current size of bootconfig structure
 *
 * Return: New size of bootconfig params section after update
 *         -1 in case of errors
 */
int bootconfig_append_cmdline(char *cmdline_string, void *bootc_start, size_t bootc_size);

#endif /* __BOOT_BOOTCONFIG_H__ */
