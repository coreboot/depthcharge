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

#include "base/cleanup_funcs.h"
#include "drivers/tpm/lpc.h"

typedef struct {
	uint8_t access;
	uint8_t padding0[7];
	uint32_t int_enable;
	uint8_t vector;
	uint8_t padding1[3];
	uint32_t int_status;
	uint32_t int_capability;
	uint8_t tpm_status;
	uint8_t burst_count[2];
	uint8_t padding2[9];
	uint8_t data;
	uint8_t padding3[3803];
	uint32_t did_vid;
	uint8_t rid;
	uint8_t padding4[251];
} TpmLocality;

typedef struct TpmRegs
{
	TpmLocality localities[5];
} TpmRegs;

enum {
	TisStsValid = 0x80,
	TisStsCommandReady = 0x40,
	TisStsTpmGo = 0x20,
	TisStsDataAvailable = 0x10,
	TisStsExpect = 0x08,
	TisStsResponseRetry = 0x02
};

enum {
	TisAccessTpmRegValidSts = 0x80,
	TisAccessActiveLocality = 0x20,
	TisAccessBeenSeized = 0x10,
	TisAccessSeize = 0x08,
	TisAccessPendingRequest = 0x04,
	TisAccessRequestUse = 0x02,
	TisAccessTpmEstablishment = 0x01
};

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

// Retrieve burst count value.
static uint16_t burst_count(TpmLocality *dev)
{
	uint16_t value;
	value = readb(&dev->burst_count[0]);
	value |= readb(&dev->burst_count[1]) << 8;
	return value;
}

/*
 * lpctpm_wait_reg()
 *
 * Wait for at least a second for a register to change its state to match the
 * expected state. Normally the transition happens within microseconds.
 *
 * @reg - pointer to the TPM register
 * @mask - bitmask for the bitfield(s) to watch
 * @expected - value the field(s) are supposed to be set to
 *
 * Returns 0 on success or -1 on timeout.
 */
