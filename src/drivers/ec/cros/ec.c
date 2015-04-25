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

#include "config.h"
#include "drivers/ec/cros/message.h"
#include "drivers/ec/cros/ec.h"

static CrosEcBusOps *cros_ec_bus;
static GpioOps *cros_ec_interrupt_gpio;

typedef int (*SendCommandFunc)(int cmd, int cmd_version, const void *dout,
			       int dout_len, void *dinp, int din_len);

SendCommandFunc send_command_func;

static struct ec_host_request *proto3_request;
static int proto3_request_size;

static struct ec_host_response *proto3_response;
static int proto3_response_size;

static int max_param_size;
static int passthru_param_size;
static int initialized;

#define DEFAULT_BUF_SIZE 0x100

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

void cros_ec_dump_data(const char *name, int cmd, const void *data, int len)
{
#ifdef DEBUG
	const uint8_t *bytes = data;
	int i;

	printf("%s: ", name);
	if (cmd != -1)
		printf("cmd=%#x: ", cmd);
	for (i = 0; i < len; i++)
		printf("%02x ", bytes[i]);
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
uint8_t cros_ec_calc_checksum(const void *data, int size)
{
	uint8_t csum;
	const uint8_t *bytes = data;
	int i;

	for (i = csum = 0; i < size; i++)
		csum += bytes[i];
	return csum & 0xff;
}

/**
 * Create a request packet for protocol version 3.
 *
 * @param rq		Request structure to fill
 * @param rq_size	Size of request structure, including data
 * @param cmd		Command to send (EC_CMD_...)
 * @param cmd_version	Version of command to send (EC_VER_...)
 * @param dout          Output data (may be NULL If dout_len=0)
 * @param dout_len      Size of output data in bytes
 * @return packet size in bytes, or <0 if error.
 */
static int create_proto3_request(struct ec_host_request *rq, int rq_size,
				 int cmd, int cmd_version,
				 const void *dout, int dout_len)
{
	int out_bytes = dout_len + sizeof(*rq);

	/* Fail if output size is too big */
	if (out_bytes > rq_size) {
		printf("%s: Cannot send %d bytes\n", __func__, dout_len);
		return -EC_RES_REQUEST_TRUNCATED;
	}

	/* Fill in request packet */
	rq->struct_version = EC_HOST_REQUEST_VERSION;
	rq->checksum = 0;
	rq->command = cmd;
	rq->command_version = cmd_version;
	rq->reserved = 0;
	rq->data_len = dout_len;

	/* Copy data after header */
	memcpy(rq + 1, dout, dout_len);

	/* Write checksum field so the entire packet sums to 0 */
	rq->checksum = (uint8_t)(-cros_ec_calc_checksum(rq, out_bytes));

	cros_ec_dump_data("out", cmd, rq, out_bytes);

	/* Return size of request packet */
	return out_bytes;
}

/**
 * Prepare the device to receive a protocol version 3 response.
 *
 * @param rs_size	Maximum size of response in bytes
 * @param din_len       Maximum amount of data in the response
 * @return maximum expected number of bytes in response, or <0 if error.
 */
static int prepare_proto3_response_buffer(int rs_size, int din_len)
{
	int in_bytes = din_len + sizeof(struct ec_host_response);

	/* Fail if input size is too big */
	if (in_bytes > rs_size) {
		printf("%s: Cannot receive %d bytes\n", __func__, din_len);
		return -EC_RES_RESPONSE_TOO_BIG;
	}

	/* Return expected size of response packet */
	return in_bytes;
}

/**
 * Process a protocol version 3 response packet.
 *
 * @param resp		Response structure to parse
 * @param dinp          Returns pointer to response data
 * @param din_len       Maximum size of data in response in bytes
 * @return number of bytes of response data, or <0 if error
 */
static int handle_proto3_response(struct ec_host_response *rs,
				  uint8_t **dinp, int din_len)
{
	int in_bytes;
	int csum;

	cros_ec_dump_data("in-header", -1, rs, sizeof(*rs));

	/* Check input data */
	if (rs->struct_version != EC_HOST_RESPONSE_VERSION) {
		printf("%s: EC response version mismatch\n", __func__);
		return -EC_RES_INVALID_RESPONSE;
	}

	if (rs->reserved) {
		printf("%s: EC response reserved != 0\n", __func__);
		return -EC_RES_INVALID_RESPONSE;
	}

	if (rs->data_len > din_len) {
		printf("%s: EC returned too much data\n", __func__);
		return -EC_RES_RESPONSE_TOO_BIG;
	}

	cros_ec_dump_data("in-data", -1, rs + sizeof(*rs), rs->data_len);

	/* Update in_bytes to actual data size */
	in_bytes = sizeof(*rs) + rs->data_len;

	/* Verify checksum */
	csum = cros_ec_calc_checksum(rs, in_bytes);
	if (csum) {
		printf("%s: EC response checksum invalid: 0x%02x\n", __func__,
		      csum);
		return -EC_RES_INVALID_CHECKSUM;
	}

	/* Return error result, if any */
	if (rs->result)
		return -(int)rs->result;

	/* If the caller wants the response data, copy it out */
	if (dinp)
		memcpy(dinp, rs + 1, din_len);

	return rs->data_len;
}

static int send_command_proto3_work(int cmd, int cmd_version,
				    const void *dout, int dout_len,
				    void *dinp, int din_len)
{
	int out_bytes, in_bytes;
	int rv;

	/* Create request packet */
	out_bytes = create_proto3_request(proto3_request, proto3_request_size,
					  cmd, cmd_version, dout, dout_len);
	if (out_bytes < 0)
		return out_bytes;

	/* Prepare response buffer */
	in_bytes = prepare_proto3_response_buffer(proto3_response_size,
						  din_len);

	if (in_bytes < 0)
		return in_bytes;

	rv = cros_ec_bus->send_packet(cros_ec_bus, proto3_request, out_bytes,
				      proto3_response, in_bytes);

	if (rv < 0)
		return rv;

	/* Process the response */
	return handle_proto3_response(proto3_response, dinp, din_len);
}

static int send_command_proto3(int cmd, int cmd_version,
			       const void *dout, int dout_len,
			       void *dinp, int din_len)
{
	int rv;

	rv = send_command_proto3_work(cmd, cmd_version, dout, dout_len,
				      dinp, din_len);

	/* If the command doesn't complete, wait a while */
	if (rv == -EC_RES_IN_PROGRESS) {
		struct ec_response_get_comms_status resp;
		uint64_t start;

		/* Wait for command to complete */
		start = timer_us(0);
		do {
			int ret;

			mdelay(50);	/* Insert some reasonable delay */
			ret = send_command_proto3_work(EC_CMD_GET_COMMS_STATUS,
				0, NULL, 0, &resp, sizeof(resp));
			if (ret < 0)
				return ret;

			if (timer_us(start) > CROS_EC_CMD_TIMEOUT_MS * 1000) {
				printf("%s: Command %#02x timeout",
				      __func__, cmd);
				return -EC_RES_TIMEOUT;
			}
		} while (resp.flags & EC_COMMS_STATUS_PROCESSING);

		/* OK it completed, so read the status response */
		rv = send_command_proto3_work(EC_CMD_RESEND_RESPONSE,
			0, NULL, 0, dinp, din_len);
	}

	return rv;
}

/**
 * Send a command to the ChromeOS EC device and optionally return the reply.
 *
 * The device's internal input/output buffers are used.
 *
 * @param cmd		Command to send (EC_CMD_...)
 * @param cmd_version	Version of command to send (EC_VER_...)
 * @param dout          Output data (may be NULL If dout_len=0)
 * @param dout_len      Size of output data in bytes
 * @param din           Response data (may be NULL If din_len=0).
 * @param din_len       Maximum size of response in bytes
 * @return number of bytes in response, or -1 on error
 */
static int send_command_proto2(int cmd, int cmd_version,
			       const void *dout, int dout_len,
			       void *din, int din_len)
{
	int len;

	if (!cros_ec_bus) {
		printf("No ChromeOS EC bus configured.\n");
		return -1;
	}

	/* Proto2 can't send 16-bit command codes */
	if (cmd > 0xff)
		return -EC_RES_INVALID_COMMAND;

	len = cros_ec_bus->send_command(cros_ec_bus, cmd, cmd_version, dout,
					dout_len, din, din_len);

	/* If the command doesn't complete, wait a while */
	if (len == -EC_RES_IN_PROGRESS) {
		struct ec_response_get_comms_status resp;
		uint64_t start;

		/* Wait for command to complete */
		start = timer_us(0);
		do {
			int ret;

			mdelay(50);	/* Insert some reasonable delay */
			ret = cros_ec_bus->send_command(
				cros_ec_bus, EC_CMD_GET_COMMS_STATUS,
				0, NULL, 0, &resp, sizeof(resp));
			if (ret < 0)
				return ret;

			if (timer_us(start) > CROS_EC_CMD_TIMEOUT_MS * 1000) {
				printf("%s: Command %#02x timeout",
				      __func__, cmd);
				return -EC_RES_TIMEOUT;
			}
		} while (resp.flags & EC_COMMS_STATUS_PROCESSING);

		/* OK it completed, so read the status response */
		len = cros_ec_bus->send_command(
			cros_ec_bus, EC_CMD_RESEND_RESPONSE,
			0, NULL, 0, din, din_len);
	}

#ifdef DEBUG
	printf("%s: len=%d, din=%p\n", __func__, len, din);
#endif

	return len;
}

static int ec_command(int cmd, int cmd_version,
		      const void *dout, int dout_len,
		      void *din, int din_len)
{
	if (!initialized && cros_ec_init())
		return -1;

	assert(send_command_func);
	return send_command_func(cmd, cmd_version, dout, dout_len,
				 din, din_len);
}

int cros_ec_get_protocol_info(int devidx,
			      struct ec_response_get_protocol_info *info)
{
	if (ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) +
		       EC_CMD_GET_PROTOCOL_INFO, 0, NULL, 0, info,
		       sizeof(*info)) < sizeof(*info))
		return -1;

	return 0;
}

