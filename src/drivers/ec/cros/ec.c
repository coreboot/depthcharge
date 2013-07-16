/*
 * Chromium OS EC driver
 *
 * Copyright 2012 Google Inc.
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * The Matrix Keyboard Protocol driver handles talking to the keyboard
 * controller chip. Mostly this is for keyboard functions, but some other
 * things have slipped in, so we provide generic services to talk to the
 * KBC.
 */

#include <assert.h>
#include <libpayload.h>

#include "base/time.h"
#include "drivers/ec/cros/message.h"
#include "drivers/ec/cros/ec.h"

static CrosEcBusOps *cros_ec_bus;

static int initialized;

int cros_ec_set_bus(CrosEcBusOps *bus)
{
	if (cros_ec_bus) {
		printf("ChromeOS EC bus already configured.\n");
		return -1;
	}
	cros_ec_bus = bus;
	return 0;
}

/* Timeout waiting for a flash erase command to complete */
static const int CROS_EC_CMD_TIMEOUT_MS = 5000;

/* Timeout waiting for EC hash calculation completion */
static const int CROS_EC_HASH_TIMEOUT_MS = 2000;

/* Time to delay between polling status of EC hash calculation */
static const int CROS_EC_HASH_CHECK_DELAY_MS = 10;

/* Note: depends on enum ec_current_image */
static const char * const ec_current_image_name[] = {"unknown", "RO", "RW"};

void cros_ec_dump_data(const char *name, int cmd, const uint8_t *data, int len)
{
#ifdef DEBUG
	int i;

	printf("%s: ", name);
	if (cmd != -1)
		printf("cmd=%#x: ", cmd);
	for (i = 0; i < len; i++)
		printf("%02x ", data[i]);
	printf("\n");
#endif
}

/*
 * Calculate a simple 8-bit checksum of a data block
 *
 * @param data	Data block to checksum
 * @param size	Size of data block in bytes
 * @return checksum value (0 to 255)
 */
uint8_t cros_ec_calc_checksum(const uint8_t *data, int size)
{
	uint8_t csum, i;

	for (i = csum = 0; i < size; i++)
		csum += data[i];
	return csum & 0xff;
}

/**
 * Send a command to the ChromeOS EC device and return the reply.
 *
 * The device's internal input/output buffers are used.
 *
 * @param cmd		Command to send (EC_CMD_...)
 * @param cmd_version	Version of command to send (EC_VER_...)
 * @param dout          Output data (may be NULL If dout_len=0)
 * @param dout_len      Size of output data in bytes
 * @param dinp          Response data (may be NULL If din_len=0).
 *			The value of *dinp is a place for ec_command() to put
 *			the data (it will be copied there). If NULL on entry,
 *			then it will be updated to point to the data (no copy)
 *			and will always be double word aligned (64-bits)
 * @param din_len       Maximum size of response in bytes
 * @return number of bytes in response, or -1 on error
 */
