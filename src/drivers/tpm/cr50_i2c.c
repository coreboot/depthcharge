/*
 * Copyright 2016 Google Inc.
 *
 * Based on Linux Kernel TPM driver by
 * Peter Huewe <peter.huewe@infineon.com>
 * Copyright (C) 2011 Infineon Technologies
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

/*
 * cr50 is a TPM 2.0 capable device that requires special
 * handling for the I2C interface.
 *
 * - Use an interrupt for transaction status instead of hardcoded delays
 * - Must use write+wait+read read protocol
 * - All 4 bytes of status register must be read/written at once
 * - Burst count max is 63 bytes, and burst count behaves
 *   slightly differently than other I2C TPMs
 * - When reading from FIFO the full burstcnt must be read
 *   instead of just reading header and determining the remainder
 */

#include <config.h>
#include <endian.h>
#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/tpm/i2c.h"
#include "drivers/tpm/cr50_i2c.h"

enum {
	Cr50TimeoutLong = 2 * 1000 * 1000,	// usecs
	Cr50TimeoutShort = 2 * 1000,		// usecs
	Cr50TimeoutNoIrq = 20 * 1000,		// usecs
	Cr50TimeoutIrq = 100 * 1000,		// usecs
};

enum {
	CR50_DID_VID = 0x00281ae0L
};

/*
 * cr50_i2c_wait_tpm_ready() - wait until TPM is ready for more data
 *
 * Wait for interrupt (or GPIO) to indicate TPM is ready
 *
 * Return -1 on timeout error, 0 on success.
 */
static int cr50_i2c_wait_tpm_ready(Cr50I2c *tpm)
{
	uint64_t start, timeout;

	if (!tpm->irq_status) {
		// Fixed delay if interrupt not supported
		udelay(Cr50TimeoutNoIrq);
		return 0;
	}

	timeout = Cr50TimeoutIrq;
	start = timer_us(0);

	while (!tpm->irq_status())
		if (timer_us(start) > timeout) {
			printf("%s: Timeout waiting for ready", __func__);
			return -1;
		}

	return 0;
}

/* Clear pending interrupts */
static void cr50_i2c_clear_tpm_irq(Cr50I2c *tpm)
{
	if (tpm->irq_status && tpm->irq_status())
		printf("%s: Clearing stale interrupt\n", __func__);
}

/*
 * cr50_i2c_read() - read from TPM register
 *
 * @tpm: TPM chip information
 * @addr: register address to read from
 * @buffer: provided by caller
 * @len: number of bytes to read
 *
 * 1) send register address byte 'addr' to the TPM
 * 2) wait for TPM to indicate it is ready
 * 3) read 'len' bytes of TPM response into the provided 'buffer'
 *
 * Return -1 on error, 0 on success.
 */
static int cr50_i2c_read(Cr50I2c *tpm, uint8_t addr, uint8_t *buffer,
			 size_t len)
{
	if (!tpm->base.bus)
		return -1;

	// Clear interrupt before starting transaction
	cr50_i2c_clear_tpm_irq(tpm);

	// Send the register address byte to the TPM
	if (i2c_write_raw(tpm->base.bus, tpm->base.addr, &addr, 1)) {
		printf("%s: Address write failed\n", __func__);
		return -1;
	}

	// Wait for TPM to be ready with response data
	if (cr50_i2c_wait_tpm_ready(tpm)) {
		printf("%s: Waiting for ready interrupt failed\n", __func__ );
		return -1;
	}

	// Read response data from the TPM
	if (i2c_read_raw(tpm->base.bus, tpm->base.addr, buffer, len)) {
		printf("%s: Read response failed\n", __func__);
		return -1;
	}

	return 0;
}

/*
 * cr50_i2c_write() - write to TPM register
 *
 * @tpm: TPM chip information
 * @addr: register address to write to
 * @buffer: data to write
 * @len: number of bytes to write
 *
 * 1) prepend the provided address to the provided data
 * 2) send the address+data to the TPM
 * 3) wait for TPM to indicate it is done writing
 *
 * Returns -1 on error, 0 on success.
 */
