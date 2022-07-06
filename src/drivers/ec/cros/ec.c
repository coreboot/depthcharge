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
 */

/*
 * The Matrix Keyboard Protocol driver handles talking to the keyboard
 * controller chip. Mostly this is for keyboard functions, but some other
 * things have slipped in, so we provide generic services to talk to the
 * KBC.
 */

#include <assert.h>
#include <libpayload.h>
#include <vb2_api.h>

#include "base/cleanup_funcs.h"
#include "base/container_of.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/ec/cros/commands.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/power/power.h"


#define DEFAULT_BUF_SIZE 0x100

/* Timeout waiting for a flash erase command to complete */
static const int CROS_EC_CMD_TIMEOUT_MS = 5000;

/* Timeout waiting for EC hash calculation completion */
static const int CROS_EC_HASH_TIMEOUT_MS = 2000;

/* Time to delay between polling status of EC hash calculation */
static const int CROS_EC_HASH_CHECK_DELAY_MS = 10;

/* List of registered chip drivers to perform auxfw update */
ListNode ec_aux_fw_chip_list;

static int ec_init(CrosEc *me);

void cros_ec_dump_data(const char *name, int cmd, const void *data, int len)
{
#ifdef DEBUG
	const uint8_t *bytes = data;

	printf("%s: ", name);
	if (cmd != -1)
		printf("cmd=%#x: ", cmd);
	printf(" (%d/0x%x bytes)\n", len, len);
	hexdump(bytes, len);
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
				  uint8_t *dinp, int din_len)
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

	cros_ec_dump_data("in-data", -1, rs + 1, rs->data_len);

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

static int send_command_proto3_work(CrosEc *me, int cmd, int cmd_version,
				    const void *dout, int dout_len,
				    void *dinp, int din_len)
{
	int out_bytes, in_bytes;
	int rv;

	/* Create request packet */
	out_bytes = create_proto3_request(me->proto3_request,
					  me->proto3_request_size,
					  cmd, cmd_version, dout, dout_len);
	if (out_bytes < 0)
		return out_bytes;

	/* Prepare response buffer */
	in_bytes = prepare_proto3_response_buffer(me->proto3_response_size,
						  din_len);

	if (in_bytes < 0)
		return in_bytes;

	rv = me->bus->send_packet(me->bus, me->proto3_request, out_bytes,
				  me->proto3_response, in_bytes);

	if (rv < 0)
		return rv;

	/* Process the response */
	return handle_proto3_response(me->proto3_response, dinp, din_len);
}

static int send_command_proto3(CrosEc *me, int cmd, int cmd_version,
			       const void *dout, int dout_len,
			       void *dinp, int din_len)
{
	int rv;

	rv = send_command_proto3_work(me, cmd, cmd_version, dout, dout_len,
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
			ret = send_command_proto3_work(me,
				EC_CMD_GET_COMMS_STATUS, 0, NULL, 0,
				&resp, sizeof(resp));
			if (ret < 0)
				return ret;

			if (timer_us(start) > CROS_EC_CMD_TIMEOUT_MS * 1000) {
				printf("%s: Command %#02x timeout",
				      __func__, cmd);
				return -EC_RES_TIMEOUT;
			}
		} while (resp.flags & EC_COMMS_STATUS_PROCESSING);

		/* OK it completed, so read the status response */
		rv = send_command_proto3_work(me, EC_CMD_RESEND_RESPONSE,
			0, NULL, 0, dinp, din_len);
	}

	return rv;
}

int ec_command(CrosEc *me, int cmd, int cmd_version,
	       const void *dout, int dout_len,
	       void *din, int din_len)
{
	if (!me->initialized && ec_init(me))
		return -1;

	return send_command_proto3(me, cmd, cmd_version, dout, dout_len, din,
				   din_len);
}

CrosEc *cros_ec_get(void)
{
	VbootEcOps *ec = vboot_get_ec();
	return container_of(ec, CrosEc, vboot);
}

/**
 * Get the versions of the command supported by the EC.
 *
 * @param cmd		Command
 * @param pmask		Destination for version mask; will be set to 0 on
 *			error.
 * @return 0 if success, <0 if error
 */