static int ec_command(uint8_t cmd, int cmd_version,
		      const void *dout, int dout_len,
		      uint8_t **dinp, int din_len)
{
	uint8_t *din;
	int len;

	if (!cros_ec_bus) {
		printf("No ChromeOS EC bus configured.\n");
		return -1;
	}

	if (!initialized && cros_ec_init())
		return -1;

	len = cros_ec_bus->send_command(cros_ec_bus, cmd, cmd_version, dout,
					dout_len, &din, din_len);

	/* If the command doesn't complete, wait a while */
	if (len == -EC_RES_IN_PROGRESS) {
		struct ec_response_get_comms_status *resp;
		uint64_t start;

		/* Wait for command to complete */
		start = timer_us(0);
		do {
			int ret;

			mdelay(50);	/* Insert some reasonable delay */
			ret = cros_ec_bus->send_command(
				cros_ec_bus, EC_CMD_GET_COMMS_STATUS,
				0, NULL, 0, (uint8_t **)&resp, sizeof(*resp));
			if (ret < 0)
				return ret;

			if (timer_us(start) > CROS_EC_CMD_TIMEOUT_MS * 1000) {
				printf("%s: Command %#02x timeout",
				      __func__, cmd);
				return -EC_RES_TIMEOUT;
			}
		} while (resp->flags & EC_COMMS_STATUS_PROCESSING);

		/* OK it completed, so read the status response */
		len = cros_ec_bus->send_command(
			cros_ec_bus, EC_CMD_RESEND_RESPONSE,
			0, NULL, 0, &din, 0);
	}

	/*
	 * If we were asked to put it somewhere, do so, otherwise just
	 * return a pointer to the data in dinp.
	 */
#ifdef DEBUG
	printf("%s: len=%d, din=%p, dinp=%p, *dinp=%p\n", __func__,
		len, din, dinp, dinp ? *dinp : NULL);
#endif
	if (dinp) {
		/* If we have any data to return, it must be 64bit-aligned */
		assert(len <= 0 || !((uintptr_t)din & 7));
		if (len > 0) {
			if (*dinp)
				memmove(*dinp, din, len);
			else
				*dinp = din;
		}
	}

	return len;
}

int cros_ec_scan_keyboard(struct cros_ec_keyscan *scan)
{
	if (ec_command(EC_CMD_MKBP_STATE, 0, NULL, 0, (uint8_t **)&scan,
		       sizeof(scan->data)) < sizeof(scan->data))
		return -1;

	return 0;
}

int cros_ec_read_id(char *id, int maxlen)
{
	struct ec_response_get_version *r = NULL;

	if (ec_command(EC_CMD_GET_VERSION, 0, NULL, 0, (uint8_t **)&r,
		       sizeof(*r)) < sizeof(*r))
		return -1;

	if (maxlen > sizeof(r->version_string_ro))
		maxlen = sizeof(r->version_string_ro);

	switch (r->current_image) {
	case EC_IMAGE_RO:
		memcpy(id, r->version_string_ro, maxlen);
		break;
	case EC_IMAGE_RW:
		memcpy(id, r->version_string_rw, maxlen);
		break;
	default:
		return -1;
	}

	id[maxlen - 1] = '\0';
	return 0;
}

int cros_ec_read_version(struct ec_response_get_version **versionp)
{
	*versionp = NULL;
	if (ec_command(EC_CMD_GET_VERSION, 0, NULL, 0, (uint8_t **)versionp,
			sizeof(**versionp)) < sizeof(**versionp))
		return -1;

	return 0;
}

int cros_ec_read_build_info(char **strp)
{
	*strp = NULL;
	if (ec_command(EC_CMD_GET_BUILD_INFO, 0, NULL, 0, (uint8_t **)strp,
			EC_HOST_PARAM_SIZE) < 0)
		return -1;

	return 0;
}

int cros_ec_read_current_image(enum ec_current_image *image)
{
	struct ec_response_get_version *r = NULL;

	if (ec_command(EC_CMD_GET_VERSION, 0, NULL, 0, (uint8_t **)&r,
		       sizeof(*r)) < sizeof(*r))
		return -1;

	*image = r->current_image;
	return 0;
}

