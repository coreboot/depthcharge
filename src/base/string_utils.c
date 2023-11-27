// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <string.h>

char *set_key_value_in_separated_list(const char *src, const char *key,
				      const char *value, const char *separator)
{
	char *tmp;
	char *dst;
	int str_idx;

	if (src == NULL || key == NULL || value == NULL || separator == NULL)
		return NULL;
	/*
	 * Get enough space for the worst case, where we need to insert key,
	 * value and separator, '=' and NULL terminator before potential new param.
	 */
	dst = calloc(1, strlen(src) + strlen(separator) + strlen(key) +
			strlen(value) + 2);
	if (dst == NULL)
		return dst;

	tmp = strstr(src, key);

	/* If there is no "<key>", add "<key>=<value>" string */
	if (tmp == NULL) {
		if (src == NULL || strlen(src) == 0)
			sprintf(dst, "%s=%s", key, value);
		else
			sprintf(dst, "%s%s%s=%s", src, separator, key, value);
		return dst;
	}

	/* 1. Copy old content up to and including "key=" */
	str_idx = (tmp - src) + strlen(key) + 1;
	memcpy(dst, src, str_idx);

	/* 2. Copy value for the key */
	memcpy(dst + str_idx, value, strlen(value));

	tmp = strstr(tmp, separator);
	if (tmp == NULL) {
		/* No space after <key><separator> in src, what means we don't
		   have more to copy */
		return dst;
	}

	/* 3. Copy rest of the old string if necessary */
	memcpy(dst + strlen(value) + str_idx, tmp, strlen(tmp) + 1);

	return dst;
}
