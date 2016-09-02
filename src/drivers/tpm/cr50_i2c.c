/*
 * Copyright 2016 Google Inc.
 * Based on TPM 1.2 driver by Peter Huewe <huewe.external@infineon.com>
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
 */

#include <config.h>
#include <endian.h>
#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/tpm/i2c.h"
#include "drivers/tpm/cr50_i2c.h"

enum {
	TpmTimeout = 750, // usecs
	TpmMaxBurstCount = 63,
};

// Expected value for DIDVID register.
enum {
	TPM_TIS_I2C_DID_VID_CR50 = 0x00281ae0L
};

static const char * const chip_name[] = {
	[CR50] = "cr50",
	[UNKNOWN] = "unknown"
};

/*
 * iic_tpm_read() - read from TPM register
 *
 * @tpm: pointer to TPM structure
 * @addr: register address to read from
 * @buffer: provided by caller
 * @len: number of bytes to read
 *
 * Read len bytes from TPM register and put them into buffer.
 *
 * NOTE: TPM is big-endian for multi-byte values. Multi-byte values have to
 * be swapped.
 *
 * Return nonzero on error, 0 on success.
 */
static int iic_tpm_read(Cr50I2c *tpm, uint8_t addr, uint8_t *buffer,
			size_t len)
{
	int rc;
	int count;

	if (!tpm->base.bus)
		return -1;

	for (count = 0; count < 50; count++) {
		rc = i2c_write_raw(tpm->base.bus, tpm->base.addr, &addr, 1);
		if (rc == 0)
			break;

		udelay(TpmTimeout);
	}

	if (rc)
		return -rc;

	/*
	 * After the TPM has successfully received the register address
	 * it needs some time, thus we're sleeping here again, before
	 * retrieving the data
	 */
	for (count = 0; count < 50; count++) {
		udelay(TpmTimeout);
		rc = i2c_read_raw(tpm->base.bus, tpm->base.addr,
				  buffer, len);
		if (rc == 0)
			break;
	}

	// Take care of 'guard time'.
	udelay(TpmTimeout);
	if (rc)
		return -rc;

	return 0;
}

/*
 * iic_tpm_write() - write to TPM register
 *
 * @tpm: pointer to TPM structure
 * @addr: register address to write to
 * @buffer: containing data to be written
 * @len: number of bytes to write
 *
 * Write len bytes from provided buffer to TPM register (little
 * endian format, i.e. buffer[0] is written as first byte).
 *
 * NOTE: TPM is big-endian for multi-byte values. Multi-byte
 * values have to be swapped.
 *
 * Return nonzero on error, 0 on success
 */
static int iic_tpm_write(Cr50I2c *tpm, uint8_t addr, const uint8_t *buffer,
			 size_t len)
{
	int rc = 0;
	int count;

	if (len > TpmMaxBufSize) {
		printf("%s: Length %zd is too large\n", __func__, len);
		return -1;
	}

	// Prepare send buffer.
	tpm->buf[0] = addr;
	memcpy(&tpm->buf[1], buffer, len);

	if (!tpm->base.bus)
		return -1;
	for (count = 0; count < 50; count++) {
		rc = i2c_write_raw(tpm->base.bus, tpm->base.addr,
				   tpm->buf, len + 1);
		if (rc == 0)
			break;

		udelay(TpmTimeout);
	}

	// Take care of 'guard time'.
	udelay(TpmTimeout);
	if (rc)
		return -rc;

	return 0;
}

#define TPM_HEADER_SIZE 10

enum {
	TpmAccessValid = 0x80,
	TpmAccessActiveLocality = 0x20,
	TpmAccessRequestPending = 0x04,
	TpmAccessRequestUse = 0x02,
};

enum {
	TpmStsValid = 0x80,
	TpmStsCommandReady = 0x40,
	TpmStsGo = 0x20,
	TpmStsDataAvail = 0x10,
	TpmStsDataExpect = 0x08,
};

static inline uint8_t tpm_access(uint8_t locality)
{
	return 0x0 | (locality << 4);
}

static inline uint8_t tpm_sts(uint8_t locality)
{
	return 0x1 | (locality << 4);
}

static inline uint8_t tpm_data_fifo(uint8_t locality)
{
	return 0x5 | (locality << 4);
}

static inline uint8_t tpm_did_vid(uint8_t locality)
{
	return 0x6 | (locality << 4);
}