/**
 * Get the versions of the command supported by the EC.
 *
 * @param cmd		Command
 * @param pmask		Destination for version mask; will be set to 0 on
 *			error.
 * @return 0 if success, <0 if error
 */
static int cros_ec_get_cmd_versions(int devidx, int cmd, uint32_t *pmask)
{
	struct ec_params_get_cmd_versions p;
	struct ec_response_get_cmd_versions r;

	*pmask = 0;

	p.cmd = cmd;

	if (ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) + EC_CMD_GET_CMD_VERSIONS,
		       0, &p, sizeof(p), &r, sizeof(r)) < sizeof(r))
		return -1;

	*pmask = r.version_mask;
	return 0;
}

/**
 * Return non-zero if the EC supports the command and version
 *
 * @param cmd		Command to check
 * @param ver		Version to check
 * @return non-zero if command version supported; 0 if not.
 */
static int cros_ec_cmd_version_supported(int devidx, int cmd, int ver)
{
	uint32_t mask = 0;

	if (cros_ec_get_cmd_versions(devidx, cmd, &mask))
		return 0;

	return (mask & EC_VER_MASK(ver)) ? 1 : 0;
}

int cros_ec_scan_keyboard(struct cros_ec_keyscan *scan)
{
	if (ec_command(EC_CMD_MKBP_STATE, 0, NULL, 0, scan,
		       sizeof(*scan)) < sizeof(*scan))
		return -1;

	return 0;
}

