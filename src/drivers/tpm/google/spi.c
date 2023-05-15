/*
 * Copyright 2016 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is a driver for a SPI interfaced TPM2 device.
 *
 * It assumes that the required SPI interface has been initialized before the
 * driver is started. A 'sruct spi_peripheral' pointer passed at initialization
 * is used to direct traffic to the correct SPI interface. This driver does not
 * provide a way to instantiate multiple TPM devices. Also, to keep things
 * simple, the driver unconditionally uses of TPM locality zero.
 *
 * References to documentation are based on the TCG issued "TPM Profile (PTP)
 * Specification Revision 00.43".
 */

#include <assert.h>
#include <libpayload.h>

#include "drivers/timer/timer.h"
#include "drivers/tpm/google/spi.h"
#include "drivers/tpm/google/tpm.h"

/************************************************************/
/*  Plumbing to make porting of the coreboot driver easier. */
struct spi_peripheral {};

struct tpm2_info {
	uint16_t vendor_id;
	uint16_t device_id;
	uint16_t revision;
};

/*
 * Use static driver structure, we're not going to have multiple TPM devices
 * on board.
 */
static SpiTpm spi_tpm;

static int spi_claim_bus(struct spi_peripheral *unused)
{
	return spi_tpm.bus->start(spi_tpm.bus);
}

static void spi_release_bus(struct spi_peripheral *unused)
{
	spi_tpm.bus->stop(spi_tpm.bus);
}

static int spi_xfer(struct spi_peripheral *unused, const void *dout,
		    unsigned bytesout, void *din, unsigned bytesin)
{
	unsigned count;

	count = bytesin;
	if (bytesin < bytesout)
		count = bytesout;
	else
		count = bytesin;

	return spi_tpm.bus->transfer(spi_tpm.bus, din, dout, count);
}

static uint32_t read_be32(const void *ptr)
{
	uint32_t result;

	memcpy(&result, ptr, sizeof(result));
	return be32toh(result);
}

/*********************************************************/
/*   coreboot driver with as little changes as possible. */


#define TPM_LOCALITY_0_SPI_BASE 0x00d40000

/* Assorted TPM2 registers for interface type FIFO. */
#define TPM_ACCESS_REG    (TPM_LOCALITY_0_SPI_BASE + 0)
#define TPM_STS_REG       (TPM_LOCALITY_0_SPI_BASE + 0x18)
#define TPM_DATA_FIFO_REG (TPM_LOCALITY_0_SPI_BASE + 0x24)
#define TPM_DID_VID_REG   (TPM_LOCALITY_0_SPI_BASE + 0xf00)
#define TPM_RID_REG       (TPM_LOCALITY_0_SPI_BASE + 0xf04)

/* SPI Interface descriptor used by the driver. */
struct tpm_spi_if {
	struct spi_peripheral *peripheral;
	int (*cs_assert)(struct spi_peripheral *peripheral);
	void (*cs_deassert)(struct spi_peripheral *peripheral);
	int  (*xfer)(struct spi_peripheral *peripheral, const void *dout,
		     unsigned bytesout, void *din,
		     unsigned bytesin);
};

/* Use the common SPI driver wrapper as the interface callbacks. */
static struct tpm_spi_if tpm_if = {
	.cs_assert = spi_claim_bus,
	.cs_deassert = spi_release_bus,
	.xfer = spi_xfer
};

/* Cached TPM device identification. */
static struct tpm2_info tpm_info;

/*
 * TODO(vbendeb): make CONFIG_DEBUG_TPM an int to allow different level of
 * debug traces. Right now it is either 0 or 1.
 */
static const int debug_level_ = 0;

/*
 * SPI frame header for TPM transactions is 4 bytes in size, it is described
 * in section "6.4.6 Spi Bit Protocol".
 */
typedef struct {
	unsigned char body[4];
} spi_frame_header;

static int tpm_irq_status(void)
{
	if (!spi_tpm.irq_status) {
		mdelay(10);
		return 1;
	}

	return spi_tpm.irq_status();
}

/*
 * TPM may trigger a irq after finish processing previous transfer.
 * Waiting for this irq to sync tpm status.
 *
 * Returns 0 on success, a negative value on failure (timeout).
 */
static int tpm_sync(void)
{
	struct stopwatch sw;

	stopwatch_init_msecs_expire(&sw, CONFIG_TPM_GOOGLE_IRQ_TIMEOUT_MS);
	while (!tpm_irq_status()) {
		if (stopwatch_expired(&sw)) {
			printf("Timeout wait for tpm irq!\n");
			return -1;
		}
	}
	return 0;
}

