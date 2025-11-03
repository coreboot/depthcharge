// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>

#include "boot/bootconfig.h"
#include "boot/commandline.h"

void bootconfig_init(struct bootconfig *bc, void *start, size_t buffer_size)
{
	assert(bc != NULL && start != NULL);
	assert(buffer_size >= sizeof(struct bootconfig_trailer) + 1);

	/* Enough place, bootconfig is placed right after ramdisks */
	bc->start = start;
	/* Reserved space for bootconfig trailer and terminating character */
	bc->limit = buffer_size - sizeof(struct bootconfig_trailer) - 1;
	bc->size = 0;
	/* To make thing easier treat bootconfig as a string */
	bc->start[0] = '\0';
}

int bootconfig_reinit(struct bootconfig *bc, void *trailer_addr)
{
	struct bootconfig_trailer *trailer = trailer_addr;

	assert(bc != NULL && trailer != NULL);

	if (memcmp(trailer->magic, BOOTCONFIG_MAGIC, BOOTCONFIG_MAGIC_BYTES))
		return -1;

	bc->start = (char *)trailer - trailer->params_size;
	bc->limit = trailer->params_size;

	/* String length should be at most bc->limit - 1. */
	bc->size = strnlen(bc->start, bc->limit);
	if (bc->size == bc->limit)
		return -1;

	return 0;
}

int bootconfig_append_params(struct bootconfig *bc, void *bootsection_params,
			     size_t bootsection_size)
{
	if (!bootsection_size)
		return 0;

	if (bc->size + bootsection_size > bc->limit)
		return -1;

	memmove(&bc->start[bc->size], bootsection_params, bootsection_size);
	bc->size += bootsection_size;
	bc->start[bc->size] = '\0';

	return 0;
}

int bootconfig_append_cmdline(struct bootconfig *bc, char *cmdline_string)
{
	bool in_quote = false;
	char *in, *out;
	int ret = -1;

	for (in = cmdline_string, out = bc->start + bc->size; ; in++, out++) {
		if (out >= bc->start + bc->limit)
			goto exit;

		if (!in_quote && isspace(in[0])) {
			out[0] = BOOTCONFIG_DELIMITER;
			while (isspace(in[1]))
				in++;
			continue;
		}
		if (in[0] == '"')
			in_quote = !in_quote;

		if (in[0] == '\0') {
			*out++ = BOOTCONFIG_DELIMITER;
			break;
		}

		out[0] = in[0];
	}

	if (in_quote)
		goto exit;

	bc->size = out - bc->start;
	ret = 0;

exit:
	bc->start[bc->size] = '\0';
	return ret;
}

int bootconfig_append(struct bootconfig *bc, const char *key, const char *value)
{
	const char *forbidden_separators = ";#}'\"\n";
	char *end;
	int len;
	size_t space;

	assert(bc != NULL && key != NULL && value != NULL);

	/* Exit early if the value contains any of the forbidden characters */
	len = strcspn(value, forbidden_separators);
	if (len != strlen(value)) {
		printf("Forbidden character \" %c \" found, \
			skip appending:\nkey=%s value=%s\n",
			value[len], key, value);
		return -1;
	}

	/* Add parameters at the end */
	space = bc->limit - bc->size;
	end = &bc->start[bc->size];
	len = snprintf(end, space, "%s=%s%c", key, value, BOOTCONFIG_DELIMITER);
	if (len >= space || len < 0) {
		bc->start[bc->size] = '\0';
		return -1;
	}
	bc->size += len;

	return 0;
}

static uint32_t bootconfig_checksum(char *bootconfig, size_t size)
{
	uint32_t sum = 0;

	for (size_t i = 0; i < size; i++)
		sum += bootconfig[i];

	return sum;
}

void bootconfig_checksum_recalculate(struct bootconfig *bc, struct bootconfig_trailer *trailer)
{
	trailer->params_checksum = bootconfig_checksum(bc->start, trailer->params_size);
}

struct bootconfig_trailer *bootconfig_finalize(struct bootconfig *bc, size_t reserved)
{
	struct bootconfig_trailer *trailer;
	size_t full_size;
	uint32_t sum;

	if (bc->limit < bc->size + reserved + 1)
		return NULL;

	/* Make sure that reserved space and byte for null termination are zeroed */
	memset(&bc->start[bc->size], 0, reserved + 1);

	full_size = bc->size + reserved + 1;
	trailer = (struct bootconfig_trailer *)(bc->start + full_size);

	/* size */
	trailer->params_size = full_size;

	/* checksum */
	sum = bootconfig_checksum(bc->start, trailer->params_size);
	trailer->params_checksum = sum;

	/* magic at the end of trailer */
	memcpy(trailer->magic, BOOTCONFIG_MAGIC, BOOTCONFIG_MAGIC_BYTES);

	return trailer;
}
