/*
 * Copyright (C) 2011 Infineon Technologies
 * Copyright 2013 Google Inc.
 *
 * Authors:
 * Peter Huewe <huewe.external@infineon.com>
 *
 * Description:
 * Device driver for TCG/TCPA TPM (trusted platform module).
 * Specifications at www.trustedcomputinggroup.org
 *
 * It is based on the Linux kernel driver tpm.c from Leendert van
 * Dorn, Dave Safford, Reiner Sailer, and Kyleen Hall.
 *
 * Version: 2.1.1
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <endian.h>
#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/tpm/i2c.h"
#include "drivers/tpm/tpm.h"

static int i2ctpm_close_cleanup(CleanupFunc *cleanup, CleanupType type)
{
	I2cTpm *tpm = cleanup->data;
	return tpm->chip_ops.cleanup(&tpm->chip_ops);
}

static int i2ctpm_init(I2cTpm *tpm)
{
	/*
	 * Probe TPM twice; the first probing might fail because TPM is asleep,
	 * and the probing can wake up TPM.
	 */
	if (i2c_writeb(tpm->bus, tpm->addr, 0, 0) &&
	    i2c_writeb(tpm->bus, tpm->addr, 0, 0))
		return -1;

	if (tpm->chip_ops.init(&tpm->chip_ops))
		return -1;

	list_insert_after(&tpm->cleanup.list_node, &cleanup_funcs);

	tpm->initialized = 1;

	return 0;
}

static int i2ctpm_xmit(TpmOps *me, const uint8_t *sendbuf, size_t sbuf_size,
		       uint8_t *recvbuf, size_t *rbuf_len)
{
	I2cTpm *tpm = container_of(me, I2cTpm, ops);

	if (!tpm->initialized && i2ctpm_init(tpm))
		return -1;

	uint32_t count, ordinal;
	memcpy(&count, sendbuf + TpmCmdCountOffset, sizeof(count));
	count = betohl(count);
	memcpy(&ordinal, sendbuf + TpmCmdOrdinalOffset, sizeof(ordinal));
	ordinal = betohl(ordinal);

	if (count == 0) {
		printf("%s: No data.\n", __func__);
		return -1;
	}
	if (count > sbuf_size) {
		printf("%s: Invalid count value %x, max %zx.\n", __func__,
			count, sbuf_size);
		return -1;
	}

	assert(tpm->chip_ops.send);
	ssize_t rc = tpm->chip_ops.send(&tpm->chip_ops, sendbuf, count);
	if (rc < 0) {
		printf("%s: tpm_send: error %zd\n", __func__, rc);
		*rbuf_len = 0;
		return -1;
	}

	uint64_t start = timer_us(0);
	while (1) {
		assert(tpm->chip_ops.status);
		uint8_t status = tpm->chip_ops.status(&tpm->chip_ops);
		if ((status & tpm->req_complete_mask) ==
				tpm->req_complete_val) {
			break;
		}

		if (status == tpm->req_canceled) {
			printf("%s: Operation canceled.\n", __func__);
			return -1;
		}
		mdelay(1);

		// Two minute timeout.
		if (timer_us(start) > 2 * 60 * 1000 * 1000) {
			if (tpm->chip_ops.cancel)
				tpm->chip_ops.cancel(&tpm->chip_ops);
			printf("%s: Operation timed out.\n", __func__);
			return -1;
		}
	}

	ssize_t len = tpm->chip_ops.recv(&tpm->chip_ops, recvbuf, *rbuf_len);
	if (len < 0) {
		printf("%s: tpm_recv: error %zd\n", __func__, len);
		*rbuf_len = 0;
		return -1;
	}

	if (len < 10) {
		printf("%s: Too few bytes received (%d).\n", __func__, len);
		*rbuf_len = 0;
		return -1;
	}

	if (len > *rbuf_len) {
		printf("%s: Too many bytes received (%d).\n", __func__, len);
		*rbuf_len = len;
		return -1;
	}

	*rbuf_len = len;
	return 0;
}

void i2ctpm_fill_in(I2cTpm *tpm, I2cOps *bus, uint8_t addr,
		    uint8_t req_complete_mask, uint8_t req_complete_val,
		    uint8_t req_canceled)
{
	memset(tpm, 0, sizeof(*tpm));

	tpm->ops.xmit = &i2ctpm_xmit;

	tpm->bus = bus;
	tpm->addr = addr;

	tpm->req_complete_mask = req_complete_mask;
	tpm->req_complete_val = req_complete_val;
	tpm->req_canceled = req_canceled;

	tpm->cleanup.cleanup = &i2ctpm_close_cleanup;
	tpm->cleanup.types = CleanupOnReboot | CleanupOnPowerOff |
			     CleanupOnHandoff | CleanupOnLegacy;
	tpm->cleanup.data = tpm;
}
