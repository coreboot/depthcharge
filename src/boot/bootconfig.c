// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/string_utils.h"
#include "boot/bootconfig.h"
#include "boot/commandline.h"

static char *bootconfig_set_key_value(const char *src, char *key, char *value)
{
	return set_key_value_in_separated_list(src, key, value, "\n");
}

/*
 * Calculate checksum for bootconfig params.
 *
 * @param addr pointer to the start of the buffer.
 * @param size size of the buffer in bytes.
 * @return checksum result.
 */
static uint32_t checksum(const unsigned char *const buffer, size_t size) {
	uint32_t sum = 0;

	for (size_t i = 0; i < size; i++)
		sum += buffer[i];

	return sum;
}

/*
 * Add boot config trailer.
 */
static void append_bootconfig_trailer(void *bootc_start, size_t params_size)
{
	uint8_t *trailer;
	uint32_t sum;

	trailer = (uint8_t *)bootc_start + params_size;

	/* size */
	memcpy(trailer, &params_size, BOOTCONFIG_SIZE_BYTES);

	/* checksum */
	sum = checksum((unsigned char *)bootc_start, params_size);
	memcpy(trailer + BOOTCONFIG_SIZE_BYTES, &sum,
	       BOOTCONFIG_CHECKSUM_BYTES);

	/* magic at the end of trailer */
	memcpy(trailer + BOOTCONFIG_SIZE_BYTES + BOOTCONFIG_CHECKSUM_BYTES,
	       BOOTCONFIG_MAGIC, BOOTCONFIG_MAGIC_BYTES);
}

int bootconfig_append_cmdline(char *cmdline_string, void *bootc_start, size_t bootc_size)
{
	char *p, *p_prev, *tmp;
	size_t len, bootparams_size;

	/* Make a copy for modifications */
	tmp = strdup(cmdline_string);
	if (tmp == NULL)
		return -1;

	p = p_prev = tmp;
	while(p) {
		p = strstr(tmp, " ");
		if (p == NULL)
			break;
		*p = '\n';
		/* Check for the case with multiple consecutive spaces */
		if (p - p_prev == 1) {
			memmove(p, p + 1, strlen(p + 1));
			*(p + strlen(p) - 1) = '\0';
			continue;
		}
		p_prev = p;
	}

	len = strlen(tmp);
	bootparams_size = bootc_size - BOOTCONFIG_TRAILER_BYTES;

	/* Append string at the end of the current bootconfig params string */
	memcpy((char *)bootc_start + bootparams_size, tmp, len);
	append_bootconfig_trailer(bootc_start, bootparams_size + len);

	free(tmp);
	return bootc_size + len;
}

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
			     size_t bootc_size, size_t buffer_size)
{
	size_t size;
	char *bootc_str = (char *)bootc_start;
	char *updated_str;

	if (bootc_start == NULL || key == NULL || value == NULL)
		return -1;

	/*
	 * If bootconfig structure doesn't have trailer (its current size is
	 * 0), we will need to construct it from scratch after adding key=value
	 * string. Shadow `bootc_size` real value, so that algorithm below will
	 * take care of it.
	 */
	if (bootc_size == 0)
		bootc_size = BOOTCONFIG_TRAILER_BYTES;

	/*
	 * Firstly, we need to append NULL terminator to bootconfig entries to
	 * convert it to string acceptable by API used below. Trailer can be
	 * overridden as we will reconstruct it anyway.
	 */
	bootc_str[bootc_size - BOOTCONFIG_TRAILER_BYTES] = '\0';

	updated_str = bootconfig_set_key_value(bootc_str, key, value);

	/* Check if caller has enough space allocated */
	if (buffer_size < strlen(updated_str) + BOOTCONFIG_TRAILER_BYTES) {
		free(updated_str);
		return -1;
	}

	/* Don't include string terminator */
	memcpy((void *)bootc_str, updated_str, strlen(updated_str));
	append_bootconfig_trailer(bootc_start, strlen(updated_str));

	size = strlen(updated_str) + BOOTCONFIG_TRAILER_BYTES;
	free(updated_str);

	return size;
}

/**
 * Parse bootconfig section created at build time (usually on vendor_boot
 * partition) and based on this create eventual bootconfig structure at the
 * provided target address.
 *
 * @param bootc_start - Start address of bootconfig section in memory (usually right
 *                      after ramdisks)
 * @param bootc_params - pointer to the buffer with bootconfig parameters
 * @param params_size - size in bytes of bootc_params
 *
 * @return Size of the whole bootconfig structure,
 *         -1 on error
 */
int parse_build_time_bootconfig(void *bootc_start, void *bootc_params,
				size_t params_size)
{
	if (bootc_start == NULL || bootc_params == NULL)
		return -1;

	if (params_size != 0) {
		memcpy(bootc_start, bootc_params, params_size);
	} else {
		/*
		 * "bootconfig" is appended to command line only
		 * if there are parameters added by Android. Since
		 * bootconfig is created from scratch command line
		 * must be modified here.
		 */
		commandline_append("bootconfig");
	}
	append_bootconfig_trailer(bootc_start, params_size);

	return params_size + BOOTCONFIG_TRAILER_BYTES;
}
