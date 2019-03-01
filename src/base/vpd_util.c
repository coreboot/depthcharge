/*
 * Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */
#include <assert.h>
#include <libpayload.h>
#include "vpd_util.h"

enum {
	VPD_OK = 0,
	VPD_FAIL,
};

enum {
	VPD_TYPE_TERMINATOR = 0,
	VPD_TYPE_STRING,
	VPD_TYPE_INFO = 0xfe,
	VPD_TYPE_IMPLICIT_TERMINATOR = 0xff,
};

/* Callback for vpd_decode_string to invoke. */
typedef int vpd_decode_callback(const uint8_t *key, int32_t key_len,
				const uint8_t *value, int32_t value_len,
				void *arg);

struct vpd_cbmem {
	uint32_t magic;
	uint32_t version;
	uint32_t ro_size;
	uint32_t rw_size;
	uint8_t blob[0];
	/* The blob contains both RO and RW data. It starts with RO (0 ..
	 * ro_size) and then RW (ro_size .. ro_size+rw_size).
	 */
};

struct vpd_gets_arg {
	const uint8_t *key;
	const uint8_t *value;
	int32_t key_len, value_len;
	int matched;
};

int vpd_decode_len(const int32_t max_len, const uint8_t *in, int32_t *length,
		   int32_t *decoded_len)
{
	uint8_t more;
	int i = 0;

	assert(length);
	assert(decoded_len);

	*length = 0;
	do {
		if (i >= max_len)
			return VPD_FAIL;
		more = in[i] & 0x80;
		*length <<= 7;
		*length |= in[i] & 0x7f;
		++i;
	} while (more);

	*decoded_len = i;
	return VPD_OK;
}

/* Sequentially decodes type, key, and value. */
int vpd_decode_string(const int32_t max_len, const uint8_t *input_buf,
		      int32_t *consumed, vpd_decode_callback callback,
		      void *callback_arg)
{
	int type;
	int32_t key_len, value_len;
	int32_t decoded_len;
	const uint8_t *key, *value;

	/* type */
	if (*consumed >= max_len)
		return VPD_FAIL;

	type = input_buf[*consumed];
	switch (type) {
	case VPD_TYPE_INFO:
	case VPD_TYPE_STRING:
		(*consumed)++;

		/* key */
		if (vpd_decode_len(max_len - *consumed,
				   &input_buf[*consumed], &key_len,
				   &decoded_len) != VPD_OK ||
		    *consumed + decoded_len >= max_len) {
			return VPD_FAIL;
		}

		*consumed += decoded_len;
		key = &input_buf[*consumed];
		*consumed += key_len;

		/* value */
		if (vpd_decode_len(max_len - *consumed,
				   &input_buf[*consumed], &value_len,
				   &decoded_len) != VPD_OK ||
		    *consumed + decoded_len > max_len) {
			return VPD_FAIL;
		}
		*consumed += decoded_len;
		value = &input_buf[*consumed];
		*consumed += value_len;

		if (type == VPD_TYPE_STRING)
			return callback(key, key_len, value, value_len,
					callback_arg);

		return VPD_OK;

	default:
		return VPD_FAIL;
	}
	return VPD_OK;
}

static int vpd_gets_callback(const uint8_t *key, int32_t key_len,
			     const uint8_t *value, int32_t value_len, void *arg)
{
	struct vpd_gets_arg *result = (struct vpd_gets_arg *)arg;
	if (key_len != result->key_len ||
	    memcmp(key, result->key, key_len) != 0)
		/* Returns VPD_OK to continue parsing. */
		return VPD_OK;

	result->matched = 1;
	result->value = value;
	result->value_len = value_len;
	/* Returns VPD_FAIL to stop parsing. */
	return VPD_FAIL;
}

const void *vpd_find(const char *key, int *size)
{
	struct vpd_gets_arg arg = {0};
	int consumed = 0;
	const struct vpd_cbmem *vpd;

	vpd = (struct vpd_cbmem *)lib_sysinfo.chromeos_vpd;
	if (!vpd || !vpd->ro_size)
		return NULL;

	arg.key = (const uint8_t *)key;
	arg.key_len = strlen(key);

	while (vpd_decode_string(vpd->ro_size, vpd->blob, &consumed,
				 vpd_gets_callback, &arg) == VPD_OK) {
		/* Iterate until found or no more entries. */
	}

	if (!arg.matched)
		return NULL;

	*size = arg.value_len;
	return arg.value;
}

char *vpd_gets(const char *key, char *buffer, int size)
{
	const void *string_address;
	int string_size;

	string_address = vpd_find(key, &string_size);
	if (!string_address)
		return NULL;

	if (size > (string_size + 1)) {
		memcpy(buffer, string_address, string_size);
		buffer[string_size] = '\0';
	} else {
		memcpy(buffer, string_address, size - 1);
		buffer[size - 1] = '\0';
	}
	return buffer;
}
