/*
 * Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */
#include <assert.h>
#include <libpayload.h>
#include "vpd_decode.h"
#include "vpd_util.h"

/* Currently we only support Google VPD 2.0, which has a fixed offset. */
enum {
	GOOGLE_VPD_2_0_OFFSET = 0x600,
};

#define VPD_INFO_MAGIC \
	"\xfe"		/* type: VPD header */ \
	"\x09"		/* key length, 9 = 1 + 8 */ \
	"\x01"		/* info version, 1 */ \
	"gVpdInfo"	/* signature, 8 bytes */ \
	"\x04"		/* value length */

/* Google specific VPD info */
struct google_vpd_info {
	union {
		struct {
			uint8_t type;
			uint8_t key_len;
			uint8_t info_ver;
			uint8_t signature[8];
			uint8_t value_len;
		} tlv;
		uint8_t magic[12];
	} header;
	uint32_t size;
} __packed;

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
	u32 key_len, value_len;
	int matched;
};

static int vpd_gets_callback(const u8 *key, u32 key_len,
			     const u8 *value, u32 value_len, void *arg)
{
	struct vpd_gets_arg *result = (struct vpd_gets_arg *)arg;
	if (key_len != result->key_len ||
	    memcmp(key, result->key, key_len) != 0)
		/* Returns VPD_OK to continue parsing. */
		return VPD_DECODE_OK;

	result->matched = 1;
	result->value = value;
	result->value_len = value_len;
	/* Returns VPD_DECODE_FAIL to stop parsing. */
	return VPD_DECODE_FAIL;
}

const u8 *vpd_find(const char *key, const u8 *blob, u32 *offset, u32 *size)
{
	struct vpd_gets_arg arg = {0};
	u32 consumed = 0;
	u32 vpd_size;
	struct google_vpd_info vpd_info;
	const struct vpd_cbmem *vpd;
	const u8 *vpd_data;

	if (blob) {
		vpd_data = blob + GOOGLE_VPD_2_0_OFFSET;
		memcpy(&vpd_info, vpd_data, sizeof(vpd_info));

		/* Check that blob really is a vpd section */
		if (memcmp(vpd_info.header.magic, VPD_INFO_MAGIC,
		    sizeof(vpd_info.header.magic)) != 0) {
			printf("%s: invalid vpd blob\n", __func__);
			return NULL;
		}

		vpd_size = vpd_info.size;
		vpd_data += sizeof(vpd_info);
	} else {
		vpd = (struct vpd_cbmem *)lib_sysinfo.chromeos_vpd;
		if (!vpd || !vpd->ro_size)
			return NULL;
		blob = vpd->blob;
		vpd_data = blob;
		vpd_size = vpd->ro_size;
	}

	arg.key = (const u8 *)key;
	arg.key_len = strlen(key);

	while (vpd_decode_string(vpd_size, vpd_data, &consumed,
				 vpd_gets_callback, &arg) == VPD_DECODE_OK) {
		/* Iterate until found or no more entries. */
	}

	if (!arg.matched)
		return NULL;

	*size = arg.value_len;
	*offset = arg.value - blob;
	return arg.value;
}

char *vpd_gets(const char *key, char *buffer, u32 size)
{
	const void *string_address;
	u32 string_size, offset;

	string_address = vpd_find(key, NULL, &offset, &string_size);
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
