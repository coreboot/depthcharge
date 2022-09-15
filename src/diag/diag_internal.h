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

/* For diagnostics reports */
#define DIAG_REPORT_EVENT_TYPE_MAX GENMASK(ELOG_CROS_DIAG_LOG_TYPE_BITS, 0)
#define DIAG_REPORT_EVENT_RESULT_MAX GENMASK(ELOG_CROS_DIAG_LOG_TYPE_BITS, 0)

#define DIAG_REPORT_EVENT_MAX 50

#endif