static int check_locality(Cr50I2c *tpm, int loc)
{
	uint8_t buf;
	int rc;

	rc = iic_tpm_read(tpm, tpm_access(loc), &buf, 1);
	if (rc < 0)
		return rc;

	if ((buf & (TpmAccessActiveLocality | TpmAccessValid)) ==
		(TpmAccessActiveLocality | TpmAccessValid)) {
		tpm->base.locality = loc;
		return loc;
	}

	return -1;
}

static void release_locality(Cr50I2c *tpm, int loc, int force)
{
	uint8_t buf;
	if (iic_tpm_read(tpm, tpm_access(loc), &buf, 1) < 0)
		return;

	if (force || (buf & (TpmAccessRequestPending | TpmAccessValid)) ==
			(TpmAccessRequestPending | TpmAccessValid)) {
		buf = TpmAccessActiveLocality;
		iic_tpm_write(tpm, tpm_access(loc), &buf, 1);
	}
}

static int request_locality(Cr50I2c *tpm, int loc)
{
	uint8_t buf = TpmAccessRequestUse;

	if (check_locality(tpm, loc) >= 0)
		return loc; // We already have the locality.

	iic_tpm_write(tpm, tpm_access(loc), &buf, 1);

	uint64_t start = timer_us(0);
	while (timer_us(start) < 2 * 1000 * 1000) { // Two second timeout.
		if (check_locality(tpm, loc) >= 0)
			return loc;
		udelay(TpmTimeout);
	}

	return -1;
}

static uint8_t tpm_status(I2cTpmChipOps *me)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);
	// NOTE: since i2c read may fail, return 0 in this case --> time-out
	uint8_t buf[4];
	if (iic_tpm_read(tpm, tpm_sts(tpm->base.locality),
			 buf, sizeof(buf)) < 0)
		return 0;
	else
		return buf[0];
}

static void tpm_ready(I2cTpmChipOps *me)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);
	// This causes the current command to be aborted.
	uint8_t buf[4] = { TpmStsCommandReady };
	iic_tpm_write(tpm, tpm_sts(tpm->base.locality), buf, sizeof(buf));
}

static int wait_for_burst_status(Cr50I2c *tpm, uint8_t mask,
				 size_t *burst, int *status)
{
	uint8_t buf[4];
	uint64_t start = timer_us(0);

	while (timer_us(start) < 2 * 1000 * 1000) { // Two second timeout.

		if (iic_tpm_read(tpm, tpm_sts(tpm->base.locality),
				 buf, sizeof(buf)) < 0) {
			printf("%s: Read failed\n", __func__);
			udelay(TpmTimeout);
			continue;
		}

		*status = buf[0];
		*burst = (buf[2] << 8) + buf[1];

		if ((*status & mask) == mask &&
		    *burst > 0 && *burst <= TpmMaxBurstCount)
			return 0;

		udelay(TpmTimeout);
	}

	printf("%s: Timeout reading burst and status\n", __func__);
	return -1;
}

static int tpm_recv(I2cTpmChipOps *me, uint8_t *buf, size_t buf_len)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);

	int ret = -1;
	int status;
	uint32_t expected_buf;
	size_t burstcnt, expected, current, len;
	uint8_t addr = tpm_data_fifo(tpm->base.locality);

	if (buf_len < TPM_HEADER_SIZE)
		goto out;

	if (wait_for_burst_status(tpm, TpmStsValid, &burstcnt, &status) < 0)
		goto out;
	if (!(status & TpmStsDataAvail)) {
		printf("%s: First chunk not available\n", __func__);
		goto out;
	}

	// Read first chunk of burstcnt bytes
	if (iic_tpm_read(tpm, addr, buf, burstcnt) < 0) {
		printf("%s: Read failed\n", __func__);
		goto out;
	}

	memcpy(&expected_buf, buf + TpmCmdCountOffset, sizeof(expected_buf));
	expected = betohl(expected_buf);
	if (expected > buf_len) {
		printf("%s: Too much data: %zu > %zu\n", __func__,
		       expected, buf_len);
		goto out;
	}

	// Now read the rest of the data
	current = burstcnt;
	while (current < expected) {
		if (wait_for_burst_status(tpm, TpmStsValid,
					  &burstcnt, &status) < 0)
			goto out;
		if (!(status & TpmStsDataAvail)) {
			printf("%s: Data not available\n", __func__);
			goto out;
		}

		len = MIN(burstcnt, expected - current);
		if (iic_tpm_read(tpm, addr, buf + current, len) != 0) {
			printf("%s: Read failed\n", __func__);
			goto out;
		}

		current += len;
	}

	if (wait_for_burst_status(tpm, TpmStsValid, &burstcnt, &status) < 0)
		goto out;
	if (status & TpmStsDataAvail) {
		printf("%s: Data still available\n", __func__);
		goto out;
	}

	ret = current;

