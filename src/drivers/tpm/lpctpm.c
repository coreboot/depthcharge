/*
 * Copyright 2011 Google Inc.
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

/*
 * The code in this file is based on the article "Writing a TPM Device Driver"
 * published on http://ptgmedia.pearsoncmg.com.
 *
 * One principal difference is that in the simplest config the other than 0
 * TPM localities do not get mapped by some devices (for instance, by Infineon
 * slb9635), so this driver provides access to locality 0 only.
 */

#include <arch/io.h>
#include <config.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>

#include "drivers/tpm/tpm.h"

struct tpm_locality {
	uint32_t access;
	uint8_t padding0[4];
	uint32_t int_enable;
	uint8_t vector;
	uint8_t padding1[3];
	uint32_t int_status;
	uint32_t int_capability;
	uint32_t tpm_status;
	uint8_t padding2[8];
	uint8_t data;
	uint8_t padding3[3803];
	uint32_t did_vid;
	uint8_t rid;
	uint8_t padding4[251];
};

/*
 * This pointer refers to the TPM chip, 5 of its localities are mapped as an
 * array.
 */
#define TPM_TOTAL_LOCALITIES	5
static struct tpm_locality *lpc_tpm_dev = (void *)(uintptr_t)0xfed40000;

/* Some registers' bit field definitions */
#define TIS_STS_VALID                  (1 << 7) /* 0x80 */
#define TIS_STS_COMMAND_READY          (1 << 6) /* 0x40 */
#define TIS_STS_TPM_GO                 (1 << 5) /* 0x20 */
#define TIS_STS_DATA_AVAILABLE         (1 << 4) /* 0x10 */
#define TIS_STS_EXPECT                 (1 << 3) /* 0x08 */
#define TIS_STS_RESPONSE_RETRY         (1 << 1) /* 0x02 */

#define TIS_ACCESS_TPM_REG_VALID_STS   (1 << 7) /* 0x80 */
#define TIS_ACCESS_ACTIVE_LOCALITY     (1 << 5) /* 0x20 */
#define TIS_ACCESS_BEEN_SEIZED         (1 << 4) /* 0x10 */
#define TIS_ACCESS_SEIZE               (1 << 3) /* 0x08 */
#define TIS_ACCESS_PENDING_REQUEST     (1 << 2) /* 0x04 */
#define TIS_ACCESS_REQUEST_USE         (1 << 1) /* 0x02 */
#define TIS_ACCESS_TPM_ESTABLISHMENT   (1 << 0) /* 0x01 */

#define TIS_STS_BURST_COUNT_MASK       (0xffff)
#define TIS_STS_BURST_COUNT_SHIFT      (8)

/*
 * Error value returned if a tpm register does not enter the expected state
 * after continuous polling. No actual TPM register reading ever returns -1,
 * so this value is a safe error indication to be mixed with possible status
 * register values.
 */
#define TPM_TIMEOUT_ERR			(-1)

/* Error value returned on various TPM driver errors. */
#define TPM_DRIVER_ERR		(1)

 /* 1 second is plenty for anything TPM does. */
#define MAX_DELAY_US	(1000 * 1000)

/* Retrieve burst count value out of the status register contents. */
static uint16_t burst_count(uint32_t status)
{
	return (status >> TIS_STS_BURST_COUNT_SHIFT) & TIS_STS_BURST_COUNT_MASK;
}

/*
 * Structures defined below allow creating descriptions of TPM vendor/device
 * ID information for run time discovery. The only device the system knows
 * about at this time is Infineon slb9635.
 */
struct device_name {
	uint16_t dev_id;
	const char * const dev_name;
};

struct vendor_name {
	uint16_t vendor_id;
	const char *vendor_name;
	const struct device_name *dev_names;
};

static const struct device_name atmel_devices[] = {
	{0x3204, "AT97SC3204"},
	{0xffff}
};

static const struct device_name infineon_devices[] = {
	{0x000b, "SLB9635 TT 1.2"},
	{0xffff}
};

static const struct device_name nuvoton_devices[] = {
	{0x00fe, "NPCT420AA V2"},
	{0xffff}
};

static const struct device_name stmicro_devices[] = {
	{0x0000, "ST33ZP24" },
	{0xffff}
};

static const struct vendor_name vendor_names[] = {
	{0x1114, "Atmel", atmel_devices},
	{0x15d1, "Infineon", infineon_devices},
	{0x1050, "Nuvoton", nuvoton_devices},
	{0x104a, "ST Microelectronics", stmicro_devices},
};

/*
 * Cached vendor/device ID pair to indicate that the device has been already
 * discovered.
 */
