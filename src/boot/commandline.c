/*
 * Copyright 2012 Google LLC
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

#include <commonlib/list.h>
#include <ctype.h>
#include <libpayload.h>
#include <stdbool.h>
#include <stdint.h>

#include "boot/commandline.h"

typedef struct {
	const char *param;
	struct list_node list_node;
} ParamNode;

static struct list_node param_list;

void commandline_append(const char *param)
{
	ParamNode *node = xzalloc(sizeof(*node));
	node->param = strdup(param);
	list_insert_after(&node->list_node, &param_list);
}

int commandline_subst(const char *src, char *dest, size_t dest_size,
		      const struct commandline_info *info)
{
	static const char cros_secure[] = "cros_secure ";
	const int cros_secure_size = sizeof(cros_secure) - 1;
	int devnum = info->devnum;
	int partnum = info->partnum;

	/* Confidence check on dest size */
	if (dest_size > 10000)
		return 1;

	/*
	 * Condition "dst + X <= dst_end" checks if there is at least X bytes
	 * left in dst. We use X > 1 so that there is always 1 byte for '\0'
	 * after the loop.
	 *
	 * We constantly estimate how many bytes we are going to write to dst
	 * for copying characters from src or for the string replacements, and
	 * check if there is sufficient space.
	 */

	char *dest_end = dest + dest_size;
	char *dest_start = dest;

#define CHECK_SPACE(bytes) \
	if (!(dest + (bytes) <= dest_end)) { \
		printf("update_cmdline: Out of space.\n"); \
		return 1; \
	}

	// Prepend "cros_secure " to the command line.
	CHECK_SPACE(cros_secure_size);
	memcpy(dest, cros_secure, cros_secure_size);
	dest += (cros_secure_size);

	int c;

	while ((c = *src++)) {
		if (c != '%') {
			CHECK_SPACE(2);
			*dest++ = c;
			continue;
		}

		switch ((c = *src++)) {
		case '\0':
			printf("update_cmdline: Input ended with '%%'\n");
			return 1;
		case 'D':
			/* Confidence check */
			if (devnum < 0 || devnum > 25)
				return 1;
			/*
			 * TODO: Do we have any better way to know whether %D
			 * is replaced by a letter or digits? So far, this is
			 * done by a rule of thumb that if %D is followed by a
			 * 'p' character, then it is replaced by digits.
			 */
			if (*src == 'p') {
				CHECK_SPACE(3);
				dest = itoa(dest, devnum);
			} else {
				CHECK_SPACE(2);
				*dest++ = 'a' + devnum;
			}
			break;
		case 'P':
			/* Confidence check */
			if (partnum < 1 || partnum > 99)
				return 1;
			CHECK_SPACE(3);
			dest = itoa(dest, partnum);
			break;
		case 'U':
			/* GUID replacement needs 36 bytes */
			CHECK_SPACE(GUID_STRLEN);
			GptGuidToStr(info->guid, dest, GUID_STRLEN,
				     GPT_GUID_LOWERCASE);
			dest += (GUID_STRLEN - 1);;
			break;
		case 'R':
			/*
			 * If booting from NAND, /dev/ubiblock%P_0
			 * If booting from disk, PARTUUID=%U/PARTNROFF=1
			 */
			if (info->external_gpt) {
				/* Confidence check */
				if (partnum < 1 || partnum > 99)
					return 1;
				char start[] = "/dev/ubiblock", end[] = "_0";
				size_t start_size = sizeof(start) - 1,
					end_size = sizeof(end) - 1;
				CHECK_SPACE(start_size + 3 + end_size);
				memcpy(dest, start, start_size);
				dest += start_size;
				dest = itoa(dest, partnum);
				memcpy(dest, end, end_size);
				dest += end_size;
			} else {
				char start[] = "PARTUUID=",
					end[] = "/PARTNROFF=1";
				size_t start_size = sizeof(start) - 1,
					end_size = sizeof(end) - 1;
				CHECK_SPACE(start_size + 36 + 1 + end_size);
				memcpy(dest, start, start_size);
				dest += start_size;
				dest = emit_guid(dest, info->guid);
				memcpy(dest, end, end_size);
				dest += end_size;
			}
			break;
		default:
			CHECK_SPACE(3);
			*dest++ = '%';
			*dest++ = c;
			break;
		}
	}

	ParamNode *node;
	list_for_each(node, param_list, list_node) {
		dest += snprintf(dest, dest_end - dest, " %s", node->param);
		CHECK_SPACE(0);
	}

#undef CHECK_SPACE

	*dest = '\0';
	printf("Modified kernel command line: %s\n", dest_start);

	return 0;
}

static const char *get_cmdline_value(const char *cmdline, const char *key)
{
	const char *pos = cmdline;
	size_t key_len;

	if (!cmdline || !key)
		return NULL;

	key_len = strlen(key);

	while (*pos) {
		/* Find the start of the next argument */
		while (isspace(*pos))
			pos++;
		if (!*pos)
			break;

		/* Check if the current argument matches the key */
		if (strncmp(pos, key, key_len) == 0 && pos[key_len] == '=')
			return pos + key_len + 1;

		/* Move to the next argument */
		bool in_quote = false;
		while (*pos && (in_quote || !isspace(*pos))) {
			if (*pos == '"')
				in_quote = !in_quote;
			pos++;
		}
	}

	return NULL;
}

int get_cmdline_uint32_value(const char *cmdline, const char *key, uint32_t *out)
{
	const char *value_start = get_cmdline_value(cmdline, key);
	unsigned long val;
	char *endptr;
	bool quoted = false;

	if (out == NULL)
		return -1; /* Invalid output pointer */

	if (value_start == NULL)
		return -1;

	if (*value_start == '"') {
		quoted = true;
		value_start++; /* Skip opening quote */
	}

	val = strtoul(value_start, &endptr, 0);

	/* If no characters were converted */
	if (value_start == endptr)
		return -1;

	if (quoted) {
		/* If it was quoted, endptr must point to the closing quote */
		if (*endptr != '"')
			return -1;
		endptr++; /* Move past the closing quote */
	}

	/* After the value, it must be end of string or whitespace */
	if (*endptr != '\0' && !isspace(*endptr))
		return -1;

	/* Check for overflow */
	if (val > UINT32_MAX)
		return -1;

	*out = (uint32_t)val;
	return 0;
}