int cros_ec_read_hash(struct ec_response_vboot_hash *hash)
{
	struct ec_params_vboot_hash p;
	uint64_t start;
	int recalc_requested = 0;

	start = timer_us(0);
	do {
		/* Get hash if available. */
		p.cmd = EC_VBOOT_HASH_GET;
		if (ec_command(EC_CMD_VBOOT_HASH, 0, &p, sizeof(p),
			       (uint8_t **)&hash, sizeof(*hash)) < 0)
			return -1;

		switch (hash->status) {
		case EC_VBOOT_HASH_STATUS_NONE:
			/* We have no valid hash - let's request a recalc
			 * if we haven't done so yet. */
			if (recalc_requested != 0)
				break;
			printf("%s: No valid hash (status=%d size=%d). "
			      "Compute one...\n", __func__, hash->status,
			      hash->size);

			p.cmd = EC_VBOOT_HASH_START;
			p.hash_type = EC_VBOOT_HASH_TYPE_SHA256;
			p.nonce_size = 0;
			p.offset = EC_VBOOT_HASH_OFFSET_RW;

			if (ec_command(EC_CMD_VBOOT_HASH, 0, &p,
				       sizeof(p), (uint8_t **)&hash,
				       sizeof(*hash)) < 0)
				return -1;

			recalc_requested = 1;
			/* Expect status to be busy (and don't break while)
			 * since we just sent a recalc request. */
			hash->status = EC_VBOOT_HASH_STATUS_BUSY;
			break;
		case EC_VBOOT_HASH_STATUS_BUSY:
			/* Hash is still calculating. */
			mdelay(CROS_EC_HASH_CHECK_DELAY_MS);
			break;
		case EC_VBOOT_HASH_STATUS_DONE:
		default:
			/* We have a valid hash. */
			break;
		}
	} while (hash->status == EC_VBOOT_HASH_STATUS_BUSY &&
		 timer_us(start) < CROS_EC_HASH_TIMEOUT_MS * 1000);

	if (hash->status != EC_VBOOT_HASH_STATUS_DONE) {
		printf("%s: Hash status not done: %d\n", __func__,
		      hash->status);
		return -1;
	}

	return 0;
}

int cros_ec_reboot(enum ec_reboot_cmd cmd, uint8_t flags)
{
	struct ec_params_reboot_ec p;

	p.cmd = cmd;
	p.flags = flags;

	if (ec_command(EC_CMD_REBOOT_EC, 0, &p, sizeof(p), NULL, 0) < 0)
		return -1;

	if (!(flags & EC_REBOOT_FLAG_ON_AP_SHUTDOWN)) {
		/*
		 * EC reboot will take place immediately so delay to allow it
		 * to complete.  Note that some reboot types (EC_REBOOT_COLD)
		 * will reboot the AP as well, in which case we won't actually
		 * get to this point.
		 */
		int timeout = 20;
		do {
			mdelay(50);
		} while (timeout-- && cros_ec_test());

		if (!timeout)
			return -1;
	}

	return 0;
}

int cros_ec_interrupt_pending(void)
{
	return 0;
}

int cros_ec_mkbp_info(struct ec_response_mkbp_info *info)
{
	if (ec_command(EC_CMD_MKBP_INFO, 0, NULL, 0, (uint8_t **)&info,
			sizeof(*info)) < sizeof(*info))
		return -1;

	return 0;
}

int cros_ec_get_host_events(uint32_t *events_ptr)
{
	struct ec_response_host_event_mask *resp = NULL;

	/*
	 * Use the B copy of the event flags, because the main copy is already
	 * used by ACPI/SMI.
	 */
	if (ec_command(EC_CMD_HOST_EVENT_GET_B, 0, NULL, 0,
		       (uint8_t **)&resp, sizeof(*resp)) < sizeof(*resp))
		return -1;

	if (resp->mask & EC_HOST_EVENT_MASK(EC_HOST_EVENT_INVALID))
		return -1;

	*events_ptr = resp->mask;
	return 0;
}

int cros_ec_clear_host_events(uint32_t events)
{
	struct ec_params_host_event_mask params;

	params.mask = events;

	/*
	 * Use the B copy of the event flags, so it affects the data returned
	 * by cros_ec_get_host_events().
	 */
	if (ec_command(EC_CMD_HOST_EVENT_CLEAR_B, 0,
		       &params, sizeof(params), NULL, 0) < 0)
		return -1;

	return 0;
}

int cros_ec_flash_protect(uint32_t set_mask, uint32_t set_flags,
			  struct ec_response_flash_protect *resp)
{
	struct ec_params_flash_protect params;

