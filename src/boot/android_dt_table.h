/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Private header file for use by android_dtboimg
 *
 * This defines Android device tree table and entry headers as documented in:
 * https://source.android.com/docs/core/architecture/dto/partition. The contents
 * in this file are imported from system/libufdt/utils/src/dt_table_core.h AOSP
 * header file with some modifications.
 */
#ifndef _ANDROID_DT_TABLE_H_
#define _ANDROID_DT_TABLE_H_

#include <stdint.h>

#define DT_TABLE_MAGIC 0xd7b7ab1e
#define DT_TABLE_MAGIC_SIZE 4

struct dt_table_header {
	uint32_t magic;			/* DT_TABLE_MAGIC */
	uint32_t total_size;		/* includes dt_table_header + all dt_table_entry
					   and all dtb/dtbo */
	uint32_t header_size;		/* sizeof(dt_table_header) */

	uint32_t dt_entry_size;		/* sizeof(dt_table_entry_v*) */
	uint32_t dt_entry_count;	/* number of dt_table_entry */
	uint32_t dt_entries_offset;	/* offset to the first dt_table_entry
					   from head of dt_table_header */

	uint32_t page_size;		/* flash page size we assume */
	uint32_t version;		/* DTBO image version, the current version is 1.
					   The version is incremented when the
					   dt_table_header struct is updated. */
};

#define DT_COMPRESSION_MASK 0xf
enum dt_compression_info {
	NO_COMPRESSION,
	ZLIB_COMPRESSION,
	GZIP_COMPRESSION,
};

struct dt_table_entry_v1 {
	uint32_t dt_size;
	uint32_t dt_offset;	/* offset from head of dt_table_header */

	uint32_t id;		/* optional, must be zero if unused */
	uint32_t rev;		/* optional, must be zero if unused */
	uint32_t flags;		/* For version 1 of dt_table_header, the 4 least significant
				   bits of 'flags' will be used indicate the compression format
				   of the DT entry as per the enum 'dt_compression_info' */
	uint32_t custom[3];	/* optional, must be zero if unused */
};

#endif /* _ANDROID_DT_TABLE_H_ */