enum {
	SPI_WRITE = 0,
	SPI_READ
};

/*
 * Each TPM2 SPI transaction starts the same: CS is asserted, the 4 byte
 * header is sent to the TPM, the controller waits til TPM is ready to continue.
 *
 * Returns 0 on success, a negative value on failure (TPM2 flow control
 * timeout).
 */
static int start_transaction(int read_write, size_t bytes, unsigned addr)
{
	spi_frame_header header;
	uint8_t byte;
	int i;
	struct stopwatch sw;

	/* Wait for tpm to finish previous transaction */
	tpm_sync();

	/* Try to wake gsc if it is asleep. */
	tpm_if.cs_assert(tpm_if.peripheral);
	udelay(1);
	tpm_if.cs_deassert(tpm_if.peripheral);
	udelay(100);

	/*
	 * The first byte of the frame header encodes the transaction type
	 * (read or write) and transfer size (set to length - 1), limited to
	 * 64 bytes.
	 */
	header.body[0] = (read_write ? 0x80 : 0) | 0x40 | (bytes - 1);

	/* The rest of the frame header is the TPM register address. */
	for (i = 0; i < 3; i++)
		header.body[i + 1] = (addr >> (8 * (2 - i))) & 0xff;

	/* CS assert wakes up the peripheral. */
	tpm_if.cs_assert(tpm_if.peripheral);

	/*
	 * The TCG TPM over SPI specification introduces the notion of SPI
	 * flow control (Section "6.4.5 Flow Control").
	 *
	 * Again, the peripheral (TPM device) expects each transaction to start
	 * with a 4 byte header trasmitted by controller. The header indicates
	 * if the controller needs to read or write a register, and the register
	 * address.
	 *
	 * If the peripheral needs to stall the transaction (for instance it is
	 * not ready to send the register value to the controller), it sets the
	 * SDO (Serial Data Out) line to 0 during the last clock of the 4 byte
	 * header. In this case the controller is supposed to start polling the
	 * SPI bus, one byte at time, until the last bit in the received byte
	 * (transferred during the last clock of the byte) is set to 1.
	 *
	 * Due to some SPI controllers' shortcomings (Rockchip comes to
	 * mind...) we transmit the 4 byte header without checking the byte
	 * transmitted by the TPM during the transaction's last byte.
	 *
	 * We know that gsc is guaranteed to set the flow control bit to 0
	 * during the header transfer, but real TPM2 might be fast enough not
	 * to require to stall the controller, this would present an issue.
	 * crosbug.com/p/52132 has been opened to track this.
	 */
	tpm_if.xfer(tpm_if.peripheral, header.body, sizeof(header.body),
		    NULL, 0);

	/* Now poll the bus until TPM removes the stall bit. */
	stopwatch_init_msecs_expire(&sw, 10);
	do {
		byte = 0;
		tpm_if.xfer(tpm_if.peripheral, NULL, 0, &byte, 1);
		if (stopwatch_expired(&sw)) {
			printf("TPM flow control failure\n");
			return -1;
		}
	} while (!(byte & 1));
	return 0;
}

/*
 * Print out the contents of a buffer, if debug is enabled. Skip registers
 * other than FIFO, unless debug_level_ is 2.
 */
static void trace_dump(const char *prefix, uint32_t reg,
		       size_t bytes, const uint8_t *buffer,
		       int force)
{
	static char prev_prefix;
	static unsigned prev_reg;
	static int current_char;
	const int BYTES_PER_LINE = 32;

	if (!force) {
		if (!debug_level_)
			return;

		if ((debug_level_ < 2) && (reg != TPM_DATA_FIFO_REG))
			return;
	}

	/*
	 * Do not print register address again if the last dump print was for
	 * that register.
	 */
	if ((prev_prefix != *prefix) || (prev_reg != reg)) {
		prev_prefix = *prefix;
		prev_reg = reg;
		printf("\n%s %2.2x:", prefix, reg);
		current_char = 0;
	}

	if ((reg != TPM_DATA_FIFO_REG) && (bytes == 4)) {
		/*
		 * This must be a regular register address, print the 32 bit
		 * value.
		 */
		printf(" %8.8x", *(const uint32_t *)buffer);
	} else {
		int i;

		/*
		 * Data read from or written to FIFO or not in 4 byte
		 * quantiites is printed byte at a time.
		 */
		for (i = 0; i < bytes; i++) {
			if (current_char && !(current_char % BYTES_PER_LINE)) {
				printf("\n     ");
				current_char = 0;
			}
			current_char++;
			printf(" %2.2x", buffer[i]);
		}
	}
}