static int cr50_i2c_write(Cr50I2c *tpm, uint8_t addr, const uint8_t *buffer,
			  size_t len)
{
	if (!tpm->base.bus)
		return -1;

	if (len > Cr50MaxBufSize) {
		printf("%s: Length %zd is too large\n", __func__, len);
		return -1;
	}

	// Prepend the 'register address' to the buffer
	tpm->buf[0] = addr;
	memcpy(tpm->buf + 1, buffer, len);

	// Clear interrupt before starting transaction
	cr50_i2c_clear_tpm_irq(tpm);

	// Send write request buffer with address
	if (i2c_write_raw(tpm->base.bus, tpm->base.addr, tpm->buf, len + 1)) {
		printf("%s: Error writing to TPM\n", __func__);
		return -1;
	}

	// Wait for TPM to be ready
	return cr50_i2c_wait_tpm_ready(tpm);
}

#define TPM_HEADER_SIZE 10

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
	uint8_t mask = TpmAccessValid | TpmAccessActiveLocality;
	uint8_t buf;

	if (cr50_i2c_read(tpm, tpm_access(loc), &buf, 1))
		return -1;

	if ((buf & mask) == mask)
		return loc;
	printf("%s: Failed to check locality\n",__func__);

	return -1;
}

static void release_locality(Cr50I2c *tpm, int force)
{
	uint8_t mask = TpmAccessValid | TpmAccessRequestPending;
	uint8_t addr = tpm_access(tpm->base.locality);
	uint8_t buf;

	if (cr50_i2c_read(tpm, addr, &buf, 1))
		return;

	if (force || (buf & mask) == mask) {
		buf = TpmAccessActiveLocality;
		cr50_i2c_write(tpm, addr, &buf, 1);
	}

	tpm->base.locality = 0;
}

static int request_locality(Cr50I2c *tpm, int loc)
{
	uint8_t buf = TpmAccessRequestUse;

	if (check_locality(tpm, loc) == loc)
		return loc;

	if (cr50_i2c_write(tpm, tpm_access(loc), &buf, 1))
		return -1;

	uint64_t start = timer_us(0);
	while (timer_us(start) < Cr50TimeoutLong) {
		if (check_locality(tpm, loc) == loc) {
			tpm->base.locality = loc;
			return loc;
		}
		udelay(Cr50TimeoutShort);
	}

	return -1;
}


/*
 * cr50_i2c_tpm_status() - get TPM status
 *
 * Note: This would probably be better returning an int so we can indicate an
 * error code. For now it returns 0 in the case of an error.
 *
 * cr50 requires all 4 bytes of status register to be read
 *
 * Returns current TPM status, or 0 on error
 */
static uint8_t cr50_i2c_tpm_status(I2cTpmChipOps *me)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);
	uint8_t buf[4];

	if (cr50_i2c_read(tpm, tpm_sts(tpm->base.locality), buf, sizeof(buf))) {
		printf("%s: Read failure, so returning 0 as tpm_status\n",
		       __func__);
		return 0; /* yes, 0 */
	}

	return buf[0];
}

/*
 * cr50_i2c_tpm_ready() - Write TpmStsCommandReady to cr50
 *
 * cr50 requires all 4 bytes of status register to be written
 *
 * An error cannot be reported by this function, but it is logged. The caller
 * can check the status itself.
 *
 * @me: base.chip_ops member of Cr50I2c tpm
 */
static void cr50_i2c_tpm_ready(I2cTpmChipOps *me)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);
	uint8_t buf[4] = { TpmStsCommandReady };

	// Ignore any write error, but it is logged.
	cr50_i2c_write(tpm, tpm_sts(tpm->base.locality), buf, sizeof(buf));
	udelay(Cr50TimeoutShort);
	printf("%s: Failed to write TpmStsCommandReady\n", __func__);
}

/*
 * cr50_i2c_wait_burststs() - Wait for updated burst status
 *
 * @tpm: TPM chip information
 * @mask: Mask for status byte
 * @burst: Returns new burst length
 * @status: Returns new status byte
 * Returns -1 on error, 0 on success.
 */