	params.mask = set_mask;
	params.flags = set_flags;

	if (ec_command(EC_CMD_FLASH_PROTECT, EC_VER_FLASH_PROTECT,
		       &params, sizeof(params),
		       (uint8_t **)&resp, sizeof(*resp)) < sizeof(*resp))
		return -1;

	return 0;
}

int cros_ec_test(void)
{
	struct ec_params_hello req;
	struct ec_response_hello *resp = NULL;

	req.in_data = 0x12345678;
	if (ec_command(EC_CMD_HELLO, 0, &req, sizeof(req),
		       (uint8_t **)&resp, sizeof(*resp)) < sizeof(*resp)) {
		printf("ec_command() returned error\n");
		return -1;
	}
	if (resp->out_data != req.in_data + 0x01020304) {
		printf("Received invalid handshake %x\n", resp->out_data);
		return -1;
	}

	return 0;
}

int cros_ec_flash_offset(enum ec_flash_region region, uint32_t *offset,
			 uint32_t *size)
{
	struct ec_params_flash_region_info p;
	struct ec_response_flash_region_info *r = NULL;
	int ret;

	p.region = region;
	ret = ec_command(EC_CMD_FLASH_REGION_INFO, EC_VER_FLASH_REGION_INFO,
			 &p, sizeof(p), (uint8_t **)&r, sizeof(*r));
	if (ret != sizeof(*r))
		return -1;

	if (offset)
		*offset = r->offset;
	if (size)
		*size = r->size;

	return 0;
}

int cros_ec_flash_erase(uint32_t offset, uint32_t size)
{
	struct ec_params_flash_erase p;

	p.offset = offset;
	p.size = size;
	return ec_command(EC_CMD_FLASH_ERASE, 0, &p, sizeof(p), NULL, 0);
}

/**
 * Write a single block to the flash
 *
 * Write a block of data to the EC flash. The size must not exceed the flash
 * write block size which you can obtain from cros_ec_flash_write_burst_size().
 *
 * The offset starts at 0. You can obtain the region information from
 * cros_ec_flash_offset() to find out where to write for a particular region.
 *
 * Attempting to write to the region where the EC is currently running from
 * will result in an error.
 *
 * @param data		Pointer to data buffer to write
 * @param offset	Offset within flash to write to.
 * @param size		Number of bytes to write
 * @return 0 if ok, -1 on error
 */
static int cros_ec_flash_write_block(const uint8_t *data, uint32_t offset,
				     uint32_t size)
{
	struct ec_params_flash_write p;

	p.offset = offset;
	p.size = size;
	assert(data && p.size <= sizeof(p.data));
	memcpy(p.data, data, p.size);

	return ec_command(EC_CMD_FLASH_WRITE, 0,
			  &p, sizeof(p), NULL, 0) >= 0 ? 0 : -1;
}

/**
 * Return optimal flash write burst size
 */
static int cros_ec_flash_write_burst_size(void)
{
	struct ec_params_flash_write p;
	return sizeof(p.data);
}