/*
 * Once transaction is initiated and the TPM indicated that it is ready to go,
 * write the actual bytes to the register.
 */
static void write_bytes(const void *buffer, size_t bytes)
{
	tpm_if.xfer(tpm_if.peripheral, buffer, bytes, NULL, 0);
}

/*
 * Once transaction is initiated and the TPM indicated that it is ready to go,
 * read the actual bytes from the register.
 */
static int read_bytes(void *buffer, size_t bytes)
{
	return tpm_if.xfer(tpm_if.peripheral, NULL, 0, buffer, bytes);
}

/*
 * To write a register, start transaction, transfer data to the TPM, deassert
 * CS when done.
 *
 * Returns 0 to indicate success, -1 (not yet implemented) to indicate failure.
 */
static int tpm2_write_reg(unsigned reg_number, const void *buffer, size_t bytes)
{
	int result = 0;
	trace_dump("W", reg_number, bytes, buffer, 0);
	if (start_transaction(SPI_WRITE, bytes, reg_number)) {
		printf("failed to write tpm reg %#x\n", reg_number);
		result = -1;
	} else {
		write_bytes(buffer, bytes);
	}
	tpm_if.cs_deassert(tpm_if.peripheral);
	return result;
}

/*
 * To read a register, start transaction, transfer data from the TPM, deassert
 * CS when done.
 *
 * Returns 0 to indicate success, -1 (not yet implemented) to indicate failure.
 */
static int tpm2_read_reg(unsigned reg_number, void *buffer, size_t bytes)
{
	int result = 0;
	if (start_transaction(SPI_READ, bytes, reg_number) ||
	    read_bytes(buffer, bytes)) {
		printf("failed to read tpm reg %#x\n", reg_number);
		memset(buffer, 0, bytes);
		result = -1;
	}
	tpm_if.cs_deassert(tpm_if.peripheral);
	trace_dump("R", reg_number, bytes, buffer, 0);
	return result;
}

/*
 * Status register is accessed often, wrap reading and writing it into
 * dedicated functions.
 */
static int read_tpm_sts(uint32_t *status)
{
	return tpm2_read_reg(TPM_STS_REG, status, sizeof(*status));
}

static int write_tpm_sts(uint32_t status)
{
	return tpm2_write_reg(TPM_STS_REG, &status, sizeof(status));
}

/*
 * The TPM may limit the transaction bytes count (burst count) below the 64
 * bytes max. The current value is available as a field of the status
 * register.
 */
static uint32_t get_burst_count(void)
{
	uint32_t status;

	read_tpm_sts(&status);
	return (status & TpmStsBurstCountMask) >> TpmStsBurstCountShift;
}

static uint8_t tpm2_read_access_reg(void)
{
	uint8_t access;
	tpm2_read_reg(TPM_ACCESS_REG, &access, sizeof(access));
	/* We do not care about access establishment bit state. Ignore it. */
	return access & ~TpmAccessEstablishment;
}

static void tpm2_write_access_reg(uint8_t cmd)
{
	/* Writes to access register can set only 1 bit at a time. */
	assert (!(cmd & (cmd - 1)));
	tpm2_write_reg(TPM_ACCESS_REG, &cmd, sizeof(cmd));
}

static int tpm2_claim_locality(void)
{
	uint8_t access;

	access = tpm2_read_access_reg();
	/* If the requested locality is already active and TPM access is valid,
	   return success. */
	if (access & (TpmAccessValid | TpmAccessActiveLocality))
		return 0;
	/*
	 * If active locality is set (maybe reset line is not connected?),
	 * release the locality and try again.
	 */
	if (access & TpmAccessActiveLocality) {
		tpm2_write_access_reg(TpmAccessActiveLocality);
		access = tpm2_read_access_reg();
	}

	if (access != TpmAccessValid) {
		printf("Invalid reset status: %#x\n", access);
		return -1;
	}

	tpm2_write_access_reg(TpmAccessRequestUse);
	access = tpm2_read_access_reg();
	if (access != (TpmAccessValid | TpmAccessActiveLocality)) {
		printf("Failed to claim locality 0, status: %#x\n", access);
		return -1;
	}

	return 0;
}

