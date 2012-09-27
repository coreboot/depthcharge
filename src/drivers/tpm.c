/*
 * Copyright (c) 2012 The Chromium OS Authors.
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

#include <arch/io.h>
#include <endian.h>
#include <libpayload.h>
#include <vboot_api.h>

#include "drivers/tpm.h"

#ifdef DEBUG
#define	TPM_DEBUG(fmt, args...)		\
	if (TPM_DEBUG_ON) {		\
		printf(PREFIX);		\
		printf(fmt , ##args);	\
	}
#else
#define TPM_DEBUG(fmt, args...) if (0) {}
#endif

#define PREFIX "lpc_tpm: "

/*
 * Structures defined below allow creating descriptions of TPM vendor/device
 * ID information for run time discovery. The only device the system knows
 * about at this time is Infineon slb9635
 */
struct device_name {
	uint16_t dev_id;
	const char * const dev_name;
};

struct vendor_name {
	uint16_t vendor_id;
	const char * vendor_name;
	struct device_name* dev_names;
};

static struct device_name infineon_devices[] = {
	{0xb, "SLB9635 TT 1.2"},
	{0}
};

static const struct vendor_name vendor_names[] = {
	{0x15d1, "Infineon", infineon_devices},
};

/*
 * Cached vendor/device ID pair to indicate that the device has been already
 * discovered
 */
static uint32_t vendor_dev_id;

static int is_byte_reg(uint32_t reg)
{
	/*
	 * These TPM registers are 8 bits wide and as such require byte access
	 * on writes and truncated value on reads.
	 */
	return ((reg == TIS_REG_ACCESS)	||
		(reg == TIS_REG_INT_VECTOR) ||
		(reg == TIS_REG_DATA_FIFO));
}

/* TPM access functions are carved out to make tracing easier. */
uint32_t tpm_read(int locality, uint32_t reg)
{
	uint32_t value;
	/*
	 * Data FIFO register must be read and written in byte access mode,
	 * otherwise the FIFO values are returned 4 bytes at a time.
	 */
	if (is_byte_reg(reg))
		value = readb(TIS_REG(locality, reg));
	else
		value = readl(TIS_REG(locality, reg));

	TPM_DEBUG("Read reg 0x%x returns 0x%x\n", reg, value);
	return value;
}

void tpm_write(uint32_t value, int locality,  uint32_t reg)
{
	TPM_DEBUG("Write reg 0x%x with 0x%x\n", reg, value);

	if (is_byte_reg(reg))
		writeb(value & 0xff, TIS_REG(locality, reg));
	else
		writel(value, TIS_REG(locality, reg));
}

/*
 * tis_wait_reg()
 *
 * Wait for at least a second for a register to change its state to match the
 * expected state. Normally the transition happens within microseconds.
 *
 * @reg - the TPM register offset
 * @locality - locality
 * @mask - bitmask for the bitfield(s) to watch
 * @expected - value the field(s) are supposed to be set to
 *
 * Returns the register contents in case the expected value was found in the
 * appropriate register bits, or TPM_TIMEOUT_ERR on timeout.
 */