int cros_ec_read_id(char *id, int maxlen)
{
	struct ec_response_get_version r;

	if (ec_command(EC_CMD_GET_VERSION, 0, NULL, 0, &r,
		       sizeof(r)) < sizeof(r))
		return -1;

	if (maxlen > sizeof(r.version_string_ro))
		maxlen = sizeof(r.version_string_ro);

	switch (r.current_image) {
	case EC_IMAGE_RO:
		memcpy(id, r.version_string_ro, maxlen);
		break;
	case EC_IMAGE_RW:
		memcpy(id, r.version_string_rw, maxlen);
		break;
	default:
		return -1;
	}

	id[maxlen - 1] = '\0';
	return 0;
}

int cros_ec_read_version(struct ec_response_get_version *versionp)
{
	if (ec_command(EC_CMD_GET_VERSION, 0, NULL, 0, versionp,
			sizeof(*versionp)) < sizeof(*versionp))
		return -1;

	return 0;
}

int cros_ec_read_build_info(char *strp)
{
	if (ec_command(EC_CMD_GET_BUILD_INFO, 0, NULL, 0, strp,
		       max_param_size) < 0)
		return -1;

	return 0;
}

int cros_ec_read_current_image(int devidx, enum ec_current_image *image)
{
	struct ec_response_get_version r;

	if (ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) + EC_CMD_GET_VERSION, 0,
		       NULL, 0, &r, sizeof(r)) < sizeof(r))
		return -1;

	*image = r.current_image;
	return 0;
}