static int lpctpm_wait_reg(uint8_t *reg, uint8_t mask, uint8_t expected)
{
	uint64_t start = timer_us(0);
	do {
		if ((readb(reg) & mask) == expected)
			return 0;
	} while (timer_us(start) < 1000 * 1000);
	return -1;
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
 * @tpm - pointer to the TPM structure
 *
 * Returns 0 on success if the TPM is ready for transactions.
 * Returns -1 if the command ready bit does not get set.
 */
static int lpctpm_command_ready(LpcTpm *tpm)
{
	uint8_t *status = &tpm->regs->localities[0].tpm_status;

	// 1st attempt to set command ready.
	writeb(TisStsCommandReady, status);

	// Check if command ready is set yet.
	if (readb(status) & TisStsCommandReady)
		return 0;

	// 2nd attempt to set command ready.
	writeb(TisStsCommandReady, status);

	// Wait for command ready to get set.
	return lpctpm_wait_reg(status, TisStsCommandReady, TisStsCommandReady);
}

/*
 * lpctpm_senddata()
 *
 * send the passed in data to the TPM device.
 *
 * @tpm - pointer to the TPM structure
 * @data - address of the data to send, byte by byte
 * @len - length of the data to send
 *
 * Returns 0 on success, -1 on error (in case the device does not accept the
 * entire command).
 */
static uint32_t lpctpm_senddata(LpcTpm *tpm, const uint8_t * const data,
				uint32_t len)
{
	uint32_t offset = 0;

	if (lpctpm_wait_reg(&tpm->regs->localities[0].tpm_status,
			    TisStsCommandReady, TisStsCommandReady)) {
		printf("%s:%d - failed to get 'command_ready' status\n",
		       __FILE__, __LINE__);
		return -1;
	}
	uint16_t burst = burst_count(&tpm->regs->localities[0]);

	while (1) {
		// Wait till the device is ready to accept more data.
		uint64_t start = timer_us(0);
		while (!burst) {
			if (timer_us(start) > 1000 * 1000) {
				printf("%s:%d failed to feed %d bytes of %d.\n",
				       __FILE__, __LINE__, len - offset, len);
				return -1;
			}
			burst = burst_count(&tpm->regs->localities[0]);
		}

		/*
		 * Calculate number of bytes the TPM is ready to accept in one
		 * shot.
		 *
		 * We want to send the last byte outside of the loop (hence
		 * the -1 below) to make sure that the 'expected' status bit
		 * changes to zero exactly after the last byte is fed into the
		 * FIFO.
		 */
		unsigned count = MIN(burst, len - offset - 1);
		while (count--)
			writeb(data[offset++], &tpm->regs->localities[0].data);

		if (lpctpm_wait_reg(&tpm->regs->localities[0].tpm_status,
				    TisStsValid, TisStsValid) ||
		    !(readb(&tpm->regs->localities[0].tpm_status) &
		      TisStsExpect)) {
			printf("%s:%d TPM command feed overflow\n",
			       __FILE__, __LINE__);
			return -1;
		}

		burst = burst_count(&tpm->regs->localities[0]);
		if ((offset == (len - 1)) && burst) {
			/*
			 * We need to be able to send the last byte to the
			 * device, so burst size must be nonzero before we
			 * break out.
			 */
			break;
		}
	}

	// Send the last byte.
	writeb(data[offset++], &tpm->regs->localities[0].data);
	/*
	 * Verify that TPM does not expect any more data as part of this
	 * command.
	 */
	if (lpctpm_wait_reg(&tpm->regs->localities[0].tpm_status,
			    TisStsValid, TisStsValid) ||
	    (readb(&tpm->regs->localities[0].tpm_status) & TisStsExpect)) {
		printf("%s:%d unexpected TPM status 0x%x\n",
		       __FILE__, __LINE__,
		       readb(&tpm->regs->localities[0].tpm_status));
		return -1;
	}

	// OK, sitting pretty, let's start the command execution.
	writeb(TisStsTpmGo, &tpm->regs->localities[0].tpm_status);
	return 0;
}

/*
 * lpctpm_readresponse()
 *
 * read the TPM device response after a command was issued.
 *
 * @tpm - pointer to the TPM structure
 * @buffer - address where to read the response, byte by byte.
 * @len - pointer to the size of buffer
 *
 * On success stores the number of received bytes to len and returns 0. On
 * errors (misformatted TPM data or synchronization problems) returns -1.
 */
static uint32_t lpctpm_readresponse(LpcTpm *tpm, uint8_t *buffer, size_t *len)
{
	uint16_t burst;
	uint32_t offset = 0;
	const uint32_t has_data = TisStsDataAvailable | TisStsValid;
	uint32_t expected_count = *len;

	// Wait for the TPM to process the command.
	if (lpctpm_wait_reg(&tpm->regs->localities[0].tpm_status,
			      has_data, has_data)) {
		printf("%s:%d failed processing command\n", __FILE__, __LINE__);
		return -1;
	}

	do {
		uint64_t start = timer_us(0);
		while ((burst = burst_count(&tpm->regs->localities[0])) == 0) {
			if (timer_us(start) > 1000 * 1000) {
				printf("%s:%d TPM stuck on read\n",
				       __FILE__, __LINE__);
				return -1;
			}
			udelay(1);
		}

		while (burst-- && (offset < expected_count)) {
			buffer[offset++] =
				readb(&tpm->regs->localities[0].data);

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
					return -1;
				}
			}
		}

		// Wait for the next portion.
		if (lpctpm_wait_reg(&tpm->regs->localities[0].tpm_status,
				     TisStsValid, TisStsValid)) {
			printf("%s:%d failed to read response\n",
			       __FILE__, __LINE__);
			return -1;
		}

		if (offset == expected_count)
			break;	// We got all we needed.

	} while ((readb(&tpm->regs->localities[0].tpm_status) & has_data)
	         == has_data);

	/*
	 * Make sure we indeed read all there was. The TisStsValid bit is
	 * known to be set.
	 */
	if (readb(&tpm->regs->localities[0].tpm_status) & TisStsDataAvailable) {
		printf("%s:%d wrong receive status %x\n",
		       __FILE__, __LINE__,
		       readb(&tpm->regs->localities[0].tpm_status));
		return -1;
	}

	// Tell the TPM that we are done.
	if (lpctpm_command_ready(tpm) < 0)
		return -1;

	*len = offset;
	return 0;
}

