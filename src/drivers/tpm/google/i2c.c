/*
 * Copyright 2016 Google LLC
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
 * Cr50 and Ti50 (Collectively GSC) are TPM 2.0 capable devices
 * that requires special handling for the I2C interface.
 *
 * - Use an interrupt for transaction status instead of hardcoded delays
 * - Must use write+wait+read read protocol
 * - All 4 bytes of status register must be read/written at once
 * - Burst count max is 63 bytes, and burst count behaves
 *   slightly differently than other I2C TPMs
 * - When reading from FIFO the full burstcnt must be read
 *   instead of just reading header and determining the remainder
 */

#include <endian.h>
#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/google/tpm.h"
#include "drivers/tpm/i2c.h"

#define GSC_I2C_DEBUG 0

enum {
	GscTimeoutLong = 2 * 1000 * 1000,	// usecs
	GscTimeoutShort = 2 * 1000,		// usecs
	GscTimeoutNoIrq = 20 * 1000,		// usecs
	GscTimeoutWake = 50,			// usecs
};

enum {
	GscWakeRetries = 3
};

enum {
	CR50_DID_VID = 0x00281ae0L,
	TI50_DID_VID = 0x504a6666L,
};

/*
 * to_tpm_reg() - translates an I2C TPM register to TPM register
 */
static uint16_t to_tpm_reg(uint8_t reg) {
	switch (reg) {

	case 0:
		return 0;	/* TPM Access */
	case 1:
		return 0x18;	/* TPM Status */
	case 5:
		return 0x24;	/* TPM Fifo, variable size. */
	case 6:
		return 0xf00;	/* TPM DID VID */
	case 0xa:
		return 0x14;	/* TPM TPM_INTF_CAPABILITY */
	case 0xe:
		return 0xf04;	/* TPM RID */
	case 0xf:
		return 0xf90;	/* TPM_FW_VER */
	default:
		return 0xffff;
	}
}

/*
 * to_tpm_name() - translates an I2C TPM register to a readable name.
 */
static const char *to_tpm_name(uint8_t reg) {
	switch (reg) {

	case 0:
		return "TPM Access";
	case 1:
		return "TPM Status";
	case 5:
		return "TPM FIFO";
	case 6:
		return "TPM DID VID";
	case 0xa:
		return "TPM TPM_INTF_CAPABILITY";
	case 0xe:
		return "TPM RID";
	case 0xf:
		return "TPM_FW_VER";
	default:
		return "?";
	}
}

/*
 * gsc_i2c_wait_tpm_ready() - wait until TPM is ready for more data
 *
 * Wait for interrupt (or GPIO) to indicate TPM is ready
 *
 * Return -1 on timeout error, 0 on success.
 */
static int gsc_i2c_wait_tpm_ready(GscI2c *tpm)
{
	uint64_t start, timeout;

	if (!tpm->irq_status) {
		// Fixed delay if interrupt not supported
		udelay(GscTimeoutNoIrq);
		return 0;
	}

	timeout = CONFIG_TPM_GOOGLE_IRQ_TIMEOUT_MS;
	start = timer_us(0);

	while (!tpm->irq_status())
		if (timer_us(start) > timeout * USECS_PER_MSEC) {
			printf("%s: Timeout after %llums\n", __func__, timeout);
			return -1;
		}

	return 0;
}

/* Clear pending interrupts */
static void gsc_i2c_clear_tpm_irq(GscI2c *tpm)
{
	if (tpm->irq_status && tpm->irq_status())
		printf("%s: Clearing stale interrupt\n", __func__);
}

/*
 * gsc_i2c_write_raw() - raw write to TPM with retry
 *
 * Return -1 on error, 0 on success.
 */
static int gsc_i2c_write_raw(GscI2c *tpm, uint8_t *data, int len)
{
	int try, ret = -1;

	for (try = 0; try < GscWakeRetries; try++) {
		ret = i2c_write_raw(tpm->base.bus, tpm->base.addr, data, len);
		if (!ret)
			break;
		udelay(GscTimeoutWake);
	}

	return ret;
}