static int get_cmd_versions(CrosEc *me, int cmd, uint32_t *pmask)
{
	struct ec_params_get_cmd_versions_v1 p;
	struct ec_response_get_cmd_versions r;

	*pmask = 0;

	p.cmd = cmd;

	if (ec_command(me, EC_CMD_GET_CMD_VERSIONS,
		       1, &p, sizeof(p), &r, sizeof(r)) != sizeof(r))
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
static int cmd_version_supported(CrosEc *me, int cmd, int ver)
{
	uint32_t mask = 0;

	if (get_cmd_versions(me, cmd, &mask))
		return 0;

	return (mask & EC_VER_MASK(ver)) ? 1 : 0;
}

static int cbi_get_uint32(uint32_t *id, uint32_t type)
{
	struct ec_params_get_cbi p = { 0 };

	p.tag = type;

	int rv = ec_command(cros_ec_get(), EC_CMD_GET_CROS_BOARD_INFO, 0,
			    &p, sizeof(p), id, sizeof(*id));

	return rv < 0 ? rv : 0;
}

int cros_ec_cbi_get_sku_id(uint32_t *id)
{
	return cbi_get_uint32(id, CBI_TAG_SKU_ID);
}

int cros_ec_cbi_get_oem_id(uint32_t *id)
{
	return cbi_get_uint32(id, CBI_TAG_OEM_ID);
}


int cros_ec_pd_control(uint8_t pd_port, enum ec_pd_control_cmd cmd)
{
	struct ec_params_pd_control p = {p.chip = pd_port, p.subcmd = cmd};

	int rv = ec_command(cros_ec_get(), EC_CMD_PD_CONTROL, 0, &p,
			    sizeof(p), NULL, 0);
	return rv < 0 ? rv : 0;
}

int cros_ec_scan_keyboard(struct cros_ec_keyscan *scan)
{
	if (ec_command(cros_ec_get(), EC_CMD_MKBP_STATE, 0, NULL, 0,
		       &scan->data, sizeof(scan->data)) != sizeof(scan->data))
		return -1;

	return 0;
}

int cros_ec_get_next_event(struct ec_response_get_next_event *e)
{
	int rv = ec_command(cros_ec_get(), EC_CMD_GET_NEXT_EVENT, 0, NULL,
			    0, e, sizeof(*e));

	return rv < 0 ? rv : 0;
}

static vb2_error_t vboot_running_rw(VbootEcOps *vbec, int *in_rw)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);
	struct ec_response_get_version r;

	if (ec_command(me, EC_CMD_GET_VERSION, 0, NULL, 0, &r, sizeof(r)) < 0)
		return VB2_ERROR_UNKNOWN;

	switch (r.current_image) {
	case EC_IMAGE_RO:
		*in_rw = 0;
		break;
	case EC_IMAGE_RW:
		*in_rw = 1;
		break;
	default:
		printf("Unrecognized EC image type %d.\n", r.current_image);
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

static uint32_t get_vboot_hash_offset(enum vb2_firmware_selection select)
{
	switch (select) {
	case VB_SELECT_FIRMWARE_READONLY:
		return EC_VBOOT_HASH_OFFSET_RO;
	case VB_SELECT_FIRMWARE_EC_UPDATE:
		return EC_VBOOT_HASH_OFFSET_UPDATE;
	default:
		return EC_VBOOT_HASH_OFFSET_ACTIVE;
	}
}

static enum ec_flash_region vboot_to_ec_region(
	enum vb2_firmware_selection select)
{
	switch (select) {
	case VB_SELECT_FIRMWARE_READONLY:
		return EC_FLASH_REGION_WP_RO;
	case VB_SELECT_FIRMWARE_EC_UPDATE:
		return EC_FLASH_REGION_UPDATE;
	default:
		return EC_FLASH_REGION_ACTIVE;
	}
}

static vb2_error_t vboot_hash_image(VbootEcOps *vbec,
				    enum vb2_firmware_selection select,
				    const uint8_t **hash, int *hash_size)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);
	static struct ec_response_vboot_hash resp;
	struct ec_params_vboot_hash p = { 0 };
	uint64_t start;
	int recalc_requested = 0;
	uint32_t hash_offset;

	hash_offset = get_vboot_hash_offset(select);

	start = timer_us(0);
	do {
		/* Get hash if available. */
		p.cmd = EC_VBOOT_HASH_GET;
		p.offset = hash_offset;
		if (ec_command(me, EC_CMD_VBOOT_HASH, 0, &p, sizeof(p),
			       &resp, sizeof(resp)) < 0)
			return VB2_ERROR_UNKNOWN;

		switch (resp.status) {
		case EC_VBOOT_HASH_STATUS_NONE:
			/* We have no valid hash - let's request a recalc
			 * if we haven't done so yet. */
			if (recalc_requested != 0)
				break;
			printf("%s: No valid hash (status=%d size=%d). "
			      "Compute one...\n", __func__, resp.status,
			      resp.size);

			p.cmd = EC_VBOOT_HASH_START;
			p.hash_type = EC_VBOOT_HASH_TYPE_SHA256;
			p.nonce_size = 0;

			if (ec_command(me, EC_CMD_VBOOT_HASH, 0, &p,
				       sizeof(p), &resp, sizeof(resp)) < 0)
				return VB2_ERROR_UNKNOWN;

			recalc_requested = 1;
			/* Expect status to be busy (and don't break while)
			 * since we just sent a recalc request. */
			resp.status = EC_VBOOT_HASH_STATUS_BUSY;
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
	} while (resp.status == EC_VBOOT_HASH_STATUS_BUSY &&
		 timer_us(start) < CROS_EC_HASH_TIMEOUT_MS * 1000);

	if (resp.status != EC_VBOOT_HASH_STATUS_DONE) {
		printf("%s: Hash status not done: %d\n", __func__,
		      resp.status);
		return VB2_ERROR_UNKNOWN;
	}
	if (resp.hash_type != EC_VBOOT_HASH_TYPE_SHA256) {
		printf("EC hash was the wrong type.\n");
		return VB2_ERROR_UNKNOWN;
	}

	*hash = resp.hash_digest;
	*hash_size = resp.digest_size;

	return VB2_SUCCESS;
}