static int cr50_i2c_wait_burststs(Cr50I2c *tpm, uint8_t mask,
				  size_t *burst, int *status)
{
	uint32_t buf;
	uint64_t start = timer_us(0);

	while (timer_us(start) < Cr50TimeoutLong) {

		if (cr50_i2c_read(tpm, tpm_sts(tpm->base.locality),
				  (uint8_t *)&buf, sizeof(buf))) {
			udelay(Cr50TimeoutShort);
			continue;
		}

		*status = buf & 0xff;
		*burst = le16toh((buf >> 8) & 0xffff);

		if ((*status & mask) == mask &&
		    *burst > 0 && *burst <= Cr50MaxBufSize)
			return 0;

		udelay(Cr50TimeoutShort);
	}

	printf("%s: Timeout reading burst status\n", __func__);
	return -1;
}

static int cr50_i2c_tpm_recv(I2cTpmChipOps *me, uint8_t *buf, size_t buf_len)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);

	int status;
	uint32_t expected_buf;
	size_t burstcnt, expected, current, len;
	uint8_t addr = tpm_data_fifo(tpm->base.locality);
	uint8_t mask = TpmStsValid | TpmStsDataAvail;

	if (buf_len < TPM_HEADER_SIZE) {
		printf("%s: INTERNAL ERROR: buffer %zu too small\n", __func__,
		       buf_len);
		return -1;
	}

	if (cr50_i2c_wait_burststs(tpm, mask, &burstcnt, &status)) {
		printf("%s: First chunk not available\n", __func__);
		goto out_err;
	}

	// Read first chunk of burstcnt bytes
	if (cr50_i2c_read(tpm, addr, buf, burstcnt)) {
		printf("%s: Read failed\n", __func__);
		goto out_err;
	}

	// Determine expected data in the return buffer
	memcpy(&expected_buf, buf + TpmCmdCountOffset, sizeof(expected_buf));
	expected = betohl(expected_buf);
	if (expected > buf_len) {
		printf("%s: Too much data: %zu > %zu\n", __func__,
		       expected, buf_len);
		goto out_err;
	}

	// Now read the rest of the data
	current = burstcnt;
	while (current < expected) {
		// Read updated burst count and check status
		if (cr50_i2c_wait_burststs(tpm, mask, &burstcnt, &status)) {
			printf("%s: Reading burst status failed\n", __func__ );
			goto out_err;
		}

		len = MIN(burstcnt, expected - current);
		if (cr50_i2c_read(tpm, addr, buf + current, len)) {
			printf("%s: Read failed\n", __func__);
			goto out_err;
		}

		current += len;
	}


	if (cr50_i2c_wait_burststs(tpm, TpmStsValid, &burstcnt, &status)) {
		printf("%s: Reading final burst status failed\n", __func__ );
		goto out_err;
	}

	if (status & TpmStsDataAvail) {
		printf("%s: Data still available\n", __func__);
		goto out_err;
	}

	return current;

out_err:
	/*
	 * Abort current transaction if still pending. This will NOT abort
	 * it if cr50_i2c_tpm_status() fails, since it returns 0 in that case.
	 * We cannot detect that separately from a valid '0' result, so this
	 * is an escape, of some sort.
	 */
	if (cr50_i2c_tpm_status(&tpm->base.chip_ops) & TpmStsCommandReady)
		cr50_i2c_tpm_ready(&tpm->base.chip_ops); // May fail, will log

	return -1;
}

