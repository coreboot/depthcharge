// SPDX-License-Identifier: GPL-2.0

#include "board/skywalker/include/variant.h"

int override_pdc_chip(struct pdc_chip *pdc)
{
	if (pdc->chip == PDC_RTS5452P &&
	    pdc->retimer == PDC_RETIMER_PS8747) {
		pdc->config.variant = '1'; /* 0B01 */
		return 0;
	}
	if (pdc->chip == PDC_RTS5452P_VB) {
		if (pdc->retimer == PDC_RETIMER_PS8747 ||
		pdc->retimer == PDC_RETIMER_IT5205) {
			pdc->config.variant = '0'; /* 0U00/0W00 */
			return 0;
		}
	}

return 0;
}
