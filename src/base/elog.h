/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __BASE_ELOG_H__
#define __BASE_ELOG_H__

#include "image/fmap.h"

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

#endif /* __BASE_ELOG_H__ */
