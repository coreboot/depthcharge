/*
 * Copyright (C) 2011 Infineon Technologies
 *
 * Authors:
 * Peter Huewe <huewe.external@infineon.com>
 *
 * Description:
 * Device driver for TCG/TCPA TPM (trusted platform module).
 * Specifications at www.trustedcomputinggroup.org
 *
 * This device driver implements the TPM interface as defined in
 * the TCG TPM Interface Spec version 1.2, revision 1.0 and the
 * Infineon I2C Protocol Stack Specification v0.20.
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

#include <config.h>
#include <endian.h>
#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/tpm/i2c.h"
#include "drivers/tpm/slb9635_i2c.h"

enum {
	TpmTimeout = 1 // msecs
};

// Expected value for DIDVID register.
enum {
	TPM_TIS_I2C_DID_VID_9635 = 0x000b15d1L,
	TPM_TIS_I2C_DID_VID_9645 = 0x001a15d1L
};

static const char * const chip_name[] = {
	[SLB9635] = "slb9635tt",
	[SLB9645] = "slb9645tt",
	[UNKNOWN] = "unknown/fallback to slb9635",
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
static int iic_tpm_read(Slb9635I2c *tpm, uint8_t addr, uint8_t *buffer,
			size_t len)
{
	int rc;
	int count;

	if (!tpm->base.bus)
		return -1;
	if ((tpm->chip_type == SLB9635) || (tpm->chip_type == UNKNOWN)) {
		// slb9635 protocol should work in both cases.
		for (count = 0; count < 50; count++) {
			rc = i2c_write_raw(tpm->base.bus, tpm->base.addr,
					   &addr, 1);
			if (rc == 0)
				break;

			udelay(200);
		}

		if (rc)
			return -rc;

		/*
		 * After the TPM has successfully received the register address
		 * it needs some time, thus we're sleeping here again, before
		 * retrieving the data
		 */
		for (count = 0; count < 50; count++) {
			udelay(200);
			rc = i2c_read_raw(tpm->base.bus, tpm->base.addr,
					  buffer, len);
			if (rc == 0)
				break;
		}
	} else {
		/*
		 * use a combined read for newer chips
		 * unfortunately the smbus functions are not suitable due to
		 * the 32 byte limit of the smbus.
		 * retries should usually not be needed, but are kept just to
		 * be safe on the safe side.
		 */
		I2cSeg aseg = { .read = 0, .chip = tpm->base.addr,
				.buf = &addr, .len = 1 };
		I2cSeg dseg = { .read = 1, .chip = tpm->base.addr,
				.buf = buffer, .len = len };
		for (count = 0; count < 50; count++) {
			rc = tpm->base.bus->transfer(tpm->base.bus, &aseg, 1) ||
			     tpm->base.bus->transfer(tpm->base.bus, &dseg, 1);
			if (rc == 0)
				break;
			udelay(200);
		}
	}

	// Take care of 'guard time'.
	udelay(60);
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
static int iic_tpm_write(Slb9635I2c *tpm, uint8_t addr, const uint8_t *buffer,
			 size_t len)
{
	int rc = 0;
	int count;

	if (len > MaxTpmBufSize) {
		printf("%s: Length %d is too large\n", __func__, len);
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

		udelay(200);
	}

	// Take care of 'guard time'.
	udelay(60);
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

static int check_locality(Slb9635I2c *tpm, int loc)
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

static void release_locality(Slb9635I2c *tpm, int loc, int force)
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

static int request_locality(Slb9635I2c *tpm, int loc)
{
	uint8_t buf = TpmAccessRequestUse;

	if (check_locality(tpm, loc) >= 0)
		return loc; // We already have the locality.

	iic_tpm_write(tpm, tpm_access(loc), &buf, 1);

	// Wait for burstcount.
	uint64_t start = timer_us(0);
	while (timer_us(start) < 2 * 1000 * 1000) { // Two second timeout.
		if (check_locality(tpm, loc) >= 0)
			return loc;
		mdelay(TpmTimeout);
	}

	return -1;
}

static uint8_t tpm_status(I2cTpmChipOps *me)
{
	Slb9635I2c *tpm = container_of(me, Slb9635I2c, base.chip_ops);
	// NOTE: since i2c read may fail, return 0 in this case --> time-out
	uint8_t buf;
	if (iic_tpm_read(tpm, tpm_sts(tpm->base.locality), &buf, 1) < 0)
		return 0;
	else
		return buf;
}

static void tpm_ready(I2cTpmChipOps *me)
{
	Slb9635I2c *tpm = container_of(me, Slb9635I2c, base.chip_ops);
	// This causes the current command to be aborted.
	uint8_t buf = TpmStsCommandReady;
	iic_tpm_write(tpm, tpm_sts(tpm->base.locality), &buf, 1);
}

static ssize_t get_burstcount(Slb9635I2c *tpm)
{
	ssize_t burstcnt;
	uint8_t buf[3];

	// Wait for burstcount.
	uint64_t start = timer_us(0);
	while (timer_us(start) < 2 * 1000 * 1000) { // Two second timeout.
		// Note: STS is little endian.
		if (iic_tpm_read(tpm, tpm_sts(tpm->base.locality) + 1,
				 buf, 3) < 0)
			burstcnt = 0;
		else
			burstcnt = (buf[2] << 16) + (buf[1] << 8) + buf[0];

		if (burstcnt)
			return burstcnt;
		mdelay(TpmTimeout);
	}
	return -1;
}

static int wait_for_stat(Slb9635I2c *tpm, uint8_t mask, int *status)
{
	uint64_t start = timer_us(0);
	while (timer_us(start) < 2 * 1000 * 1000) { // Two second timeout.
		// Check current status.
		*status = tpm_status(&tpm->base.chip_ops);
		if ((*status & mask) == mask)
			return 0;
		mdelay(TpmTimeout);
	}

	return -1;
}

static int recv_data(Slb9635I2c *tpm, uint8_t *buf, size_t count)
{
	size_t size = 0;
	ssize_t burstcnt;
	int rc;

	while (size < count) {
		burstcnt = get_burstcount(tpm);

		// Burstcount < 0 means the tpm is busy.
		if (burstcnt < 0)
			return burstcnt;

		// Limit received data to max left.
		if (burstcnt > (count - size))
			burstcnt = count - size;

		rc = iic_tpm_read(tpm, tpm_data_fifo(tpm->base.locality),
				  &buf[size], burstcnt);
		if (rc == 0)
			size += burstcnt;

	}
	return size;
}

static int tpm_recv(I2cTpmChipOps *me, uint8_t *buf, size_t count)
{
	Slb9635I2c *tpm = container_of(me, Slb9635I2c, base.chip_ops);

	int size = 0;
	uint32_t expected;
	int status;

	if (count < TPM_HEADER_SIZE) {
		size = -1;
		goto out;
	}

	// Read first 10 bytes, including tag, paramsize, and result.
	size = recv_data(tpm, buf, TPM_HEADER_SIZE);
	if (size < TPM_HEADER_SIZE) {
		printf("tpm_tis_i2c_recv: Unable to read header\n");
		goto out;
	}

	memcpy(&expected, buf + TpmCmdCountOffset, sizeof(expected));
	expected = betohl(expected);
	if ((size_t)expected > count) {
		size = -1;
		goto out;
	}

	size += recv_data(tpm, &buf[TPM_HEADER_SIZE],
			  expected - TPM_HEADER_SIZE);
	if (size < expected) {
		printf("tpm_tis_i2c_recv: Unable to "
			"read remainder of result\n");
		size = -1;
		goto out;
	}

	wait_for_stat(tpm, TpmStsValid, &status);
	if (status & TpmStsDataAvail) { // retry?
		printf("tpm_tis_i2c_recv: Error left over data\n");
		size = -1;
		goto out;
	}

out:
	tpm_ready(&tpm->base.chip_ops);

	return size;
}

static int tpm_send(I2cTpmChipOps *me, const uint8_t *buf, size_t len)
{
	Slb9635I2c *tpm = container_of(me, Slb9635I2c, base.chip_ops);
	int rc, status;
	ssize_t burstcnt;
	size_t count = 0;
	uint8_t sts = TpmStsGo;

	if (len > MaxTpmBufSize)
		return -1; // Command is too long for our tpm, sorry.

	status = tpm->base.chip_ops.status(&tpm->base.chip_ops);
	if ((status & TpmStsCommandReady) == 0) {
		tpm_ready(&tpm->base.chip_ops);
		if (wait_for_stat(tpm, TpmStsCommandReady, &status) < 0) {
			rc = -1;
			goto out_err;
		}
	}

	while (count < len - 1) {
		burstcnt = get_burstcount(tpm);

		// Burstcount < 0 means the tpm is busy.
		if (burstcnt < 0)
			return burstcnt;

		if (burstcnt > (len - 1 - count))
			burstcnt = len - 1 - count;

		rc = iic_tpm_write(tpm, tpm_data_fifo(tpm->base.locality),
				   &(buf[count]), burstcnt);
		if (rc == 0)
			count += burstcnt;

		wait_for_stat(tpm, TpmStsValid, &status);

		if ((status & TpmStsDataExpect) == 0) {
			rc = -1;
			goto out_err;
		}

	}

	// Write last byte.
	iic_tpm_write(tpm, tpm_data_fifo(tpm->base.locality),
		      &buf[count], 1);
	wait_for_stat(tpm, TpmStsValid, &status);
	if ((status & TpmStsDataExpect) != 0) {
		rc = -1;
		goto out_err;
	}

	// Go and do it.
	iic_tpm_write(tpm, tpm_sts(tpm->base.locality), &sts, 1);

	return len;
out_err:
	tpm_ready(&tpm->base.chip_ops);

	return rc;
}

static int tpm_init(I2cTpmChipOps *me)
{
	Slb9635I2c *tpm = container_of(me, Slb9635I2c, base.chip_ops);

	if (request_locality(tpm, 0) != 0)
		return -1;

	// Read four bytes from DID_VID register.
	uint32_t vendor;
	if (iic_tpm_read(tpm, tpm_did_vid(0), (uint8_t *)&vendor, 4) < 0) {
		release_locality(tpm, 0, 1);
		return -1;
	}

	if (vendor == TPM_TIS_I2C_DID_VID_9645) {
		tpm->chip_type = SLB9645;
	} else if (betohl(vendor) == TPM_TIS_I2C_DID_VID_9635) {
		tpm->chip_type = SLB9635;
	} else {
		printf("Vendor ID 0x%08x not recognized.\n", vendor);
		release_locality(tpm, 0, 1);
		return -1;
	}

	printf("1.2 TPM (chip type %s device-id 0x%X)\n",
		 chip_name[tpm->chip_type], vendor >> 16);

	return 0;
}

static int tpm_cleanup(I2cTpmChipOps *me)
{
	Slb9635I2c *tpm = container_of(me, Slb9635I2c, base.chip_ops);
	release_locality(tpm, tpm->base.locality, 1);
	return 0;
}

Slb9635I2c *new_slb9635_i2c(I2cOps *bus, uint8_t addr)
{
	Slb9635I2c *tpm = xmalloc(sizeof(*tpm));
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
