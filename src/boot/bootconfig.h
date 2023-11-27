/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __BOOT_BOOTCONFIG_H__
#define __BOOT_BOOTCONFIG_H__

#define BOOTCONFIG_DELIMITER '\n'
#define BOOTCONFIG_MAGIC "#BOOTCONFIG\n"
#define BOOTCONFIG_MAGIC_BYTES 12

struct bootconfig {
	char *start;
	size_t size;
	size_t limit;
};

struct bootconfig_trailer {
	uint32_t params_size;
	uint32_t params_checksum;
	char magic[BOOTCONFIG_MAGIC_BYTES];
};

/*
 * Initialize bootconfig structure with reserved space start and size.
 *
 * @param bc - pointer to bootconfig structure
 * @param start - pointer to the start of reserved space
 * @param buffer_size - size in bytes of reserved space
 */
void bootconfig_init(struct bootconfig *bc, void *start, size_t buffer_size);

/*
 * Reinitialize bootconfig structure from trailer data.
 *
 * @param bc - pointer to bootconfig structure
 * @param trailer_addr - pointer to the bootconfig trailer
 *
 * @return 0 on success, -1 on error
 */
int bootconfig_reinit(struct bootconfig *bc, void *trailer_addr);

/**
 * Parse bootconfig section created at build time (usually on vendor_boot
 * partition) and append the params to bootconfig reserved space.
 *
 * @param bc - pointer to bootconfig structure
 * @param bootsection_params - pointer to the bootsection part of vendor image
 * @param bootsection_size - size in bytes of the bootsection
 *
 * @return 0 on success, -1 on error
 */
int bootconfig_append_params(struct bootconfig *bc, void *bootsection_params,
			     size_t bootsection_size);

/*
 * Replace spaces in original string with new line characters. Update bootconfig
 * structure with the newly created string.
 *
 * @param bc - pointer to bootconfig structure
 * @param cmdline_string - pointer to the cmdline string which should be added
 *                         to bootconfig
 *
 * @return 0 on success, -1 on error
 */
int bootconfig_append_cmdline(struct bootconfig *bc, char *cmdline_string);

/**
 * Add new string to bootconfig parameter.
 *
 * @param bc - pointer to bootconfig structure
 * @param key - pointer to the string comprising key
 * @param value - pointer to the string comprising value for key
 *
 * @return 0 on success, -1 on error
 */
int bootconfig_append(struct bootconfig *bc, const char *key, const char *value);

/*
 * Recalculate checksum for bootconfig params and update checksum in trailer.
 *
 * @param bc - pointer to the bootconfig structure
 * @param trailer - pointer to the bootconfig trailer
 */
void bootconfig_checksum_recalculate(struct bootconfig *bc,
				     struct bootconfig_trailer *trailer);
/*
 * Creates trailer at the end of the bootconfig.
 * @param bc - pointer to bootconfig structure
 * @param reserved - size of space reserved for future params addition
 *
 * @return: pointer to the trailer on success or NULL otherwise
 */
struct bootconfig_trailer *bootconfig_finalize(struct bootconfig *bc, size_t reserved);

#endif /* __BOOT_BOOTCONFIG_H__ */