/*
 * gsc_i2c_read() - read from TPM register
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
static int gsc_i2c_read(GscI2c *tpm, uint8_t addr, uint8_t *buffer,
			 size_t len)
{
	if (!tpm->base.bus)
		return -1;

	// Clear interrupt before starting transaction
	gsc_i2c_clear_tpm_irq(tpm);

	if (GSC_I2C_DEBUG) {
		printf("%s: %#02x <= %02x [%#04x: %s][%zu bytes]\n", __func__,
		       tpm->base.addr, addr, to_tpm_reg(addr),
		       to_tpm_name(addr), len);
	}

	// Send the register address byte to the TPM
	if (gsc_i2c_write_raw(tpm, &addr, 1)) {
		printf("%s: Address write failed\n", __func__);
		return -1;
	}

	// Wait for TPM to be ready with response data
	if (gsc_i2c_wait_tpm_ready(tpm)) {
		printf("%s: Waiting for ready interrupt failed\n", __func__ );
		return -1;
	}

	// Read response data from the TPM
	if (i2c_read_raw(tpm->base.bus, tpm->base.addr, buffer, len)) {
		printf("%s: Read response failed\n", __func__);
		return -1;
	}

	if (GSC_I2C_DEBUG) {
		printf("%s: %#02x =>", __func__, tpm->base.addr);
		for (size_t i = 0; i < len; ++i)
			printf(" %02x", buffer[i]);
		printf("\n");
	}

	return 0;
}

/*
 * gsc_i2c_write() - write to TPM register
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
static int gsc_i2c_write(GscI2c *tpm, uint8_t addr, const uint8_t *buffer,
			  size_t len)
{
	if (!tpm->base.bus)
		return -1;

	if (len > GscMaxBufSize) {
		printf("%s: Length %zd is too large\n", __func__, len);
		return -1;
	}

	if (GSC_I2C_DEBUG) {
		printf("%s: %#02x <= %02x [%#04x: %s] +", __func__,
		       tpm->base.addr, addr, to_tpm_reg(addr),
		       to_tpm_name(addr));
		for (size_t i = 0; i < len; ++i)
			printf(" %02x", buffer[i]);
		printf("\n");
	}

	// Prepend the 'register address' to the buffer
	tpm->buf[0] = addr;
	memcpy(tpm->buf + 1, buffer, len);

	// Clear interrupt before starting transaction
	gsc_i2c_clear_tpm_irq(tpm);

	// Send write request buffer with address
	if (gsc_i2c_write_raw(tpm, tpm->buf, len + 1)) {
		printf("%s: Error writing to TPM\n", __func__);
		return -1;
	}

	// Wait for TPM to be ready
	return gsc_i2c_wait_tpm_ready(tpm);
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

static int check_locality(GscI2c *tpm, int loc)
{
	uint8_t mask = TpmAccessValid | TpmAccessActiveLocality;
	uint8_t buf;

	if (gsc_i2c_read(tpm, tpm_access(loc), &buf, 1))
		return -1;

	if ((buf & mask) == mask)
		return loc;
	printf("%s: Failed to check locality\n",__func__);

	return -1;
}

static void release_locality(GscI2c *tpm, int force)
{
	uint8_t mask = TpmAccessValid | TpmAccessRequestPending;
	uint8_t addr = tpm_access(tpm->base.locality);
	uint8_t buf;

	if (gsc_i2c_read(tpm, addr, &buf, 1))
		return;

	if (force || (buf & mask) == mask) {
		buf = TpmAccessActiveLocality;
		gsc_i2c_write(tpm, addr, &buf, 1);
	}

	tpm->base.locality = 0;
}

static int request_locality(GscI2c *tpm, int loc)
{
	uint8_t buf = TpmAccessRequestUse;

	if (check_locality(tpm, loc) == loc)
		return loc;

	if (gsc_i2c_write(tpm, tpm_access(loc), &buf, 1))
		return -1;

	uint64_t start = timer_us(0);
	while (timer_us(start) < GscTimeoutLong) {
		if (check_locality(tpm, loc) == loc) {
			tpm->base.locality = loc;
			return loc;
		}
		udelay(GscTimeoutShort);
	}

	return -1;
}


/*
 * gsc_i2c_tpm_status() - get TPM status
 *
 * Note: This would probably be better returning an int so we can indicate an
 * error code. For now it returns 0 in the case of an error.
 *
 * GSC requires all 4 bytes of status register to be read
 *
 * Returns current TPM status, or 0 on error
 */