int lpctpm_close(LpcTpm *tpm)
{
	uint8_t *access = &tpm->regs->localities[0].access;

	if (readb(access) & TisAccessActiveLocality) {
		writeb(TisAccessActiveLocality, access);

		if (lpctpm_wait_reg(access, TisAccessActiveLocality, 0)) {
			printf("%s:%d - failed to release locality 0\n",
			       __FILE__, __LINE__);
			return -1;
		}
	}
	return 0;
}

int lpctpm_close_cleanup(CleanupFunc *cleanup, CleanupType type)
{
	LpcTpm *tpm = cleanup->data;
	return lpctpm_close(tpm);
}

/*
 * Probe the TPM device and try determining its manufacturer/device name.
 *
 * @tpm - pointer to the TPM structure
 *
 * Returns 0 on success (the device is found or was found during an earlier
 * invocation) or -1 if the device is not found.
 */
int lpctpm_init(LpcTpm *tpm)
{
	uint8_t *access = &tpm->regs->localities[0].access;
	const char *device_name = "unknown";
	const char *vendor_name = device_name;

	if (vendor_dev_id)
		return 0;

	uint32_t didvid = readl(&tpm->regs->localities[0].did_vid);
	if (!didvid || (didvid == 0xffffffff)) {
		printf("%s: No TPM device found\n", __func__);
		return -1;
	}

	vendor_dev_id = didvid;

	uint16_t vid = didvid & 0xffff;
	uint16_t did = (didvid >> 16) & 0xffff;
	for (int i = 0; i < ARRAY_SIZE(vendor_names); i++) {
		if (vid == vendor_names[i].vendor_id)
			vendor_name = vendor_names[i].vendor_name;
		else
			continue;

		for (const struct device_name *dev = vendor_names[i].dev_names;
				dev->dev_id != 0xffff; dev++) {
			if (dev->dev_id == did) {
				device_name = dev->dev_name;
				break;
			}
		}
		break;
	}

	printf("Found TPM %s by %s\n", device_name, vendor_name);

	if (lpctpm_close(tpm))
		return -1;

	// Now request access to locality.
	writeb(TisAccessRequestUse, access);

	// Did we get a lock?
	if (lpctpm_wait_reg(access, TisAccessActiveLocality,
			    TisAccessActiveLocality)) {
		printf("%s:%d - failed to lock locality 0\n",
		       __FILE__, __LINE__);
		return -1;
	}

	// Certain TPMs need some delay here or they hang.
	udelay(10);

	if (lpctpm_command_ready(tpm) < 0)
		return -1;

	list_insert_after(&tpm->cleanup.list_node, &cleanup_funcs);

	tpm->initialized = 1;

	return 0;
}

int lpctpm_xmit(TpmOps *me, const uint8_t *sendbuf, size_t send_size,
		uint8_t *recvbuf, size_t *recv_len)
{
	LpcTpm *tpm = container_of(me, LpcTpm, ops);

	if (!tpm->initialized && lpctpm_init(tpm))
		return -1;

	if (lpctpm_senddata(tpm, sendbuf, send_size)) {
		printf("%s:%d failed sending data to TPM\n",
		       __FILE__, __LINE__);
		return -1;
	}

	return lpctpm_readresponse(tpm, recvbuf, recv_len);
}

LpcTpm *new_lpc_tpm(void *addr)
{
	LpcTpm *tpm = xzalloc(sizeof(*tpm));
	tpm->ops.xmit = &lpctpm_xmit;
	tpm->regs = addr;
	tpm->cleanup.cleanup = &lpctpm_close_cleanup;
	tpm->cleanup.types = CleanupOnReboot | CleanupOnPowerOff |
			     CleanupOnHandoff | CleanupOnLegacy;
	tpm->cleanup.data = tpm;

	return tpm;
}