static int tpm2_init(SpiOps *spi_ops)
{
	uint32_t did_vid, status;
	uint8_t cmd;

	if (tpm2_read_reg(TPM_DID_VID_REG, &did_vid, sizeof(did_vid)))
		return -1;

	/* Claim locality 0. */
	if (tpm2_claim_locality())
		return -1;

	read_tpm_sts(&status);
	if ((status & TpmStsFamilyMask) != TpmStsFamilyTpm2) {
		printf("unexpected TPM family value, status: %#x\n",
		       status);
		return -1;
	}

	/*
	 * Locality claimed, read the revision value and set up the tpm_info
	 * structure.
	 */
	tpm2_read_reg(TPM_RID_REG, &cmd, sizeof(cmd));
	tpm_info.vendor_id = did_vid & 0xffff;
	tpm_info.device_id = did_vid >> 16;
	tpm_info.revision = cmd;

	printf("Connected to device vid:did:rid of %4.4x:%4.4x:%2.2x\n",
	       tpm_info.vendor_id, tpm_info.device_id, tpm_info.revision);

	return 0;
}

/*
 * This is in seconds, certain TPM commands, like key generation, can take
 * long time to complete.
 *
 * Returns true to indicate success, false (not yet implemented) to indicate
 * failure.
 */
#define MAX_STATUS_TIMEOUT 120
static bool wait_for_status(uint32_t status_mask, uint32_t status_expected)
{
	uint32_t status;
	struct stopwatch sw;

	stopwatch_init_usecs_expire(&sw, MAX_STATUS_TIMEOUT * USECS_PER_SEC);
	do {
		udelay(MSECS_PER_SEC);
		if (stopwatch_expired(&sw)) {
			printf("failed to get expected status %#x\n",
			       status_expected);
			return false;
		}
		if (read_tpm_sts(&status)) {
			printf("Failed to read expected status %#x\n",
			       status_expected);
			return false;
		}
	} while ((status & status_mask) != status_expected);

	return true;
}

enum fifo_transfer_direction {
	fifo_transmit = 0,
	fifo_receive = 1
};

/* Union allows to avoid casting away 'const' on transmit buffers. */
union fifo_transfer_buffer {
	uint8_t *rx_buffer;
	const uint8_t *tx_buffer;
};

/*
 * Transfer requested number of bytes to or from TPM FIFO, accounting for the
 * current burst count value.
 */
static void fifo_transfer(size_t transfer_size,
			  union fifo_transfer_buffer buffer,
			  enum fifo_transfer_direction direction)
{
	size_t transaction_size;
	size_t burst_count;
	size_t handled_so_far = 0;

	do {
		struct stopwatch sw;
		stopwatch_init_msecs_expire(&sw, 100);

		do {
			/* Could be zero when TPM is busy. */
			burst_count = get_burst_count();
			if (stopwatch_expired(&sw)) {
				printf("exceeded tpm wait in burst loop\n");
				return;
			}
		} while (!burst_count);

		transaction_size = transfer_size - handled_so_far;
		transaction_size = MIN(transaction_size, burst_count);

		/*
		 * The SPI frame header does not allow to pass more than 64
		 * bytes.
		 */
		transaction_size = MIN(transaction_size, 64);

		if (direction == fifo_receive)
			tpm2_read_reg(TPM_DATA_FIFO_REG,
				      buffer.rx_buffer + handled_so_far,
				      transaction_size);
		else
			tpm2_write_reg(TPM_DATA_FIFO_REG,
				       buffer.tx_buffer + handled_so_far,
				       transaction_size);

		handled_so_far += transaction_size;

	} while (handled_so_far != transfer_size);
}