int cros_ec_read_hash(int devidx, struct ec_response_vboot_hash *hash)
{
	struct ec_params_vboot_hash p;
	uint64_t start;
	int recalc_requested = 0;

	start = timer_us(0);
	do {
		/* Get hash if available. */
		p.cmd = EC_VBOOT_HASH_GET;
		if (ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) +
			       EC_CMD_VBOOT_HASH, 0, &p, sizeof(p),
			       hash, sizeof(*hash)) < 0)
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

			if (ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) +
				       EC_CMD_VBOOT_HASH, 0, &p,
				       sizeof(p), hash, sizeof(*hash)) < 0)
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

int cros_ec_reboot(int devidx, enum ec_reboot_cmd cmd, uint8_t flags)
{
	struct ec_params_reboot_ec p;

	p.cmd = cmd;
	p.flags = flags;

	if (ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) + EC_CMD_REBOOT_EC, 0,
		       &p, sizeof(p), NULL, 0) < 0)
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

void cros_ec_set_interrupt_gpio(GpioOps *gpio)
{
	die_if(cros_ec_interrupt_gpio, "EC interrupt GPIO already set.\n");
	cros_ec_interrupt_gpio = gpio;
}

int cros_ec_interrupt_pending(void)
{
	if (cros_ec_interrupt_gpio)
		return cros_ec_interrupt_gpio->get(cros_ec_interrupt_gpio);
	return 1;	// Always assume we have input if no GPIO set
}

int cros_ec_mkbp_info(struct ec_response_mkbp_info *info)
{
	if (ec_command(EC_CMD_MKBP_INFO, 0, NULL, 0, info,
			sizeof(*info)) < sizeof(info))
		return -1;

	return 0;
}

int cros_ec_get_host_events(uint32_t *events_ptr)
{
	struct ec_response_host_event_mask resp;

	/*
	 * Use the B copy of the event flags, because the main copy is already
	 * used by ACPI/SMI.
	 */
	if (ec_command(EC_CMD_HOST_EVENT_GET_B, 0, NULL, 0,
		       &resp, sizeof(resp)) < sizeof(resp))
		return -1;

	if (resp.mask & EC_HOST_EVENT_MASK(EC_HOST_EVENT_INVALID))
		return -1;

	*events_ptr = resp.mask;
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

int cros_ec_flash_protect(int devidx, uint32_t set_mask, uint32_t set_flags,
			  struct ec_response_flash_protect *resp)
{
	struct ec_params_flash_protect params;

	params.mask = set_mask;
	params.flags = set_flags;

	if (ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) + EC_CMD_FLASH_PROTECT,
		       EC_VER_FLASH_PROTECT,
		       &params, sizeof(params),
		       resp, sizeof(*resp)) < sizeof(*resp))
		return -1;

	return 0;
}

