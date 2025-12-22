/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOARD_SKYWALKER_VARIANT_H_
#define _BOARD_SKYWALKER_VARIANT_H_

#include <stdint.h>

#define MAX_PDC_PORT_NUM 2
#define PDC_FW_NAME_LEN 8

struct pdc_fw_config {
	union {
		char str[PDC_FW_NAME_LEN + 1];
		struct {
			/* Prefix should always be "GOOG". */
			char prefix[4];
			/* 2-char config identifier. */
			char id[2];
			/* Config revision (0-9A-Za-z). */
			char revision;
			/* Variant (0-9A-Za-z). */
			char variant;
		} __packed;
	};
};

/* Skywalker PDC chip series. */
enum pdc_chip_series {
	PDC_UNKNOWN = 0,
	PDC_RTS5452P,
	PDC_RTS5453P,
	PDC_RTS5452P_VB,
};

enum pdc_retimer {
	PDC_RETIMER_UNKNOWN = 0,
	PDC_RETIMER_NONE,
	PDC_RETIMER_PS8747,
	PDC_RETIMER_TUSB546,
	PDC_RETIMER_IT5205,
	PDC_RETIMER_IT5205_2,
};

struct pdc_chip {
	struct pdc_fw_config config;
	/* Attributes derived from config.id. */
	enum pdc_chip_series chip;
	enum pdc_retimer retimer;
};

int override_pdc_chip(struct pdc_chip *pdc);

#endif /* _BOARD_SKYWALKER_VARIANT_H_ */