static uint32_t vendor_dev_id;

/* TPM access wrappers to support tracing */
static uint8_t tpm_read_byte(const uint8_t *ptr)
{
	uint8_t ret = readb(ptr);
	if (0)
		printf("lpc_tpm: Read reg 0x%4.4x returns 0x%2.2x\n",
		      (unsigned)((uintptr_t)ptr - (uintptr_t)lpc_tpm_dev), ret);
	return ret;
}

static uint32_t tpm_read_word(const uint32_t *ptr)
{
	uint32_t ret = readl(ptr);
	if (0)
		printf("lpc_tpm: Read reg 0x%4.4x returns 0x%8.8x\n",
			(unsigned)((uintptr_t)ptr - (uintptr_t)lpc_tpm_dev),
			ret);
	return ret;
}

static void tpm_write_byte(uint8_t value, uint8_t *ptr)
{
	if (0)
		printf("lpc_tpm: Write reg 0x%4.4x with 0x%2.2x\n",
			(unsigned)((uintptr_t)ptr - (uintptr_t)lpc_tpm_dev),
			value);
	writeb(value, ptr);
}

static void tpm_write_word(uint32_t value, uint32_t *ptr)
{
	if (0)
		printf("lpc_tpm: Write reg 0x%4.4x with 0x%8.8x\n",
			(unsigned)((uintptr_t)ptr - (uintptr_t)lpc_tpm_dev),
			value);
	writel(value, ptr);
}

/*
 * tis_wait_reg()
 *
 * Wait for at least a second for a register to change its state to match the
 * expected state. Normally the transition happens within microseconds.
 *
 * @reg - pointer to the TPM register
 * @mask - bitmask for the bitfield(s) to watch
 * @expected - value the field(s) are supposed to be set to
 *
 * Returns the register contents in case the expected value was found in the
 * appropriate register bits, or TPM_TIMEOUT_ERR on timeout.
 */
static uint32_t tis_wait_reg(uint32_t *reg, uint8_t mask, uint8_t expected)
{
	uint32_t time_us = MAX_DELAY_US;

	while (time_us > 0) {
		uint32_t value = tpm_read_word(reg);
		if ((value & mask) == expected)
			return value;
		udelay(1);
		time_us--;
	}
	return TPM_TIMEOUT_ERR;
}

/*
 * Probe the TPM device and try determining its manufacturer/device name.
 *
 * Returns 0 on success (the device is found or was found during an earlier
 * invocation) or TPM_DRIVER_ERR if the device is not found.
 */
int tis_init(void)
{
	uint32_t didvid;
	int i;
	const char *device_name = "unknown";
	const char *vendor_name = device_name;
	const struct device_name *dev;
	uint16_t vid, did;

	if (vendor_dev_id)
		return 0;  /* Already probed. */

	didvid = tpm_read_word(&lpc_tpm_dev[0].did_vid);
	if (!didvid || (didvid == 0xffffffff)) {
		printf("%s: No TPM device found\n", __func__);
		return TPM_DRIVER_ERR;
	}

	vendor_dev_id = didvid;

	vid = didvid & 0xffff;
	did = (didvid >> 16) & 0xffff;
	for (i = 0; i < ARRAY_SIZE(vendor_names); i++) {
		int j = 0;
		uint16_t known_did;

		if (vid == vendor_names[i].vendor_id)
			vendor_name = vendor_names[i].vendor_name;
		else
			continue;

		dev = &vendor_names[i].dev_names[j];
		while ((known_did = dev->dev_id) != 0xffff) {
			if (known_did == did) {
				device_name = dev->dev_name;
				break;
			}
			j++;
		}
		break;
	}

	printf("Found TPM %s by %s\n", device_name, vendor_name);
	return 0;
}

/*
 * PC Client Specific TPM Interface Specification section 11.2.12:
 *
 *  Software must be prepared to send two writes of a "1" to command ready
 *  field: the first to indicate successful read of all the data, thus
 *  clearing the data from the ReadFIFO and freeing the TPM's resources,
 *  and the second to indicate to the TPM it is about to send a new command.
 *
 * In practice not all TPMs behave the same so it is necessary to be
 * flexible when trying to set command ready.
 *
 * Returns 0 on success if the TPM is ready for transactions.
 * Returns TPM_TIMEOUT_ERR if the command ready bit does not get set.
 */