static uint8_t gsc_i2c_tpm_status(I2cTpmChipOps *me)
{
	GscI2c *tpm = container_of(me, GscI2c, base.chip_ops);
	uint8_t buf[4];

	if (gsc_i2c_read(tpm, tpm_sts(tpm->base.locality), buf, sizeof(buf))) {
		printf("%s: Read failure, so returning 0 as tpm_status\n",
		       __func__);
		return 0; /* yes, 0 */
	}

	return buf[0];
}

/*
 * gsc_i2c_tpm_ready() - Write TpmStsCommandReady to GSC
 *
 * GSC requires all 4 bytes of status register to be written
 *
 * An error cannot be reported by this function, but it is logged. The caller
 * can check the status itself.
 *
 * @me: base.chip_ops member of GscI2c tpm
 */
static void gsc_i2c_tpm_ready(I2cTpmChipOps *me)
{
	GscI2c *tpm = container_of(me, GscI2c, base.chip_ops);
	uint8_t buf[4] = { TpmStsCommandReady };

	// Ignore any write error, but it is logged.
	if (gsc_i2c_write(tpm, tpm_sts(tpm->base.locality), buf, sizeof(buf)))
		printf("%s: Failed to write TpmStsCommandReady\n", __func__);

	/* Why do we need this delay? gsc_i2c_write already waits for a write
	 * ack interrupt. */
	udelay(GscTimeoutShort);
}

/*
 * gsc_i2c_wait_burst_sts() - Wait for updated burst status
 *
 * @tpm: TPM chip information
 * @mask: Mask for status byte
 * @burst: Returns new burst length
 * @status: Returns new status byte
 * Returns -1 on error, 0 on success.
 */
static int gsc_i2c_wait_burst_sts(GscI2c *tpm, uint8_t mask,
				  size_t *burst, int *status)
{
	uint32_t buf;
	uint64_t start = timer_us(0);

	while (timer_us(start) < GscTimeoutLong) {

		if (gsc_i2c_read(tpm, tpm_sts(tpm->base.locality),
				  (uint8_t *)&buf, sizeof(buf))) {
			udelay(GscTimeoutShort);
			continue;
		}

		*status = buf & 0xff;
		*burst = le16toh((buf >> 8) & 0xffff);

		if ((*status & mask) == mask &&
		    *burst > 0 && *burst <= GscMaxBufSize)
			return 0;

		udelay(GscTimeoutShort);
	}

	printf("%s: Timeout reading burst status\n", __func__);
	return -1;
}

