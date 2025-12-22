// SPDX-License-Identifier: GPL-2.0

#include "board/skywalker/include/variant.h"

int override_pdc_chip(struct pdc_chip *pdc)
{
	if (pdc->retimer == PDC_RETIMER_PS8747)
		pdc->config.variant = '2'; /* 0B02 */
	return 0;
}