/**
 * Run internal tests on the ChromeOS EC interface.
 *
 * @return 0 if ok, <0 if the test failed
 */
static int ec_test(CrosEc *me)
{
	struct ec_params_hello req;
	struct ec_response_hello resp;

	req.in_data = 0x12345678;
	if (ec_command(me, EC_CMD_HELLO, 0, &req, sizeof(req),
		       &resp, sizeof(resp)) != sizeof(resp))
		return -1;
	if (resp.out_data != req.in_data + 0x01020304)
		return -1;

	return 0;
}

static int ec_reboot(CrosEc *me, enum ec_reboot_cmd cmd, uint8_t flags)
{
	struct ec_params_reboot_ec p;

	p.cmd = cmd;
	p.flags = flags;

	if (cmd == EC_REBOOT_COLD && !(flags & EC_REBOOT_FLAG_ON_AP_SHUTDOWN))
		run_cleanup_funcs(CleanupOnReboot);

	if (ec_command(me, EC_CMD_REBOOT_EC, 0,
		       &p, sizeof(p), NULL, 0) < 0)
		return -1;

	/* Do we expect our command to immediately reboot the EC? */
	if (cmd != EC_REBOOT_DISABLE_JUMP &&
	    !(flags & EC_REBOOT_FLAG_ON_AP_SHUTDOWN)) {
		/*
		 * EC reboot will take place immediately so delay to allow it
		 * to complete.  Note that some reboot types (EC_REBOOT_COLD)
		 * will reboot the AP as well, in which case we won't actually
		 * get to this point.
		 */
		mdelay(50);	// default delay we shall wait after EC reboot
		uint64_t start = timer_us(0);
		while (ec_test(me)) {
			if (timer_us(start) > 3 * 1000 * 1000) {
				printf("EC did not return from reboot.\n");
				return -1;
			}
			mdelay(5);	// avoid spamming bus too hard
		}
		printf("EC returned from reboot after %lluus\n",
		       timer_us(start));
	}

	return 0;
}

static vb2_error_t vboot_reboot_to_ro(VbootEcOps *vbec)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);

	if (ec_reboot(me, EC_REBOOT_COLD, EC_REBOOT_FLAG_ON_AP_SHUTDOWN) < 0) {
		printf("Failed to schedule EC reboot to RO.\n");
		return VB2_ERROR_UNKNOWN;
	}

	power_off();
}