int cros_ec_flash_write(const uint8_t *data, uint32_t offset, uint32_t size)
{
	uint32_t burst = cros_ec_flash_write_burst_size();
	uint32_t end, off;
	int ret;

	/*
	 * TODO: round up to the nearest multiple of write size.  Can get away
	 * without that on link right now because its write size is 4 bytes.
	 */
	end = offset + size;
	for (off = offset; off < end; off += burst, data += burst) {
		uint32_t todo;

		/* If the data is empty, there is no point in programming it */
		todo = MIN(end - off, burst);

		ret = cros_ec_flash_write_block(data, off, todo);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * Read a single block from the flash
 *
 * Read a block of data from the EC flash. The size must not exceed the flash
 * write block size which you can obtain from cros_ec_flash_write_burst_size().
 *
 * The offset starts at 0. You can obtain the region information from
 * cros_ec_flash_offset() to find out where to read for a particular region.
 *
 * @param data		Pointer to data buffer to read into
 * @param offset	Offset within flash to read from
 * @param size		Number of bytes to read
 * @return 0 if ok, -1 on error
 */
static int cros_ec_flash_read_block(uint8_t *data, uint32_t offset,
				    uint32_t size)
{
	struct ec_params_flash_read p;

	p.offset = offset;
	p.size = size;

	return ec_command(EC_CMD_FLASH_READ, 0,
			  &p, sizeof(p), &data, size) >= 0 ? 0 : -1;
}

int cros_ec_flash_read(uint8_t *data, uint32_t offset, uint32_t size)
{
	uint32_t burst = cros_ec_flash_write_burst_size();
	uint32_t end, off;
	int ret;

	end = offset + size;
	for (off = offset; off < end; off += burst, data += burst) {
		ret = cros_ec_flash_read_block(data, off,
					       MIN(end - off, burst));
		if (ret)
			return ret;
	}

	return 0;
}

int cros_ec_flash_update_rw(const uint8_t *image, int image_size)
{
	uint32_t rw_offset, rw_size;
	int ret;

	if (cros_ec_flash_offset(EC_FLASH_REGION_RW, &rw_offset, &rw_size))
		return -1;
	if (image_size > rw_size)
		return -1;

	/*
	 * Erase the entire RW section, so that the EC doesn't see any garbage
	 * past the new image if it's smaller than the current image.
	 *
	 * TODO: could optimize this to erase just the current image, since
	 * presumably everything past that is 0xff's.  But would still need to
	 * round up to the nearest multiple of erase size.
	 */
	ret = cros_ec_flash_erase(rw_offset, rw_size);
	if (ret)
		return ret;

	/* Write the image */
	ret = cros_ec_flash_write(image, rw_offset, image_size);
	if (ret)
		return ret;

	return 0;
}

int cros_ec_read_vbnvcontext(uint8_t *block)
{
	struct ec_params_vbnvcontext p;
	int len;

	p.op = EC_VBNV_CONTEXT_OP_READ;

	len = ec_command(EC_CMD_VBNV_CONTEXT, EC_VER_VBNV_CONTEXT,
			&p, sizeof(p), &block, EC_VBNV_BLOCK_SIZE);
	if (len < EC_VBNV_BLOCK_SIZE)
		return -1;

	return 0;
}

int cros_ec_write_vbnvcontext(const uint8_t *block)
{
	struct ec_params_vbnvcontext p;
	int len;

	p.op = EC_VBNV_CONTEXT_OP_WRITE;
	memcpy(p.block, block, sizeof(p.block));

	len = ec_command(EC_CMD_VBNV_CONTEXT, EC_VER_VBNV_CONTEXT,
			&p, sizeof(p), NULL, 0);
	if (len < 0)
		return -1;

	return 0;
}

int cros_ec_init(void)
{
	char id[MSG_BYTES];

	if (initialized)
		return 0;

	if (!cros_ec_bus) {
		printf("No ChromeOS EC bus configured.\n");
		return -1;
	}

	if (cros_ec_bus->init && cros_ec_bus->init(cros_ec_bus))
		return -1;

	initialized = 1;
	if (cros_ec_read_id(id, sizeof(id))) {
		printf("%s: Could not read ChromeOS EC ID\n", __func__);
		initialized = 0;
		return -1;
	}

	printf("Google ChromeOS EC driver ready, id '%s'\n", id);

	// Unconditionally clear the EC recovery request.
	printf("Clearing the recovery request.\n");
	const uint32_t kb_rec_mask =
		EC_HOST_EVENT_MASK(EC_HOST_EVENT_KEYBOARD_RECOVERY);
	cros_ec_clear_host_events(kb_rec_mask);

	return 0;
}