static int cr50_i2c_tpm_send(I2cTpmChipOps *me, const uint8_t *buf, size_t len)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);

	int status;
	size_t burstcnt, limit, sent = 0;
	uint8_t tpm_go[4] = { TpmStsGo };

	uint64_t start = timer_us(0);

	/*
	 * Since cr50_i2c_tpm_status() returns 0 on error, this will loop in
	 * that case.
	 */
	while (!(cr50_i2c_tpm_status(&tpm->base.chip_ops) &
		 TpmStsCommandReady)) {

		if (timer_us(start) > Cr50TimeoutLong) {
			printf("%s: Timed out waiting for ready\n", __func__);
			goto out_err;
		}

		/*
		 * This may fail, but we will presumably detect failure in the
		 * while loop condition abive, and cr50_i2c_tpm_ready() will log
		 * the error. So ignore the error here.
		 */
		cr50_i2c_tpm_ready(&tpm->base.chip_ops);
	}

	while (len > 0) {
		uint8_t mask = TpmStsValid;

		// Wait for data if this is not the first chunk
		if (sent > 0)
			mask |= TpmStsDataExpect;

		if (cr50_i2c_wait_burststs(tpm, mask, &burstcnt, &status)) {
			printf("%s: Error reading burst status.\n", __func__ );
			goto out_err;
		}

		// Use burstcnt - 1 to account for the address byte
		// that is inserted by cr50_i2c_write()
		limit = MIN(burstcnt - 1, len);
		if (cr50_i2c_write(tpm, tpm_data_fifo(tpm->base.locality),
				   &buf[sent], limit)) {
			printf("%s: Write failed\n", __func__);
			goto out_err;
		}

		sent += limit;
		len -= limit;
	}

	// Ensure TPM is not expecting more data
	if (cr50_i2c_wait_burststs(tpm, TpmStsValid, &burstcnt, &status)) {
		printf("%s: Reading final burst status failed\n", __func__ );
		goto out_err;
	}

	if (status & TpmStsDataExpect) {
		printf("%s: Data still expected\n", __func__);
		goto out_err;
	}

	// Start the TPM command
	if (cr50_i2c_write(tpm, tpm_sts(tpm->base.locality), tpm_go,
			   sizeof(tpm_go))) {
		printf("%s: Start command failed\n", __func__);
		goto out_err;
	}
	return sent;

out_err:
	/*
	 * Abort current transaction if still pending. This will NOT abort
	 * it if cr50_i2c_tpm_status() fails, since it returns 0 in that case.
	 * We cannot detect that separately from a valid '0' result, so this
	 * is an escape, of some sort.
	 */
	if (cr50_i2c_tpm_status(&tpm->base.chip_ops) & TpmStsCommandReady)
		cr50_i2c_tpm_ready(&tpm->base.chip_ops); // May fail, will log

	return -1;
}

static int tpm_init(I2cTpmChipOps *me)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);

	if (request_locality(tpm, 0) != 0)
		return -1;

	// Read four bytes from DID_VID register.
	uint32_t vendor;
	if (cr50_i2c_read(tpm, tpm_did_vid(0), (uint8_t *)&vendor, 4)) {
		release_locality(tpm, 1);
		return -1;
	}

	if (vendor != CR50_DID_VID) {
		printf("Vendor ID 0x%08x not recognized.\n", vendor);
		release_locality(tpm, 1);
		return -1;
	}

	printf("cr50 TPM 2.0 (i2c 0x%02x id 0x%x)\n",
	       tpm->base.addr, vendor >> 16);

	return 0;
}

static int tpm_cleanup(I2cTpmChipOps *me)
{
	Cr50I2c *tpm = container_of(me, Cr50I2c, base.chip_ops);
	release_locality(tpm, 1);
	return 0;
}

Cr50I2c *new_cr50_i2c(I2cOps *bus, uint8_t addr, cr50_irq_status_t irq_status)
{
	Cr50I2c *tpm = xmalloc(sizeof(*tpm));
	i2ctpm_fill_in(&tpm->base, bus, addr,
		       TpmStsDataAvail | TpmStsValid,
		       TpmStsDataAvail | TpmStsValid, TpmStsCommandReady);

	tpm->irq_status = irq_status;

	tpm->base.chip_ops.init = &tpm_init;
	tpm->base.chip_ops.cleanup = &tpm_cleanup;

	tpm->base.chip_ops.recv = &cr50_i2c_tpm_recv;
	tpm->base.chip_ops.send = &cr50_i2c_tpm_send;
	tpm->base.chip_ops.cancel = &cr50_i2c_tpm_ready;
	tpm->base.chip_ops.status = &cr50_i2c_tpm_status;

	return tpm;
}