out:
	tpm_ready(&tpm->base.chip_ops);
	return ret;
}

static int tpm_send(I2cTpmChipOps *me, const uint8_t *buf, size_t len)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);

	int status;
	size_t burstcnt, limit, sent = 0;
	uint8_t tpm_go[4] = { TpmStsGo };

	if (len > TpmMaxBufSize)
		return -1; // Command is too long for our tpm, sorry.

	status = tpm->base.chip_ops.status(&tpm->base.chip_ops);
	if (!(status & TpmStsCommandReady)) {
		tpm_ready(&tpm->base.chip_ops);
		if (wait_for_burst_status(tpm, TpmStsValid | TpmStsCommandReady,
					  &burstcnt, &status) < 0)
			goto out_err;
	}

	while (len > 0) {
		if (wait_for_burst_status(tpm, TpmStsValid,
					  &burstcnt, &status) < 0)
			goto out_err;
		if (sent > 0 && !(status & TpmStsDataExpect)) {
			printf("%s: Data not expected\n", __func__);
			goto out_err;
		}

		// Use burstcnt-1 to account for address byte
		// inserted by iic_tpm_write()
		limit = MIN(burstcnt - 1, len);
		if (iic_tpm_write(tpm, tpm_data_fifo(tpm->base.locality),
				  &buf[sent], limit) != 0) {
			printf("%s: Write failed\n", __func__);
			goto out_err;
		}

		sent += limit;
		len -= limit;
	}

	if (wait_for_burst_status(tpm, TpmStsValid, &burstcnt, &status) < 0)
		goto out_err;
	if (status & TpmStsDataExpect) {
		printf("%s: Data still expected\n", __func__);
		goto out_err;
	}

	// Start the TPM command
	if (iic_tpm_write(tpm, tpm_sts(tpm->base.locality), tpm_go,
			  sizeof(tpm_go)) < 0) {
		printf("%s: Start command failed\n", __func__);
		goto out_err;
	}
	return sent;

out_err:
	tpm_ready(&tpm->base.chip_ops);
	return -1;
}

static int tpm_init(I2cTpmChipOps *me)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);

	if (request_locality(tpm, 0) != 0)
		return -1;

	// Read four bytes from DID_VID register.
	uint32_t vendor;
	if (iic_tpm_read(tpm, tpm_did_vid(0), (uint8_t *)&vendor, 4) < 0) {
		release_locality(tpm, 0, 1);
		return -1;
	}

	if (vendor == TPM_TIS_I2C_DID_VID_CR50) {
		tpm->chip_type = CR50;
	} else {
		printf("Vendor ID 0x%08x not recognized.\n", vendor);
		release_locality(tpm, 0, 1);
		return -1;
	}

	printf("2.0 TPM (chip type %s device-id 0x%X)\n",
		 chip_name[tpm->chip_type], vendor >> 16);

	return 0;
}

static int tpm_cleanup(I2cTpmChipOps *me)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);
	release_locality(tpm, tpm->base.locality, 1);
	return 0;
}

Cr50I2c *new_cr50_i2c(I2cOps *bus, uint8_t addr)
{
	Cr50I2c *tpm = xmalloc(sizeof(*tpm));
	i2ctpm_fill_in(&tpm->base, bus, addr, TpmStsDataAvail | TpmStsValid,
		       TpmStsDataAvail | TpmStsValid, TpmStsCommandReady);

	tpm->base.chip_ops.init = &tpm_init;
	tpm->base.chip_ops.cleanup = &tpm_cleanup;

	tpm->base.chip_ops.recv = &tpm_recv;
	tpm->base.chip_ops.send = &tpm_send;
	tpm->base.chip_ops.cancel = &tpm_ready;
	tpm->base.chip_ops.status = &tpm_status;

	return tpm;
}
