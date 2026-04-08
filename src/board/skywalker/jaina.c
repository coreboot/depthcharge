// SPDX-License-Identifier: GPL-2.0

#include "board/skywalker/include/variant.h"

int override_pdc_chip(struct pdc_chip *pdc)
{
	if (pdc->chip == PDC_RTS5453P_VB &&
	    pdc->retimer == PDC_RETIMER_TUSB546_2) {
		pdc->config.variant = '0'; /* 0X00 */
		return 0;
	}
	return 0;
}
