/*
 * Copyright 2012 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <vboot_api.h>

#include "drivers/tpm/tpm.h"

VbError_t VbExTpmInit(void)
{
	if (tis_probe())
		return VBERROR_UNKNOWN;
	return VbExTpmOpen();
}

VbError_t VbExTpmClose(void)
{
	u8 locality = 0;
	if (tpm_read(locality, TIS_REG_ACCESS) &
	    TIS_ACCESS_ACTIVE_LOCALITY) {
		tpm_write(TIS_ACCESS_ACTIVE_LOCALITY, locality, TIS_REG_ACCESS);

		if (tis_wait_reg(TIS_REG_ACCESS, locality,
				 TIS_ACCESS_ACTIVE_LOCALITY, 0) ==
		    TPM_TIMEOUT_ERR) {
			printf("%s:%d - failed to release locality %d\n",
			       __FILE__, __LINE__, locality);
			return VBERROR_UNKNOWN;
		}
	}
	return VBERROR_SUCCESS;
}

VbError_t VbExTpmOpen(void)
{
	u8 locality = 0; /* we use locality zero for everything */

	if (VbExTpmClose())
		return VBERROR_UNKNOWN;

	/* now request access to locality */
	tpm_write(TIS_ACCESS_REQUEST_USE, locality, TIS_REG_ACCESS);

	/* did we get a lock? */
	if (tis_wait_reg(TIS_REG_ACCESS, locality,
			 TIS_ACCESS_ACTIVE_LOCALITY,
			 TIS_ACCESS_ACTIVE_LOCALITY) == TPM_TIMEOUT_ERR) {
		printf("%s:%d - failed to lock locality %d\n",
		       __FILE__, __LINE__, locality);
		return VBERROR_UNKNOWN;
	}

	/* Certain TPMs seem to need some delay here or they hang... */
	udelay(50);

	if (tis_command_ready(locality) == TPM_TIMEOUT_ERR)
		return TPM_DRIVER_ERR;

	return VBERROR_SUCCESS;
}

VbError_t VbExTpmSendReceive(const uint8_t *request, uint32_t request_length,
			     uint8_t *response, uint32_t *response_length)
{
	if (tis_senddata(request, request_length)) {
		printf("%s:%d failed sending data to TPM\n",
		       __FILE__, __LINE__);
		return VBERROR_UNKNOWN;
	}
	if (tis_readresponse(response, response_length))
		return VBERROR_UNKNOWN;
	return VBERROR_SUCCESS;
}