static int tis_command_ready(uint8_t locality)
{
	uint32_t status;

	/* 1st attempt to set command ready */
	tpm_write_word(TIS_STS_COMMAND_READY,
		       &lpc_tpm_dev[locality].tpm_status);

	/* Wait for response */
	status = tpm_read_word(&lpc_tpm_dev[locality].tpm_status);

	/* Check if command ready is set yet */
	if (status & TIS_STS_COMMAND_READY)
		return 0;

	/* 2nd attempt to set command ready */
	tpm_write_word(TIS_STS_COMMAND_READY,
		       &lpc_tpm_dev[locality].tpm_status);

	/* Wait for command ready to get set */
	status = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
			      TIS_STS_COMMAND_READY, TIS_STS_COMMAND_READY);

	return (status == TPM_TIMEOUT_ERR) ? TPM_TIMEOUT_ERR : 0;
}

/*
 * tis_senddata()
 *
 * send the passed in data to the TPM device.
 *
 * @data - address of the data to send, byte by byte
 * @len - length of the data to send
 *
 * Returns 0 on success, TPM_DRIVER_ERR on error (in case the device does
 * not accept the entire command).
 */
static uint32_t tis_senddata(const uint8_t * const data, uint32_t len)
{
	uint32_t offset = 0;
	uint16_t burst = 0;
	uint32_t max_cycles = 0;
	uint8_t locality = 0;
	uint32_t value;

	value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
			     TIS_STS_COMMAND_READY, TIS_STS_COMMAND_READY);
	if (value == TPM_TIMEOUT_ERR) {
		printf("%s:%d - failed to get 'command_ready' status\n",
		       __FILE__, __LINE__);
		return TPM_DRIVER_ERR;
	}
	burst = burst_count(value);

	while (1) {
		unsigned count;

		/* Wait till the device is ready to accept more data. */
		while (!burst) {
			if (max_cycles++ == MAX_DELAY_US) {
				printf("%s:%d failed to feed %d bytes of %d\n",
				       __FILE__, __LINE__, len - offset, len);
				return TPM_DRIVER_ERR;
			}
			udelay(1);
			burst = burst_count(tpm_read_word(&lpc_tpm_dev
						     [locality].tpm_status));
		}

		max_cycles = 0;

		/*
		 * Calculate number of bytes the TPM is ready to accept in one
		 * shot.
		 *
		 * We want to send the last byte outside of the loop (hence
		 * the -1 below) to make sure that the 'expected' status bit
		 * changes to zero exactly after the last byte is fed into the
		 * FIFO.
		 */
		count = MIN(burst, len - offset - 1);
		while (count--)
			tpm_write_byte(data[offset++],
				  &lpc_tpm_dev[locality].data);

		value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
				     TIS_STS_VALID, TIS_STS_VALID);

		if ((value == TPM_TIMEOUT_ERR) || !(value & TIS_STS_EXPECT)) {
			printf("%s:%d TPM command feed overflow\n",
			       __FILE__, __LINE__);
			return TPM_DRIVER_ERR;
		}

		burst = burst_count(value);
		if ((offset == (len - 1)) && burst) {
			/*
			 * We need to be able to send the last byte to the
			 * device, so burst size must be nonzero before we
			 * break out.
			 */
			break;
		}
	}

	/* Send the last byte. */
	tpm_write_byte(data[offset++], &lpc_tpm_dev[locality].data);
	/*
	 * Verify that TPM does not expect any more data as part of this
	 * command.
	 */
	value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
			     TIS_STS_VALID, TIS_STS_VALID);
	if ((value == TPM_TIMEOUT_ERR) || (value & TIS_STS_EXPECT)) {
		printf("%s:%d unexpected TPM status 0x%x\n",
		       __FILE__, __LINE__, value);
		return TPM_DRIVER_ERR;
	}

	/* OK, sitting pretty, let's start the command execution. */
	tpm_write_word(TIS_STS_TPM_GO, &lpc_tpm_dev[locality].tpm_status);
	return 0;
}

/*
 * tis_readresponse()
 *
 * read the TPM device response after a command was issued.
 *
 * @buffer - address where to read the response, byte by byte.
 * @len - pointer to the size of buffer
 *
 * On success stores the number of received bytes to len and returns 0. On
 * errors (misformatted TPM data or synchronization problems) returns
 * TPM_DRIVER_ERR.
 */
