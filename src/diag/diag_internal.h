/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __DIAG_DIAG_INTERNAL_H__
#define __DIAG_DIAG_INTERNAL_H__

#include <libpayload.h>

#define APPEND(buf, end, format, args...) ( \
	(buf) + ((buf) < (end) ? \
		MIN(snprintf((buf), (end) - (buf), format, ## args), \
		    (end) - (buf)) : \
		0) \
)

#endif