static vb2_error_t vboot_jump_to_rw(VbootEcOps *vbec)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);

	if (ec_reboot(me, EC_REBOOT_JUMP_RW, 0) < 0) {
		printf("Failed to make the EC jump to RW.\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

static vb2_error_t vboot_reboot_switch_rw(VbootEcOps *vbec)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);

	if (ec_reboot(me, EC_REBOOT_COLD, EC_REBOOT_FLAG_SWITCH_RW_SLOT) < 0) {
		printf("Failed to reboot the EC to switch RW slot.\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

static vb2_error_t vboot_disable_jump(VbootEcOps *vbec)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);

	if (ec_reboot(me, EC_REBOOT_DISABLE_JUMP, 0) < 0) {
		printf("Failed to make the EC disable jumping.\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

int cros_ec_interrupt_pending(void)
{
	if (cros_ec_get()->interrupt_gpio)
		return gpio_get(cros_ec_get()->interrupt_gpio);
	return -1;
}

int cros_ec_mkbp_info(struct ec_response_mkbp_info *info)
{
	if (ec_command(cros_ec_get(), EC_CMD_MKBP_INFO, 0, NULL, 0,
		       info, sizeof(*info)) != sizeof(*info))
		return -1;

	return 0;
}

int cros_ec_get_event_mask(u8 type, uint32_t *mask)
{
	struct ec_response_host_event_mask rsp;

	if (ec_command(cros_ec_get(), type, 0, NULL, 0,
		       &rsp, sizeof(rsp)) != sizeof(rsp))
		return -1;

	*mask = rsp.mask;

	return 0;
}

int cros_ec_set_event_mask(u8 type, uint32_t mask)
{
	struct ec_params_host_event_mask req;

	req.mask = mask;

	if (ec_command(cros_ec_get(), type, 0, &req, sizeof(req),
		       NULL, 0) < 0)
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
	if (ec_command(cros_ec_get(), EC_CMD_HOST_EVENT_GET_B, 0, NULL, 0,
		       &resp, sizeof(resp)) != sizeof(resp))
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
	if (ec_command(cros_ec_get(), EC_CMD_HOST_EVENT_CLEAR_B, 0,
		       &params, sizeof(params), NULL, 0) < 0)
		return -1;

	return 0;
}

static int ec_flash_protect(CrosEc *me, uint32_t set_mask, uint32_t set_flags,
			    struct ec_response_flash_protect *resp)
{
	struct ec_params_flash_protect params;

	params.mask = set_mask;
	params.flags = set_flags;

	if (ec_command(me, EC_CMD_FLASH_PROTECT, EC_VER_FLASH_PROTECT,
		       &params, sizeof(params),
		       resp, sizeof(*resp)) != sizeof(*resp))
		return -1;

	return 0;
}

/**
 * Obtain position and size of a flash region
 *
 * @param region	Flash region to query
 * @param offset	Returns offset of flash region in EC flash
 * @param size		Returns size of flash region
 * @return 0 if ok, -1 on error
 */
static int ec_flash_offset(CrosEc *me, enum ec_flash_region region,
			   uint32_t *offset, uint32_t *size)
{
	struct ec_params_flash_region_info p;
	struct ec_response_flash_region_info r;
	int ret;

	p.region = region;
	ret = ec_command(me,
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

static int ec_flash_erase(CrosEc *me, uint32_t offset, uint32_t size)
{
	struct ec_params_flash_erase p;

	p.offset = offset;
	p.size = size;
	return ec_command(me, EC_CMD_FLASH_ERASE,
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
static int ec_flash_write_block(CrosEc *me, const uint8_t *data,
				uint32_t offset, uint32_t size)
{
	uint8_t *buf;
	struct ec_params_flash_write *p;
	uint32_t bufsize = sizeof(*p) + size;
	int rv;

	assert(data);

	/* Make sure request fits in the allowed packet size */
	if (bufsize > me->max_param_size)
		return -1;

	buf = xmalloc(bufsize);

	p = (struct ec_params_flash_write *)buf;
	p->offset = offset;
	p->size = size;
	memcpy(p + 1, data, size);

	rv = ec_command(me, EC_CMD_FLASH_WRITE,
			0, buf, bufsize, NULL, 0) >= 0 ? 0 : -1;

	free(buf);

	return rv;
}

/**
 * Return optimal flash write burst size
 */
static int ec_flash_write_burst_size(CrosEc *me)
{
	struct ec_response_flash_info info;
	uint32_t pdata_max_size = me->max_param_size -
		sizeof(struct ec_params_flash_write);

	/*
	 * Determine whether we can use version 1 of the command with more
	 * data, or only version 0.
	 */
	if (!cmd_version_supported(me, EC_CMD_FLASH_WRITE, EC_VER_FLASH_WRITE))
		return EC_FLASH_WRITE_VER0_SIZE;

	/*
	 * Determine step size.  This must be a multiple of the write block
	 * size, and must also fit into the host parameter buffer.
	 */
	if (ec_command(me, EC_CMD_FLASH_INFO, 0,
		       NULL, 0, &info, sizeof(info)) != sizeof(info))
		return 0;

	return (pdata_max_size / info.write_block_size) *
		info.write_block_size;
}

/**
 * Write data to the flash
 *
 * Write an arbitrary amount of data to the EC flash, by repeatedly writing
 * small blocks.
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
static int ec_flash_write(CrosEc *me, const uint8_t *data, uint32_t offset,
			uint32_t size)
{
	uint32_t burst = ec_flash_write_burst_size(me);
	uint32_t end, off;
	int ret;

	if (!burst)
		return -1;

	end = offset + size;
	for (off = offset; off < end; off += burst, data += burst) {
		uint32_t todo = MIN(end - off, burst);
		/* If SPI flash needs to add padding to make a legitimate write
		 * block, do so on EC. */
		ret = ec_flash_write_block(me, data, off, todo);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * Run verification on a slot
 *
 * @param me     CrosEc instance
 * @param region Region to run verification on
 * @return 0 if success or not applicable. Non-zero if verification failed.
 */
static int ec_efs_verify(CrosEc *me, enum ec_flash_region region)
{
	struct ec_params_efs_verify p;
	int rv;
	p.region = region;

	rv = ec_command(me, EC_CMD_EFS_VERIFY, 0, &p, sizeof(p), NULL, 0);
	if (rv >= 0) {
		printf("EFS: Verification success\n");
		return 0;
	}

	printf("EFS: Verification failed for %d\n", rv);
	return rv;
}

static vb2_error_t vboot_set_region_protection(
	CrosEc *me, enum vb2_firmware_selection select, int enable)
{
	struct ec_response_flash_protect resp;
	uint32_t protected_region = EC_FLASH_PROTECT_ALL_NOW;
	uint32_t mask = EC_FLASH_PROTECT_ALL_NOW | EC_FLASH_PROTECT_ALL_AT_BOOT;

	if (select == VB_SELECT_FIRMWARE_READONLY)
		protected_region = EC_FLASH_PROTECT_RO_NOW;

	/* Update protection */
	if (ec_flash_protect(me, mask, enable ? mask : 0, &resp) < 0) {
		printf("Failed to update EC flash protection.\n");
		return VB2_ERROR_UNKNOWN;
	}

	if (!enable) {
		/* If protection is still enabled, need reboot */
		if (resp.flags & protected_region)
			return VB2_REQUEST_REBOOT_EC_TO_RO;

		return VB2_SUCCESS;
	}

	/*
	 * If write protect and ro-at-boot aren't both asserted, don't expect
	 * protection enabled.
	 */
	if ((~resp.flags) & (EC_FLASH_PROTECT_GPIO_ASSERTED |
			     EC_FLASH_PROTECT_RO_AT_BOOT))
		return VB2_SUCCESS;

	/* If flash is protected now, success */
	if (resp.flags & EC_FLASH_PROTECT_ALL_NOW)
		return VB2_SUCCESS;

	/* If RW will be protected at boot but not now, need a reboot */
	if (resp.flags & EC_FLASH_PROTECT_ALL_AT_BOOT)
		return VB2_REQUEST_REBOOT_EC_TO_RO;

	/* Otherwise, it's an error */
	return VB2_ERROR_UNKNOWN;
}

static vb2_error_t vboot_update_image(VbootEcOps *vbec,
				      enum vb2_firmware_selection select,
				      const uint8_t *image, int image_size)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);
	uint32_t region_offset, region_size;
	enum ec_flash_region region = vboot_to_ec_region(select);
	vb2_error_t rv = vboot_set_region_protection(me, select, 0);
	if (rv == VB2_REQUEST_REBOOT_EC_TO_RO || rv != VB2_SUCCESS)
		return rv;

	if (ec_flash_offset(me, region, &region_offset, &region_size))
		return VB2_ERROR_UNKNOWN;
	if (image_size > region_size)
		return VB2_ERROR_INVALID_PARAMETER;

	/*
	 * Erase the entire region, so that the EC doesn't see any garbage
	 * past the new image if it's smaller than the current image.
	 *
	 * TODO: could optimize this to erase just the current image, since
	 * presumably everything past that is 0xff's.  But would still need to
	 * round up to the nearest multiple of erase size.
	 */
	if (ec_flash_erase(me, region_offset, region_size))
		return VB2_ERROR_UNKNOWN;

	/* Write the image */
	if (ec_flash_write(me, image, region_offset, image_size))
		return VB2_ERROR_UNKNOWN;

	/* Verify the image */
	if (CONFIG(EC_EFS) && ec_efs_verify(me, region))
		return VB2_ERROR_UNKNOWN;

	return VB2_SUCCESS;
}

static vb2_error_t vboot_protect(VbootEcOps *vbec,
				 enum vb2_firmware_selection select)
{
	CrosEc *me = container_of(vbec, CrosEc, vboot);
	return vboot_set_region_protection(me, select, 1);
}

int cros_ec_battery_cutoff(uint8_t flags)
{
	struct ec_params_battery_cutoff p;
	int len;

	p.flags = flags;

	len = ec_command(cros_ec_get(), EC_CMD_BATTERY_CUT_OFF, 1,
			 &p, sizeof(p), NULL, 0);

	if (len < 0)
		return -1;
	return 0;
}

static vb2_error_t vboot_battery_cutoff(VbootEcOps *vbec)
{
	if (cros_ec_battery_cutoff(EC_BATTERY_CUTOFF_FLAG_AT_SHUTDOWN) < 0)
		return VB2_ERROR_UNKNOWN;
	return VB2_SUCCESS;
}

int cros_ec_set_motion_sense_activity(uint32_t activity, uint32_t value)
{
	struct ec_params_motion_sense params;
	struct ec_response_motion_sense resp;

	params.cmd = MOTIONSENSE_CMD_SET_ACTIVITY;
	params.set_activity.activity = activity;
	params.set_activity.enable = value;

	if (ec_command(cros_ec_get(), EC_CMD_MOTION_SENSE_CMD, 2,
		       &params, sizeof(params), &resp, sizeof(resp)) < 0)
		return -1;

	return 0;
}

static int read_memmap(uint8_t offset, uint8_t size, void *dest)
{
	struct ec_params_read_memmap params;

	params.offset = offset;
	params.size = size;

	CrosEc *ec = cros_ec_get();

	if (ec->bus->read)
		ec->bus->read(dest, EC_LPC_ADDR_MEMMAP + offset, size);
	else if (ec_command(ec, EC_CMD_READ_MEMMAP, 0, &params, sizeof(params),
			    dest, size) < 0)
		return -1;

	return 0;
}

int cros_ec_read_batt_volt(uint32_t *volt)
{
	return read_memmap(EC_MEMMAP_BATT_VOLT, sizeof(*volt), volt);
}

int cros_ec_read_lid_switch(uint32_t *lid)
{
	uint8_t flags, version;

	if (read_memmap(EC_MEMMAP_SWITCHES_VERSION, sizeof(version), &version))
		return -1;

	// Switch data is not initialized
	if (!version)
		return -1;

	if (read_memmap(EC_MEMMAP_SWITCHES, sizeof(flags), &flags))
		return -1;

	*lid = !!(flags & EC_SWITCH_LID_OPEN);

	return 0;
}

/**
 * Read the lid switch value from the EC
 *
 * @return 0 if lid closed, 1 if lid open or unable to read
 */
static int get_lid_switch_from_ec(GpioOps *me)
{
	uint32_t lid_open;

	if (!cros_ec_read_lid_switch(&lid_open))
		return lid_open;

	/* Assume the lid is open if we get any sort of error */
	printf("error, assuming lid is open\n");
	return 1;
}

GpioOps *cros_ec_lid_switch_flag(void)
{
	GpioOps *ops = xzalloc(sizeof(*ops));

	ops->get = &get_lid_switch_from_ec;
	return ops;
}

int cros_ec_read_power_btn(uint32_t *pwr_btn)
{
	uint8_t flags, version;

	if (read_memmap(EC_MEMMAP_SWITCHES_VERSION, sizeof(version), &version))
		return -1;

	// Switch data is not initialized
	if (!version)
		return -1;

	if (read_memmap(EC_MEMMAP_SWITCHES, sizeof(flags), &flags))
		return -1;

	*pwr_btn = !!(flags & EC_SWITCH_POWER_BUTTON_PRESSED);

	return 0;
}

/**
 * Read the power button value from the EC
 *
 * @return 1 if button is pressed, 0 in not pressed or unable to read
 */
static int get_power_btn_from_ec(GpioOps *me)
{
	uint32_t pwr_btn;

	if (!cros_ec_read_power_btn(&pwr_btn))
		return pwr_btn;

	/* Assume power button is not pressed if we get any sort of error */
	printf("error, assuming power button not pressed\n");
	return 0;
}

GpioOps *cros_ec_power_btn_flag(void)
{
	GpioOps *ops = xzalloc(sizeof(*ops));

	ops->get = &get_power_btn_from_ec;
	return ops;
}

int cros_ec_config_powerbtn(uint32_t flags)
{
	struct ec_params_config_power_button params;

	params.flags = flags;
	if (ec_command(cros_ec_get(), EC_CMD_CONFIG_POWER_BUTTON, 0,
		       &params, sizeof(params), NULL, 0) < 0)
		return -1;

	return 0;
}

int cros_ec_read_limit_power_request(int *limit_power)
{
	struct ec_params_charge_state p;
	struct ec_response_charge_state r;
	int res;

	p.cmd = CHARGE_STATE_CMD_GET_PARAM;
	p.get_param.param = CS_PARAM_LIMIT_POWER;
	res = ec_command(cros_ec_get(), EC_CMD_CHARGE_STATE, 0,
			 &p, sizeof(p), &r, sizeof(r));

	/*
	 * If our EC doesn't support the LIMIT_POWER parameter, assume that
	 * LIMIT_POWER is not requested.
	 */
	if (res == -EC_RES_INVALID_PARAM || res == -EC_RES_INVALID_COMMAND) {
		printf("PARAM_LIMIT_POWER not supported by EC.\n");
		*limit_power = 0;
		return 0;
	}

	if (res != sizeof(r.get_param))
		return -1;

	*limit_power = r.get_param.value;
	return 0;
}

static vb2_error_t vboot_check_limit_power(VbootEcOps *vbec, int *limit_power)
{
	if (cros_ec_read_limit_power_request(limit_power) < 0)
		return VB2_ERROR_UNKNOWN;
	return VB2_SUCCESS;
}

static vb2_error_t vboot_enable_power_button(VbootEcOps *vbec, int enable)
{
	uint32_t flags = 0;

	if (enable)
		flags = EC_POWER_BUTTON_ENABLE_PULSE;

	if (cros_ec_config_powerbtn(flags) < 0)
		return VB2_ERROR_UNKNOWN;
	return VB2_SUCCESS;
}

static vb2_error_t vboot_protect_tcpc_ports(VbootEcOps *vbec)
{
	CrosEc *cros_ec = container_of(vbec, CrosEc, vboot);

	if (!CONFIG(DRIVER_BUS_I2C_CROS_EC_TUNNEL))
		return VB2_SUCCESS;

	int ret = cros_ec_tunnel_i2c_protect_tcpc_ports(cros_ec);
	if (ret == -EC_RES_INVALID_COMMAND) {
		printf("EC does not support TCPC sync in RW... ignoring.\n");
	} else if (ret < 0) {
		printf("ERROR: cannot protect TCPC I2C tunnels\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

int cros_ec_read_batt_state_of_charge(uint32_t *state)
{
	struct ec_params_charge_state params;
	struct ec_response_charge_state resp;

	params.cmd = CHARGE_STATE_CMD_GET_STATE;

	if (ec_command(cros_ec_get(), EC_CMD_CHARGE_STATE, 0,
		       &params, sizeof(params), &resp, sizeof(resp)) < 0)
		return -1;

	*state = resp.get_state.batt_state_of_charge;
	return 0;
}

int cros_ec_reboot(uint8_t flags)
{
	return ec_reboot(cros_ec_get(), EC_REBOOT_COLD, flags);
}

/*
 * Set backlight.  Note that duty value needs to be passed
 * to the EC as a 16 bit number for increased precision.
 */
int cros_ec_set_bl_pwm_duty(uint32_t percent)
{
	struct ec_params_pwm_set_duty params;

	params.duty = (percent * EC_PWM_MAX_DUTY)/100;
	params.pwm_type = EC_PWM_TYPE_DISPLAY_LIGHT;
	params.index = 0;

	if (ec_command(cros_ec_get(), EC_CMD_PWM_SET_DUTY, 0,
		       &params, sizeof(params), NULL, 0) < 0)
		return -1;
	return 0;
}

int cros_ec_locate_tcpc_chip(uint8_t port, struct ec_response_locate_chip *r)
{
	struct ec_params_locate_chip p;
	int ret;

	p.type = EC_CHIP_TYPE_TCPC;
	p.index = port;
	ret = ec_command(cros_ec_get(), EC_CMD_LOCATE_CHIP, 0,
				&p, sizeof(p), r, sizeof(*r));
	if (ret < 0) {
		printf("Failed to locate TCPC chip for port%d ret:%d\n",
								port, ret);
		return ret;
	}

	return 0;
}

int cros_ec_get_usb_pd_mux_info(int port, uint8_t *mux_state)
{
	struct ec_params_usb_pd_mux_info req;
	struct ec_response_usb_pd_mux_info resp;
	int ret;

	req.port = port;

	ret = ec_command(cros_ec_get(),
			 EC_CMD_USB_PD_MUX_INFO, 0,
			 &req, sizeof(req),
			 &resp, sizeof(resp));
	if (ret < 0) {
		printf("Failed to get PD_MUX_INFO port%d ret:%d\n",
		       port, ret);
		return ret;
	}

	*mux_state = resp.flags;

	return 0;
}

int cros_ec_get_usb_pd_control(int port, int *ufp, int *dbg_acc)
{
	struct ec_params_usb_pd_control pd_control;
	struct ec_response_usb_pd_control_v2 resp;
	int ret;

	pd_control.port = port;
	pd_control.role = USB_PD_CTRL_ROLE_NO_CHANGE;
	pd_control.mux = USB_PD_CTRL_MUX_NO_CHANGE;
	pd_control.swap = USB_PD_CTRL_SWAP_NONE;

	ret = ec_command(cros_ec_get(),
			 EC_CMD_USB_PD_CONTROL, 2,
			 &pd_control, sizeof(pd_control),
			 &resp, sizeof(resp));
	if (ret < 0) {
		printf("Failed to get PD_CONTROLv2 port%d ret:%d\n",
		       port, ret);
		return ret;
	}

	*ufp = !(resp.role & PD_CTRL_RESP_ROLE_DATA);
	*dbg_acc = (resp.cc_state == PD_CC_UFP_DEBUG_ACC ||
		    resp.cc_state == PD_CC_DFP_DEBUG_ACC);

	return 0;
}

static int set_max_proto3_sizes(CrosEc *me, int request_size, int response_size)
{
	free(me->proto3_request);
	free(me->proto3_response);

	if (request_size)
		me->proto3_request = xmalloc(request_size);
	else
		me->proto3_request = NULL;
	if (response_size)
		me->proto3_response = xmalloc(response_size);
	else
		me->proto3_response = NULL;

	me->proto3_request_size = request_size;
	me->proto3_response_size = response_size;

	me->max_param_size = me->proto3_request_size -
			     sizeof(struct ec_host_request);

	return 0;
}

static int ec_init(CrosEc *me)
{
	if (me->initialized)
		return 0;

	if (!me->bus) {
		printf("No ChromeOS EC bus configured.\n");
		return -1;
	}

	if (me->bus->init && me->bus->init(me->bus))
		return -1;

	me->initialized = 1;

	if (set_max_proto3_sizes(me, DEFAULT_BUF_SIZE,
				 DEFAULT_BUF_SIZE))
		return -1;

	struct ec_response_get_protocol_info info;
	if (ec_command(me, EC_CMD_GET_PROTOCOL_INFO, 0,
		       NULL, 0, &info, sizeof(info)) != sizeof(info)) {
		printf("ERROR: Cannot read EC protocol info!\n");
		return -1;
	}
	printf("%s: CrosEC protocol v3 supported (%d, %d)\n",
	       __func__,
	       info.max_request_packet_size,
	       info.max_response_packet_size);
	set_max_proto3_sizes(me, info.max_request_packet_size,
			     info.max_response_packet_size);

	return 0;
}

CrosEc *new_cros_ec(CrosEcBusOps *bus, GpioOps *interrupt_gpio)
{
	CrosEc *me = xzalloc(sizeof(*me));

	me->bus = bus;
	me->interrupt_gpio = interrupt_gpio;

	me->vboot.running_rw = vboot_running_rw;
	me->vboot.jump_to_rw = vboot_jump_to_rw;
	me->vboot.disable_jump = vboot_disable_jump;
	me->vboot.hash_image = vboot_hash_image;
	me->vboot.update_image = vboot_update_image;
	me->vboot.protect = vboot_protect;
	me->vboot.reboot_to_ro = vboot_reboot_to_ro;
	me->vboot.reboot_switch_rw = vboot_reboot_switch_rw;
	me->vboot.battery_cutoff = vboot_battery_cutoff;
	me->vboot.check_limit_power = vboot_check_limit_power;
	me->vboot.enable_power_button = vboot_enable_power_button;
	me->vboot.protect_tcpc_ports = vboot_protect_tcpc_ports;

	return me;
}

/**
 * Probe EC to gather chip info that require FW update
 */
void cros_ec_probe_aux_fw_chips(void)
{
	CrosEc *cros_ec = cros_ec_get();
	struct ec_response_usb_pd_ports usb_pd_ports_r;
	struct ec_params_pd_chip_info pd_chip_p = {0};
	struct ec_response_pd_chip_info pd_chip_r = {0};
	int ret;
	uint8_t i;
	CrosEcAuxfwChipInfo *chip;
	const VbootAuxfwOps *ops;

	/* List is empty, no need to probe EC */
	if (!ec_aux_fw_chip_list.next)
		return;

	ret = ec_command(cros_ec, EC_CMD_USB_PD_PORTS, 0, NULL, 0,
				&usb_pd_ports_r, sizeof(usb_pd_ports_r));
	if (ret < 0) {
		printf("%s: Cannot resolve # of USB PD ports\n", __func__);
		return;
	}

	/*
	 * Iterate through the number of ports, get PD chip info,
	 * and get the VbootAuxfw operations for that chip.
	 */
	for (i = 0; i < usb_pd_ports_r.num_ports; i++) {
		/* Get the USB PD Chip info */
		pd_chip_p.port = i;
		pd_chip_p.live = 0;
		ret = ec_command(cros_ec, EC_CMD_PD_CHIP_INFO, 0,
					&pd_chip_p, sizeof(pd_chip_p),
					&pd_chip_r, sizeof(pd_chip_r));
		if (ret < 0) {
			printf("%s: Cannot get PD port%d info - %d\n",
					__func__, i, ret);
			continue;
		}

		list_for_each(chip, ec_aux_fw_chip_list, list_node) {
			if (pd_chip_r.vendor_id != chip->vid ||
			    pd_chip_r.product_id != chip->pid)
				continue;

			ops = chip->new_chip_aux_fw_ops(&pd_chip_r, i);
			if (ops)
				register_vboot_auxfw(ops);
			break;
		}
	}
}
