// SPDX-License-Identifier: GPL-2.0

#include "board/skywalker/include/variant.h"

int override_pdc_chip(struct pdc_chip *pdc)
{
	if (pdc->chip == PDC_RTS5452P &&
	    pdc->retimer == PDC_RETIMER_PS8747) {
		pdc->config.variant = '3'; /* 0B03 */
		return 0;
	}
	return 0;
}