int cros_ec_entering_mode(int devidx, int mode)
{
	if (ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) + EC_CMD_ENTERING_MODE, 0,
		       &mode, sizeof(mode), NULL, 0))
		return -1;
	return 0;
}

int cros_ec_test(void)
{
	struct ec_params_hello req;
	struct ec_response_hello resp;

	req.in_data = 0x12345678;
	if (ec_command(EC_CMD_HELLO, 0, &req, sizeof(req),
		       &resp, sizeof(resp)) < sizeof(resp)) {
		printf("ec_command() returned error\n");
		return -1;
	}
	if (resp.out_data != req.in_data + 0x01020304) {
		printf("Received invalid handshake %x\n", resp.out_data);
		return -1;
	}

	return 0;
}

int cros_ec_flash_offset(int devidx, enum ec_flash_region region,
			 uint32_t *offset, uint32_t *size)
{
	struct ec_params_flash_region_info p;
	struct ec_response_flash_region_info r;
	int ret;

	p.region = region;
	ret = ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) +
			 EC_CMD_FLASH_REGION_INFO, EC_VER_FLASH_REGION_INFO,
			 &p, sizeof(p), &r, sizeof(r));
	if (ret != sizeof(r))
		return -1;

	if (offset)
		*offset = r.offset;
	if (size)
		*size = r.size;

	return 0;
}

int cros_ec_flash_erase(int devidx, uint32_t offset, uint32_t size)
{
	struct ec_params_flash_erase p;

	p.offset = offset;
	p.size = size;
	return ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) + EC_CMD_FLASH_ERASE,
			  0, &p, sizeof(p), NULL, 0);
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
static int cros_ec_flash_write_block(int devidx, const uint8_t *data,
				     uint32_t offset, uint32_t size)
{
	uint8_t *buf;
	struct ec_params_flash_write *p;
	uint32_t bufsize = sizeof(*p) + size;
	int rv;

	assert(data);

	/* Make sure request fits in the allowed packet size */
	if (bufsize > (devidx == 0 ? max_param_size : passthru_param_size))
		return -1;

	buf = xmalloc(bufsize);

	p = (struct ec_params_flash_write *)buf;
	p->offset = offset;
	p->size = size;
	memcpy(p + 1, data, size);

	rv = ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) + EC_CMD_FLASH_WRITE,
			0, buf, bufsize, NULL, 0) >= 0 ? 0 : -1;

	free(buf);

	return rv;
}

/**
 * Return optimal flash write burst size
 */
static int cros_ec_flash_write_burst_size(int devidx)
{
	struct ec_response_flash_info info;
	uint32_t pdata_max_size =
		(devidx == 0 ? max_param_size : passthru_param_size) -
		sizeof(struct ec_params_flash_write);

	/*
	 * Determine whether we can use version 1 of the command with more
	 * data, or only version 0.
	 */
	if (!cros_ec_cmd_version_supported(devidx, EC_CMD_FLASH_WRITE,
					   EC_VER_FLASH_WRITE))
		return EC_FLASH_WRITE_VER0_SIZE;

	/*
	 * Determine step size.  This must be a multiple of the write block
	 * size, and must also fit into the host parameter buffer.
	 */
	if (ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) + EC_CMD_FLASH_INFO, 0,
		       NULL, 0, &info, sizeof(info)) < sizeof(info))
		return 0;

	return (pdata_max_size / info.write_block_size) *
		info.write_block_size;
}

