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

#ifndef __DRIVERS_TPM_H__
#define __DRIVERS_TPM_H__

#include <stdint.h>

/* Base TPM address standard for x86 systems */
#define CONFIG_TPM_TIS_BASE_ADDRESS        0xfed40000

/* the macro accepts the locality value, but only locality 0 is operational */
#define TIS_REG(LOCALITY, REG) \
    (void *)(CONFIG_TPM_TIS_BASE_ADDRESS + (LOCALITY << 12) + REG)

/* hardware registers' offsets */
#define TIS_REG_ACCESS                 0x0
#define TIS_REG_INT_ENABLE             0x8
#define TIS_REG_INT_VECTOR             0xc
#define TIS_REG_INT_STATUS             0x10
#define TIS_REG_INTF_CAPABILITY        0x14
#define TIS_REG_STS                    0x18
#define TIS_REG_DATA_FIFO              0x24
#define TIS_REG_DID_VID                0xf00
#define TIS_REG_RID                    0xf04

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
 * after continuous polling. No actual TPM register reading ever returns ~0,
 * so this value is a safe error indication to be mixed with possible status
 * register values.
 */
#define TPM_TIMEOUT_ERR			(~0)

/* Error value returned on various TPM driver errors */
#define TPM_DRIVER_ERR		(~0)

 /* 1 second is plenty for anything TPM does.*/
#define MAX_DELAY_US	(1000 * 1000)

/* Retrieve burst count value out of the status register contents. */
#define BURST_COUNT(status) ((u16)(((status) >> TIS_STS_BURST_COUNT_SHIFT) & \
				   TIS_STS_BURST_COUNT_MASK))

/* TPM access functions are carved out to make tracing easier. */
uint32_t tpm_read(int locality, uint32_t reg);
void tpm_write(uint32_t value, int locality, uint32_t reg);

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
		      uint8_t expected);

/*
 * Probe the TPM device and try determining its manufacturer/device name.
 *
 * Returns 0 on success (the device is found or was found during an earlier
 * invocation) or TPM_DRIVER_ERR if the device is not found.
 */
uint32_t tis_probe(void);

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
uint32_t tis_senddata(const uint8_t *const data, uint32_t len);

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
uint32_t tis_readresponse(uint8_t *buffer, uint32_t *len);

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
		 uint8_t *recvbuf, size_t *recv_len);

#endif /* __DRIVERS_TPM_H__ */
