// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/string_utils.h"
#include "boot/bootconfig.h"
#include "boot/commandline.h"

static char *set_key_value(const char *src, char *key, char *value)
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
static void append_trailer(void *bootc_start, size_t params_size)
{
	struct bootconfig_trailer *trailer;
	uint32_t sum;

	trailer = (struct bootconfig_trailer *)((uint8_t *)bootc_start + params_size);

	/* size */
	trailer->params_size = params_size;

	/* checksum */
	sum = checksum((unsigned char *)bootc_start, params_size);
	trailer->params_checksum = sum;

	/* magic at the end of trailer */
	memcpy(trailer->magic, BOOTCONFIG_MAGIC, BOOTCONFIG_MAGIC_BYTES);
}

int bootconfig_append_cmdline(struct bootconfig *bc, char *cmdline_string)
{
	char *p, *p_prev, *tmp;
	size_t len, bootparams_size;

	/* Make a copy for modifications */
	tmp = strdup(cmdline_string);
	if (tmp == NULL)
		return -1;

	p = p_prev = tmp;
	while ((p = strstr(tmp, " "))) {
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
	if (bc->bootc_size == 0)
		bootparams_size = 0;
	else
		bootparams_size = bc->bootc_size - BOOTCONFIG_TRAILER_BYTES;

	/* Check if caller has enough space allocated */
	if (bc->bootc_limit < bootparams_size + len + BOOTCONFIG_TRAILER_BYTES) {
		free(tmp);
		return -1;
	}

	/* Append string at the end of the current bootconfig params string */
	memcpy(bc->bootc_start + bootparams_size, tmp, len);
	append_trailer(bc->bootc_start, bootparams_size + len);

	free(tmp);
	bc->bootc_size = bc->bootc_size + len;
	return 0;
}

int bootconfig_append_params(struct bootconfig *bc, char *key, char *value)
{
	size_t size;
	char *bootc_str = (char *)bc->bootc_start;
	char *updated_str;

	if (bc == NULL || key == NULL || value == NULL)
		return -1;

	/*
	 * If bootconfig structure doesn't have trailer (its current size is
	 * 0), we will need to construct it from scratch after adding key=value
	 * string. Shadow `bootc_size` real value, so that algorithm below will
	 * take care of it.
	 */
	if (bc->bootc_size == 0)
		bc->bootc_size = BOOTCONFIG_TRAILER_BYTES;

	/*
	 * Firstly, we need to append NULL terminator to bootconfig entries to
	 * convert it to string acceptable by API used below. Trailer can be
	 * overridden as we will reconstruct it anyway.
	 */
	bootc_str[bc->bootc_size - BOOTCONFIG_TRAILER_BYTES] = '\0';

	updated_str = set_key_value(bootc_str, key, value);

	/* Check if caller has enough space allocated */
	if (bc->bootc_limit < strlen(updated_str) + BOOTCONFIG_TRAILER_BYTES) {
		free(updated_str);
		return -1;
	}

	/* Don't include string terminator */
	memcpy(bc->bootc_start, updated_str, strlen(updated_str));
	append_trailer(bc->bootc_start, strlen(updated_str));

	size = strlen(updated_str) + BOOTCONFIG_TRAILER_BYTES;
	free(updated_str);

	bc->bootc_size = size;
	return size;
}

int bootconfig_init(struct bootconfig *bc, void *bootsection_params, size_t bootsection_size)
{
	if (bc == NULL || bootsection_params == NULL)
		return -1;

	if ((bootsection_size + BOOTCONFIG_TRAILER_BYTES) > bc->bootc_limit)
		return -1;

	if (bootsection_size != 0) {
		memmove(bc->bootc_start, bootsection_params, bootsection_size);
	} else {
		/*
		 * "bootconfig" is appended to command line only
		 * if there are parameters added by Android. Since
		 * bootconfig is created from scratch command line
		 * must be modified here.
		 */
		commandline_append("bootconfig");
	}
	append_trailer(bc->bootc_start, bootsection_size);

	bc->bootc_size = bootsection_size + BOOTCONFIG_TRAILER_BYTES;
	return 0;
}