static int gsc_i2c_tpm_recv(I2cTpmChipOps *me, uint8_t *buf, size_t buf_len)
{
	GscI2c *tpm = container_of(me, GscI2c, base.chip_ops);

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

	if (gsc_i2c_wait_burst_sts(tpm, mask, &burstcnt, &status)) {
		printf("%s: First chunk not available\n", __func__);
		goto out_err;
	}

	// Read first chunk of burstcnt bytes
	if (gsc_i2c_read(tpm, addr, buf, burstcnt)) {
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
		if (gsc_i2c_wait_burst_sts(tpm, mask, &burstcnt, &status)) {
			printf("%s: Reading burst status failed\n", __func__ );
			goto out_err;
		}

		len = MIN(burstcnt, expected - current);
		if (gsc_i2c_read(tpm, addr, buf + current, len)) {
			printf("%s: Read failed\n", __func__);
			goto out_err;
		}

		current += len;
	}


	if (gsc_i2c_wait_burst_sts(tpm, TpmStsValid, &burstcnt, &status)) {
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
	 * it if gsc_i2c_tpm_status() fails, since it returns 0 in that case.
	 * We cannot detect that separately from a valid '0' result, so this
	 * is an escape, of some sort.
	 */
	if (gsc_i2c_tpm_status(&tpm->base.chip_ops) & TpmStsCommandReady)
		gsc_i2c_tpm_ready(&tpm->base.chip_ops); // May fail, will log

	return -1;
}

static int gsc_i2c_tpm_send(I2cTpmChipOps *me, const uint8_t *buf, size_t len)
{
	GscI2c *tpm = container_of(me, GscI2c, base.chip_ops);

	int status;
	size_t burstcnt, limit, sent = 0;
	uint8_t tpm_go[4] = { TpmStsGo };

	uint64_t start = timer_us(0);

	/*
	 * Since gsc_i2c_tpm_status() returns 0 on error, this will loop in
	 * that case.
	 */
	while (!(gsc_i2c_tpm_status(&tpm->base.chip_ops) &
		 TpmStsCommandReady)) {

		if (timer_us(start) > GscTimeoutLong) {
			printf("%s: Timed out waiting for ready\n", __func__);
			goto out_err;
		}

		/*
		 * This may fail, but we will presumably detect failure in the
		 * while loop condition abive, and gsc_i2c_tpm_ready() will log
		 * the error. So ignore the error here.
		 */
		gsc_i2c_tpm_ready(&tpm->base.chip_ops);
	}

	while (len > 0) {
		uint8_t mask = TpmStsValid;

		// Wait for data if this is not the first chunk
		if (sent > 0)
			mask |= TpmStsDataExpect;

		if (gsc_i2c_wait_burst_sts(tpm, mask, &burstcnt, &status)) {
			printf("%s: Error reading burst status.\n", __func__ );
			goto out_err;
		}

		// Use burstcnt - 1 to account for the address byte
		// that is inserted by gsc_i2c_write()
		limit = MIN(burstcnt - 1, len);
		if (gsc_i2c_write(tpm, tpm_data_fifo(tpm->base.locality),
				   &buf[sent], limit)) {
			printf("%s: Write failed\n", __func__);
			goto out_err;
		}

		sent += limit;
		len -= limit;
	}

	// Ensure TPM is not expecting more data
	if (gsc_i2c_wait_burst_sts(tpm, TpmStsValid, &burstcnt, &status)) {
		printf("%s: Reading final burst status failed\n", __func__ );
		goto out_err;
	}

	if (status & TpmStsDataExpect) {
		printf("%s: Data still expected\n", __func__);
		goto out_err;
	}

	// Start the TPM command
	if (gsc_i2c_write(tpm, tpm_sts(tpm->base.locality), tpm_go,
			   sizeof(tpm_go))) {
		printf("%s: Start command failed\n", __func__);
		goto out_err;
	}
	return sent;

 out_err:
	/*
	 * Abort current transaction if still pending. This will NOT abort
	 * it if gsc_i2c_tpm_status() fails, since it returns 0 in that case.
	 * We cannot detect that separately from a valid '0' result, so this
	 * is an escape, of some sort.
	 */
	if (gsc_i2c_tpm_status(&tpm->base.chip_ops) & TpmStsCommandReady)
		gsc_i2c_tpm_ready(&tpm->base.chip_ops); // May fail, will log

	return -1;
}

static int tpm_init(I2cTpmChipOps *me)
{
	GscI2c *tpm = container_of(me, GscI2c, base.chip_ops);

	if (request_locality(tpm, 0) != 0)
		return -1;

	// Read four bytes from DID_VID register.
	uint32_t vendor;
	if (gsc_i2c_read(tpm, tpm_did_vid(0), (uint8_t *)&vendor, 4)) {
		release_locality(tpm, 1);
		return -1;
	}

	switch (vendor) {
	case CR50_DID_VID:
		printf("cr50 TPM 2.0 (i2c 0x%02x id 0x%08x)\n",
		       tpm->base.addr, vendor);
		return 0;
	case TI50_DID_VID:
		printf("ti50 TPM 2.0 (i2c 0x%02x id 0x%08x)\n",
		       tpm->base.addr, vendor);
		return 0;
	default:
		printf("Vendor ID 0x%08x not recognized.\n", vendor);
		release_locality(tpm, 1);
		return -1;
	}
}

static int tpm_cleanup(I2cTpmChipOps *me)
{
	GscI2c *tpm = container_of(me, GscI2c, base.chip_ops);
	release_locality(tpm, 1);
	return 0;
}

GscI2c *new_gsc_i2c(I2cOps *bus, uint8_t addr, gsc_irq_status_t irq_status)
{
	GscI2c *tpm = xmalloc(sizeof(*tpm));
	i2ctpm_fill_in(&tpm->base, bus, addr,
		       TpmStsDataAvail | TpmStsValid,
		       TpmStsDataAvail | TpmStsValid, TpmStsCommandReady);

	tpm->irq_status = irq_status;

	tpm->base.chip_ops.init = &tpm_init;
	tpm->base.chip_ops.cleanup = &tpm_cleanup;

	tpm->base.chip_ops.recv = &gsc_i2c_tpm_recv;
	tpm->base.chip_ops.send = &gsc_i2c_tpm_send;
	tpm->base.chip_ops.cancel = &gsc_i2c_tpm_ready;
	tpm->base.chip_ops.status = &gsc_i2c_tpm_status;

	return tpm;
}