int cros_ec_flash_write(int devidx, const uint8_t *data, uint32_t offset,
			uint32_t size)
{
	uint32_t burst = cros_ec_flash_write_burst_size(devidx);
	uint32_t end, off;
	int ret;

	if (!burst)
		return -1;

	end = offset + size;
	for (off = offset; off < end; off += burst, data += burst) {
		uint32_t todo = MIN(end - off, burst);

		if (todo < burst) {
			uint8_t *buf = xmalloc(burst);
			memcpy(buf, data, todo);
			// Pad the buffer with a decent guess for erased data
			// value.
			memset(buf + todo, 0xff, burst - todo);
			ret = cros_ec_flash_write_block(devidx, buf,
							off, burst);
			free(buf);
		} else {
			ret = cros_ec_flash_write_block(devidx, data,
							off, burst);
		}
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
static int cros_ec_flash_read_block(int devidx, uint8_t *data, uint32_t offset,
				    uint32_t size)
{
	struct ec_params_flash_read p;

	p.offset = offset;
	p.size = size;

	return ec_command(EC_CMD_PASSTHRU_OFFSET(devidx) + EC_CMD_FLASH_READ, 0,
			  &p, sizeof(p), data, size) >= 0 ? 0 : -1;
}

int cros_ec_flash_read(int devidx, uint8_t *data, uint32_t offset,
		       uint32_t size)
{
	uint32_t burst = cros_ec_flash_write_burst_size(devidx);
	uint32_t end, off;
	int ret;

	end = offset + size;
	for (off = offset; off < end; off += burst, data += burst) {
		ret = cros_ec_flash_read_block(devidx, data, off,
					       MIN(end - off, burst));
		if (ret)
			return ret;
	}

	return 0;
}

int cros_ec_flash_update_rw(int devidx, const uint8_t *image, int image_size)
{
	uint32_t rw_offset, rw_size;
	int ret;

	if (cros_ec_flash_offset(devidx, EC_FLASH_REGION_RW,
				 &rw_offset, &rw_size))
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
	ret = cros_ec_flash_erase(devidx, rw_offset, rw_size);
	if (ret)
		return ret;

	/* Write the image */
	ret = cros_ec_flash_write(devidx, image, rw_offset, image_size);
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
			&p, sizeof(p), block, EC_VBNV_BLOCK_SIZE);
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

static int set_max_proto3_sizes(int request_size, int response_size,
				int passthru_size)
{
	free(proto3_request);
	free(proto3_response);

	if (request_size)
		proto3_request = xmalloc(request_size);
	else
		proto3_request = NULL;
	if (response_size)
		proto3_response = xmalloc(response_size);
	else
		proto3_response = NULL;

	proto3_request_size = request_size;
	proto3_response_size = response_size;

	max_param_size = proto3_request_size - sizeof(struct ec_host_request);

	passthru_param_size = passthru_size - sizeof(struct ec_host_request);
	if (passthru_param_size > max_param_size)
		passthru_param_size = max_param_size;

	return 0;
}


static int cros_ec_probe_passthru(void)
{
	struct ec_response_get_protocol_info infopd;

	if (!(CONFIG_DRIVER_EC_CROS_PASSTHRU))
		return 0;

	/* See if a PD chip is present */
	if (cros_ec_get_protocol_info(1, &infopd)) {
		infopd.max_request_packet_size = 0;
	} else {
		printf("%s: devidx=1 supported (%d, %d)\n",
		       __func__,
		       infopd.max_request_packet_size,
		       infopd.max_response_packet_size);
	}
	return infopd.max_request_packet_size;
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

	// Figure out what protocol version to use.

	if (cros_ec_bus->send_packet) {
		send_command_func = &send_command_proto3;
		if (set_max_proto3_sizes(DEFAULT_BUF_SIZE, DEFAULT_BUF_SIZE,
					 DEFAULT_BUF_SIZE))
			return -1;

		struct ec_response_get_protocol_info info;
		if (cros_ec_get_protocol_info(0, &info)) {
			set_max_proto3_sizes(0, 0, 0);
			send_command_func = NULL;
		} else {
			printf("%s: CrosEC protocol v3 supported (%d, %d)\n",
			       __func__,
			       info.max_request_packet_size,
			       info.max_response_packet_size);

			int passthru_size = cros_ec_probe_passthru();

			set_max_proto3_sizes(info.max_request_packet_size,
					     info.max_response_packet_size,
					     passthru_size);
		}
	}

	if (!send_command_func) {
		// Fall back to protocol version 2.
		send_command_func = &send_command_proto2;
		max_param_size = EC_PROTO2_MAX_PARAM_SIZE;
	}

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