static size_t tpm2_process_command(const void *tpm2_command,
				   size_t command_size,
				   void *tpm2_response,
				   size_t max_response)
{
	uint32_t status;
	uint32_t expected_status_bits;
	size_t payload_size;
	size_t bytes_to_go;
	const uint8_t *cmd_body = tpm2_command;
	uint8_t *rsp_body = tpm2_response;
	union fifo_transfer_buffer fifo_buffer;
	const int HEADER_SIZE = 6;

	/* Do not try using an uninitialized TPM. */
	if (!tpm_info.vendor_id)
		return 0;

	/* Skip the two byte tag, read the size field. */
	payload_size = read_be32(cmd_body + 2);

	/* Confidence check. */
	if (payload_size != command_size) {
		printf("Command size mismatch: encoded %zd != requested %zd\n",
		       payload_size, command_size);
		trace_dump("W", TPM_DATA_FIFO_REG, command_size, cmd_body, 1);
		printf("\n");
		return 0;
	}

	/* Let the TPM know that the command is coming. */
	if (write_tpm_sts(TpmStsCommandReady)) {
		printf("Failed to notify tpm of incoming command\n");
		return 0;
	}

	/*
	 * Tpm commands and responses written to and read from the FIFO
	 * register (0x24) are datagrams of variable size, prepended by a 6
	 * byte header.
	 *
	 * The specification description of the state machine is a bit vague,
	 * but from experience it looks like there is no need to wait for the
	 * sts.expect bit to be set, at least with the 9670 and gsc devices.
	 * Just write the command into FIFO, making sure not to exceed the
	 * burst count or the maximum PDU size, whatever is smaller.
	 */
	fifo_buffer.tx_buffer = cmd_body;
	fifo_transfer(command_size, fifo_buffer, fifo_transmit);

	/* Now tell the TPM it can start processing the command. */
	if (write_tpm_sts(TpmStsGo)) {
		printf("Failed to notify tpm to process command\n");
		return 0;
	}

	/* Now wait for it to report that the response is ready. */
	expected_status_bits = TpmStsValid | TpmStsDataAvail;
	if (!wait_for_status(expected_status_bits, expected_status_bits)) {
		/*
		 * If timed out, which should never happen, let's at least
		 * print out the offending command.
		 */
		trace_dump("W", TPM_DATA_FIFO_REG, command_size, cmd_body, 1);
		printf("\n");
		return 0;
	}

	/*
	 * The response is ready, let's read it. First we read the FIFO
	 * payload header, to see how much data to expect. The response header
	 * size is fixed to six bytes, the total payload size is stored in
	 * network order in the last four bytes.
	 */
	tpm2_read_reg(TPM_DATA_FIFO_REG, rsp_body, HEADER_SIZE);

	/* Find out the total payload size, skipping the two byte tag. */
	payload_size = read_be32(rsp_body + 2);

	if (payload_size > max_response) {
		/*
		 * TODO(vbendeb): at least drain the FIFO here or somehow let
		 * the TPM know that the response can be dropped.
		 */
		printf(" tpm response too long (%zd bytes)",
		       payload_size);
		return 0;
	}

	/*
	 * Now let's read all but the last byte in the FIFO to make sure the
	 * status register is showing correct flow control bits: 'more data'
	 * until the last byte and then 'no more data' once the last byte is
	 * read.
	 */
	bytes_to_go = payload_size - 1 - HEADER_SIZE;
	fifo_buffer.rx_buffer = rsp_body + HEADER_SIZE;
	fifo_transfer(bytes_to_go, fifo_buffer, fifo_receive);

	/* Verify that there is still data to read. */
	read_tpm_sts(&status);
	if ((status & expected_status_bits) != expected_status_bits) {
		printf("unexpected intermediate status %#x\n",
		       status);
		return 0;
	}

	/* Read the last byte of the PDU. */
	tpm2_read_reg(TPM_DATA_FIFO_REG, rsp_body + payload_size - 1, 1);

	/* Terminate the dump, if enabled. */
	if (debug_level_)
		printf("\n");

	/* Verify that 'data available' is not asseretd any more. */
	read_tpm_sts(&status);
	if ((status & expected_status_bits) != TpmStsValid) {
		printf("unexpected final status %#x\n", status);
		return 0;
	}

	/*
	 * Move the TPM back to idle state. This will likely fail if the current
	 * command is disabling the tpm.
	 */
	write_tpm_sts(TpmStsCommandReady);

	return payload_size;
}

/*********************************************************/
/* Depthcharge interface to the coreboot SPI TPM driver. */

static int tpm_initialized;
static int tpm_cleanup(CleanupFunc *cleanup, CleanupType type)
{
	printf("%s: add release locality here.\n", __func__);
	return 0;
}

static int xmit_wrapper(struct TpmOps *me,
			const uint8_t *sendbuf,
			size_t send_size,
			uint8_t *recvbuf,
			size_t *recv_len)
{
	size_t response_size;
	SpiTpm *tpm = container_of(me, SpiTpm, ops);

	if (!tpm_initialized) {
		if (tpm2_init(tpm->bus))
			return -1;

		list_insert_after(&spi_tpm.cleanup.list_node, &cleanup_funcs);
		tpm_initialized = 1;
	}

	response_size = tpm2_process_command(sendbuf,
					     send_size, recvbuf, *recv_len);
	if (response_size) {
		*recv_len = response_size;
		return 0;
	}

	return -1;
}

SpiTpm *new_tpm_spi(SpiOps *bus, tpm_irq_status_t irq_status)
{
	spi_tpm.ops.xmit = xmit_wrapper;
	spi_tpm.irq_status = irq_status;
	spi_tpm.bus = bus;
	spi_tpm.cleanup.cleanup = tpm_cleanup;
	spi_tpm.cleanup.types = CleanupOnReboot | CleanupOnPowerOff |
		CleanupOnHandoff | CleanupOnLegacy;

	if (!irq_status)
		printf("WARNING: tpm irq not defined, will waste 10ms to wait on GSC!!\n");

	return &spi_tpm;
}