static uint32_t tis_readresponse(uint8_t *buffer, uint32_t *len)
{
	uint16_t burst;
	uint32_t value;
	uint32_t offset = 0;
	uint8_t locality = 0;
	const uint32_t has_data = TIS_STS_DATA_AVAILABLE | TIS_STS_VALID;
	uint32_t expected_count = *len;
	int max_cycles = 0;

	/* Wait for the TPM to process the command. */
	value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
			      has_data, has_data);
	if (value == TPM_TIMEOUT_ERR) {
		printf("%s:%d failed processing command\n",
		       __FILE__, __LINE__);
		return TPM_DRIVER_ERR;
	}

	do {
		while ((burst = burst_count(value)) == 0) {
			if (max_cycles++ == MAX_DELAY_US) {
				printf("%s:%d TPM stuck on read\n",
				       __FILE__, __LINE__);
				return TPM_DRIVER_ERR;
			}
			udelay(1);
			value = tpm_read_word(&lpc_tpm_dev
					      [locality].tpm_status);
		}

		max_cycles = 0;

		while (burst-- && (offset < expected_count)) {
			buffer[offset++] = tpm_read_byte(&lpc_tpm_dev
							 [locality].data);

			if (offset == 6) {
				/*
				 * We got the first six bytes of the reply,
				 * let's figure out how many bytes to expect
				 * total - it is stored as a 4 byte number in
				 * network order, starting with offset 2 into
				 * the body of the reply.
				 */
				uint32_t real_length;
				memcpy(&real_length,
				       buffer + 2,
				       sizeof(real_length));
				expected_count = betohl(real_length);

				if ((expected_count < offset) ||
				    (expected_count > *len)) {
					printf("%s:%d bad response size %d\n",
					       __FILE__, __LINE__,
					       expected_count);
					return TPM_DRIVER_ERR;
				}
			}
		}

		/* Wait for the next portion. */
		value = tis_wait_reg(&lpc_tpm_dev[locality].tpm_status,
				     TIS_STS_VALID, TIS_STS_VALID);
		if (value == TPM_TIMEOUT_ERR) {
			printf("%s:%d failed to read response\n",
			       __FILE__, __LINE__);
			return TPM_DRIVER_ERR;
		}

		if (offset == expected_count)
			break;	/* We got all we needed. */

	} while ((value & has_data) == has_data);

	/*
	 * Make sure we indeed read all there was. The TIS_STS_VALID bit is
	 * known to be set.
	 */
	if (value & TIS_STS_DATA_AVAILABLE) {
		printf("%s:%d wrong receive status %x\n",
		       __FILE__, __LINE__, value);
		return TPM_DRIVER_ERR;
	}

	/* Tell the TPM that we are done. */
	if (tis_command_ready(locality) == TPM_TIMEOUT_ERR)
		return TPM_DRIVER_ERR;

	*len = offset;
	return 0;
}

int tis_open(void)
{
	uint8_t locality = 0; /* we use locality zero for everything. */

	if (tis_close())
		return TPM_DRIVER_ERR;

	/* now request access to locality. */
	tpm_write_word(TIS_ACCESS_REQUEST_USE, &lpc_tpm_dev[locality].access);

	/* did we get a lock? */
	if (tis_wait_reg(&lpc_tpm_dev[locality].access,
			 TIS_ACCESS_ACTIVE_LOCALITY,
			 TIS_ACCESS_ACTIVE_LOCALITY) == TPM_TIMEOUT_ERR) {
		printf("%s:%d - failed to lock locality %d\n",
		       __FILE__, __LINE__, locality);
		return TPM_DRIVER_ERR;
	}

	/* Certain TPMs need some delay here or they hang. */
	udelay(10);

	if (tis_command_ready(locality) == TPM_TIMEOUT_ERR)
		return TPM_DRIVER_ERR;

	return 0;
}

int tis_close(void)
{
	uint8_t locality = 0;

	if (tpm_read_word(&lpc_tpm_dev[locality].access) &
	    TIS_ACCESS_ACTIVE_LOCALITY) {
		tpm_write_word(TIS_ACCESS_ACTIVE_LOCALITY,
			       &lpc_tpm_dev[locality].access);

		if (tis_wait_reg(&lpc_tpm_dev[locality].access,
				 TIS_ACCESS_ACTIVE_LOCALITY, 0) ==
		    TPM_TIMEOUT_ERR) {
			printf("%s:%d - failed to release locality %d\n",
			       __FILE__, __LINE__, locality);
			return TPM_DRIVER_ERR;
		}
	}
	return 0;
}

int tis_sendrecv(const uint8_t *sendbuf, size_t send_size,
		 uint8_t *recvbuf, size_t *recv_len)
{
	if (tis_senddata(sendbuf, send_size)) {
		printf("%s:%d failed sending data to TPM\n",
		       __FILE__, __LINE__);
		return TPM_DRIVER_ERR;
	}

	return tis_readresponse(recvbuf, (uint32_t *)recv_len);
}