uint32_t tis_wait_reg(uint8_t reg, uint8_t locality, uint8_t mask,
		      uint8_t expected)
{
	uint32_t time_us = MAX_DELAY_US;
	while (time_us > 0) {
		uint32_t value = tpm_read(locality, reg);
		if ((value & mask) == expected)
			return value;
		udelay(1); /* 1 us */
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
uint32_t tis_probe(void)
{
	uint32_t didvid = tpm_read(0, TIS_REG_DID_VID);

	if (vendor_dev_id)
		return 0;  /* Already probed. */

	if (!didvid || (didvid == 0xffffffff)) {
		printf("%s: No TPM device found\n", __FUNCTION__);
		return TPM_DRIVER_ERR;
	}

	vendor_dev_id = didvid;

#ifdef DEBUG
	int i;
	const char *device_name = "unknown";
	const char *vendor_name = device_name;
	uint16_t vid = didvid & 0xffff;
	uint16_t did = (didvid >> 16) & 0xffff;
	for (i = 0; i < ARRAY_SIZE(vendor_names); i++) {
		int j = 0;
		uint16_t known_did;
		if (vid == vendor_names[i].vendor_id) {
			vendor_name = vendor_names[i].vendor_name;
		}
		while ((known_did = vendor_names[i].dev_names[j].dev_id) != 0) {
			if (known_did == did) {
				device_name =
					vendor_names[i].dev_names[j].dev_name;
				break;
			}
			j++;
		}
		break;
	}
	/* this will have to be converted into debug printout */
	TPM_DEBUG("Found TPM %s by %s\n", device_name, vendor_name);
#endif
	return 0;
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
uint32_t tis_senddata(const uint8_t *const data, uint32_t len)
{
	uint32_t offset = 0;
	uint16_t burst = 0;
	uint32_t max_cycles = 0;
	uint8_t locality = 0;
	uint32_t value;

	value = tis_wait_reg(TIS_REG_STS, locality, TIS_STS_COMMAND_READY,
			     TIS_STS_COMMAND_READY);
	if (value == TPM_TIMEOUT_ERR) {
		printf("%s:%d - failed to get 'command_ready' status\n",
		       __FILE__, __LINE__);
		return TPM_DRIVER_ERR;
	}
	burst = BURST_COUNT(value);

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
			burst = BURST_COUNT(tpm_read(locality, TIS_REG_STS));
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
			tpm_write(data[offset++], locality, TIS_REG_DATA_FIFO);

		value = tis_wait_reg(TIS_REG_STS, locality,
				     TIS_STS_VALID, TIS_STS_VALID);

		if ((value == TPM_TIMEOUT_ERR) || !(value & TIS_STS_EXPECT)) {
			printf("%s:%d TPM command feed overflow\n",
			       __FILE__, __LINE__);
			return TPM_DRIVER_ERR;
		}

		burst = BURST_COUNT(value);
		if ((offset == (len - 1)) && burst)
			/*
			 * We need to be able to send the last byte to the
			 * device, so burst size must be nonzero before we
			 * break out.
			 */
			break;
	}

	/* Send the last byte. */
	tpm_write(data[offset++], locality, TIS_REG_DATA_FIFO);

	/*
	 * Verify that TPM does not expect any more data as part of this
	 * command.
	 */
	value = tis_wait_reg(TIS_REG_STS, locality,
			     TIS_STS_VALID, TIS_STS_VALID);
	if ((value == TPM_TIMEOUT_ERR) || (value & TIS_STS_EXPECT)) {
		printf("%s:%d unexpected TPM status 0x%x\n",
		       __FILE__, __LINE__, value);
		return TPM_DRIVER_ERR;
	}

	/* OK, sitting pretty, let's start the command execution. */
	tpm_write(TIS_STS_TPM_GO, locality, TIS_REG_STS);

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
uint32_t tis_readresponse(uint8_t *buffer, uint32_t *len)
{
	uint16_t burst_count;
	uint32_t status;
	uint32_t offset = 0;
	uint8_t locality = 0;
	const uint32_t has_data = TIS_STS_DATA_AVAILABLE | TIS_STS_VALID;
	uint32_t expected_count = *len;
	int max_cycles = 0;

	/* Wait for the TPM to process the command */
	status = tis_wait_reg(TIS_REG_STS, locality, has_data, has_data);
	if (status == TPM_TIMEOUT_ERR) {
		printf("%s:%d failed processing command\n",
		       __FILE__, __LINE__);
		return TPM_DRIVER_ERR;
	}

	do {
		while ((burst_count = BURST_COUNT(status)) == 0) {
			if (max_cycles++ == MAX_DELAY_US) {
				printf("%s:%d TPM stuck on read\n",
				       __FILE__, __LINE__);
				return TPM_DRIVER_ERR;
			}
			udelay(1);
			status = tpm_read(locality, TIS_REG_STS);
		}

		max_cycles = 0;

		while (burst_count-- && (offset < expected_count)) {
			buffer[offset++] = (uint8_t) tpm_read(locality,
							 TIS_REG_DATA_FIFO);
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
				expected_count = ntohl(real_length);

				if ((expected_count < offset) ||
				    (expected_count > *len)) {
					printf("%s:%d bad response size %d\n",
					       __FILE__, __LINE__,
					       expected_count);
					return TPM_DRIVER_ERR;
				}
			}
		}

		/* Wait for the next portion */
		status = tis_wait_reg(TIS_REG_STS, locality,
				      TIS_STS_VALID, TIS_STS_VALID);
		if (status == TPM_TIMEOUT_ERR) {
			printf("%s:%d failed to read response\n",
			       __FILE__, __LINE__);
			return TPM_DRIVER_ERR;
		}

		if (offset == expected_count)
			break;	/* We got all we need */

	} while ((status & has_data) == has_data);

	/*
	 * Make sure we indeed read all there was. The TIS_STS_VALID bit is
	 * known to be set.
	 */
	if (status & TIS_STS_DATA_AVAILABLE) {
		printf("%s:%d wrong receive status %x\n",
		       __FILE__, __LINE__, status);
		return TPM_DRIVER_ERR;
	}

	/* Tell the TPM that we are done. */
	tpm_write(TIS_STS_COMMAND_READY, locality, TIS_REG_STS);

	*len = offset;
	return 0;
}

/*
 * tis_sendrecv()
 *
 * Send the requested data to the TPM and then try to get its response
 *
 * @sendbuf - buffer of the data to send
 * @send_size size of the data to send
 * @recvbuf - memory to save the response to
 * @recv_len - pointer to the size of the response buffer
 *
 * Returns 0 on success (and places the number of response bytes at recv_len)
 * or TPM_DRIVER_ERR on failure.
 */
int tis_sendrecv(const uint8_t *sendbuf, size_t send_size,
		 uint8_t *recvbuf, size_t *recv_len)
{
	if (tis_senddata(sendbuf, send_size)) {
		printf("%s:%d failed sending data to TPM\n",
		       __FILE__, __LINE__);
		return TPM_DRIVER_ERR;
	}

	return tis_readresponse(recvbuf, recv_len);
}
