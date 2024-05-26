/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __BASE_ELOG_H__
#define __BASE_ELOG_H__

#include <libpayload.h>

#include "image/fmap.h"

/* Only refers to the data max size. The "-1" is the checksum byte */
#define ELOG_MAX_EVENT_DATA_SIZE					       \
	(ELOG_MAX_EVENT_SIZE - sizeof(struct event_header) - 1)

typedef enum {
	ELOG_SUCCESS = 0,
	/* Cannot initialize elog driver */
	ELOG_ERR_BROKEN,
	/* Elog is not implemented */
	ELOG_ERR_NOT_IMPLEMENTED,
	/* External source error (e.g. fmap, flash) */
	ELOG_ERR_EX,
	/* NV content error */
	ELOG_ERR_CONTENT,
} elog_error_t;

elog_error_t elog_init(void);
elog_error_t elog_add_event_raw(uint8_t event_type, void *data,
				uint8_t data_size);
elog_error_t elog_add_event(uint8_t event_type);
elog_error_t elog_add_event_byte(uint8_t event_type, uint8_t data);
elog_error_t elog_add_event_word(uint8_t event_type, uint16_t data);
elog_error_t elog_add_event_dword(uint8_t event_type, uint32_t data);
#endif /* __BASE_ELOG_H__ */
