/*
 * Chromium OS EC driver
 *
 * Copyright 2012 Google LLC
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
#include "drivers/ec/cros/commands_api.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/power/power.h"


#define DEFAULT_BUF_SIZE 0x100

/* Maximum wait time for EC flash erase completion */
static const int CROS_EC_ERASE_TIMEOUT_MS = 30000;

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
		memcpy(dinp, rs + 1, rs->data_len);

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

		if (cmd != EC_CMD_FLASH_ERASE) {
			printf("%s: command %#02x unexpectedly returned "
			       "IN_PROGRESS\n",
			       __func__, cmd);
		}
		/* Wait for command to complete */
		start = timer_us(0);
		while (1) {
			int ret;
			uint64_t elapsed_us;

			mdelay(50);	/* Insert some reasonable delay */
			ret = send_command_proto3_work(me,
				EC_CMD_GET_COMMS_STATUS, 0, NULL, 0,
				&resp, sizeof(resp));
			if (ret < 0)
				return ret;

			elapsed_us = timer_us(start);
			if ((resp.flags & EC_COMMS_STATUS_PROCESSING) == 0) {
				printf("%s: command %#02x completed in %lld us\n",
				       __func__, cmd, elapsed_us);
				break;
			}
			if (elapsed_us > (CROS_EC_ERASE_TIMEOUT_MS * 1000)) {
				printf("%s: command %#02x timeout\n", __func__,
				       cmd);
				return -EC_RES_TIMEOUT;
			}
		}

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
static int get_cmd_versions(int cmd, uint32_t *pmask)
{
	struct ec_params_get_cmd_versions_v1 p;
	struct ec_response_get_cmd_versions r;

	*pmask = 0;

	p.cmd = cmd;

	if (ec_cmd_get_cmd_versions_v1(cros_ec_get(), &p, &r) != sizeof(r))
		return -1;

	*pmask = r.version_mask;
	return 0;
}

int cros_ec_cmd_version_supported(int cmd, int ver)
{
	uint32_t mask = 0;

	if (get_cmd_versions(cmd, &mask))
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

	int rv = ec_cmd_pd_control(cros_ec_get(), &p);
	return rv < 0 ? rv : 0;
}

int cros_ec_i2c_get_speed(uint8_t i2c_port, uint16_t *speed_khz)
{
	const struct ec_params_i2c_control p = {
		.port = i2c_port,
		.cmd = EC_I2C_CONTROL_GET_SPEED,
	};
	struct ec_response_i2c_control r;
	int rv;

	rv = ec_cmd_i2c_control(cros_ec_get(), &p, &r);
	if (rv < 0)
		return rv;
	if (rv != sizeof(r))
		return -1;

	*speed_khz = r.cmd_response.speed_khz;
	return 0;
}

int cros_ec_i2c_set_speed(uint8_t i2c_port,
			  uint16_t new_speed_khz,
			  uint16_t *old_speed_khz)
{
	const struct ec_params_i2c_control p = {
		.port = i2c_port,
		.cmd = EC_I2C_CONTROL_SET_SPEED,
		.cmd_params.speed_khz = new_speed_khz,
	};
	struct ec_response_i2c_control r;
	int rv;

	rv = ec_cmd_i2c_control(cros_ec_get(), &p, &r);
	if (rv < 0)
		return rv;
	if (rv != sizeof(r))
		return -1;

	if (old_speed_khz)
		*old_speed_khz = r.cmd_response.speed_khz;
	return 0;
}

int cros_ec_scan_keyboard(struct cros_ec_keyscan *scan)
{
	if (ec_command(cros_ec_get(), EC_CMD_MKBP_STATE, 0, NULL, 0,
		       &scan->data, sizeof(scan->data)) != sizeof(scan->data))
		return -1;

	return 0;
}

int cros_ec_get_next_event(struct ec_response_get_next_event_v1 *e)
{
	return ec_cmd_get_next_event_v2(cros_ec_get(), e);
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
	if (ec_cmd_hello(cros_ec_get(), &req, &resp) != sizeof(resp))
		return -1;
	if (resp.out_data != req.in_data + 0x01020304)
		return -1;

	return 0;
}

int cros_ec_reboot_param(CrosEc *me, enum ec_reboot_cmd cmd, uint8_t flags)
{
	struct ec_params_reboot_ec p;

	p.cmd = cmd;
	p.flags = flags;

	if (cmd == EC_REBOOT_COLD && !(flags & EC_REBOOT_FLAG_ON_AP_SHUTDOWN))
		run_cleanup_funcs(CleanupOnReboot);

	if (ec_cmd_reboot_ec(cros_ec_get(), &p) < 0)
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
		uint64_t start = timer_us(0);
		mdelay(CONFIG_DRIVER_EC_CROS_DELAY_AFTER_EC_REBOOT_MS);
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
	if (ec_cmd_host_event_get_b(cros_ec_get(), &resp) != sizeof(resp))
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
	if (ec_cmd_host_event_clear_b(cros_ec_get(), &params) < 0)
		return -1;

	return 0;
}

int cros_ec_battery_cutoff(uint8_t flags)
{
	struct ec_params_battery_cutoff p;
	int len;

	p.flags = flags;

	len = ec_cmd_battery_cut_off_v1(cros_ec_get(), &p);

	if (len < 0)
		return -1;
	return 0;
}

int cros_ec_set_motion_sense_activity(uint32_t activity, uint32_t value)
{
	struct ec_params_motion_sense params;
	struct ec_response_motion_sense resp;

	params.cmd = MOTIONSENSE_CMD_SET_ACTIVITY;
	params.set_activity.activity = activity;
	params.set_activity.enable = value;

	if (ec_cmd_motion_sense_cmd_v2(cros_ec_get(), &params, &resp) < 0)
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

int cros_ec_read_lid_switch(int *lid)
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
	int lid_open;

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

int cros_ec_read_power_btn(int *pwr_btn)
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
	int pwr_btn;

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
	if (ec_cmd_config_power_button(cros_ec_get(), &params) < 0)
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
	res = ec_cmd_charge_state(cros_ec_get(), &p, &r);

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

int cros_ec_read_batt_state_of_charge(uint32_t *state)
{
	struct ec_params_charge_state params;
	struct ec_response_charge_state resp;

	params.cmd = CHARGE_STATE_CMD_GET_STATE;

	if (ec_cmd_charge_state(cros_ec_get(), &params, &resp) < 0)
		return -1;

	*state = resp.get_state.batt_state_of_charge;
	return 0;
}

int cros_ec_reboot(uint8_t flags)
{
	return cros_ec_reboot_param(cros_ec_get(), EC_REBOOT_COLD, flags);
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

	if (ec_cmd_pwm_set_duty(cros_ec_get(), &params) < 0)
		return -1;
	return 0;
}

static int cros_ec_locate_chip(enum ec_chip_type type, uint8_t port,
			       struct ec_response_locate_chip *r)
{
	struct ec_params_locate_chip p;
	int ret;

	p.type = type;
	p.index = port;
	ret = ec_cmd_locate_chip(cros_ec_get(), &p, r);
	if (ret < 0) {
		printf("Failed to locate TCPC chip for port%d ret:%d\n",
								port, ret);
		return ret;
	}

	return 0;
}

int cros_ec_locate_tcpc_chip(uint8_t port, struct ec_response_locate_chip *r)
{
	return cros_ec_locate_chip(EC_CHIP_TYPE_TCPC, port, r);
}

int cros_ec_locate_pdc_chip(uint8_t port, struct ec_response_locate_chip *r)
{
	return cros_ec_locate_chip(EC_CHIP_TYPE_PDC, port, r);
}

int cros_ec_pd_chip_info(int port, int renew,
			 struct ec_response_pd_chip_info *r)
{
	const struct ec_params_pd_chip_info p = {
		.port = port,
		.live = renew,
	};

	return ec_cmd_pd_chip_info(cros_ec_get(), &p, r);
}

int cros_ec_get_usb_pd_mux_info(int port, uint8_t *mux_state)
{
	struct ec_params_usb_pd_mux_info req;
	struct ec_response_usb_pd_mux_info resp;
	int ret;

	req.port = port;

	ret = ec_cmd_usb_pd_mux_info(cros_ec_get(), &req, &resp);
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

	ret = ec_cmd_usb_pd_control_v2(cros_ec_get(), &pd_control, &resp);
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
	if (ec_cmd_get_protocol_info(me, &info) != sizeof(info)) {
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

	ret = ec_cmd_usb_pd_ports(cros_ec, &usb_pd_ports_r);
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
		ret = ec_cmd_pd_chip_info(cros_ec, &pd_chip_p, &pd_chip_r);
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

int cros_ec_get_features(uint32_t *flags0, uint32_t *flags1)
{
	struct ec_response_get_features response;
	int ret;

	ret = ec_cmd_get_features(cros_ec_get(), &response);
	if (ret < 0) {
		printf("ERROR: Cannot read EC feature flags!\n");
		return -1;
	}

	*flags0 = response.flags[0];
	*flags1 = response.flags[1];

	return ret;
}

int cros_ec_get_usb_pd_ports(int *num_ports)
{
	struct ec_response_usb_pd_ports response;
	int ret;

	ret = ec_cmd_usb_pd_ports(cros_ec_get(), &response);
	if (ret < 0) {
		printf("Failed to get PD count, ret:%d\n", ret);
		return ret;
	}

	*num_ports = response.num_ports;
	return ret;
}

int cros_ec_set_typec_mux(int port, int index, uint8_t mux_state)
{
	struct ec_params_typec_control params;
	int ret;

	params.port = port;
	params.command = TYPEC_CONTROL_COMMAND_USB_MUX_SET;
	params.mux_params.mux_index = index;
	params.mux_params.mux_flags = mux_state;

	ret = ec_cmd_typec_control(cros_ec_get(), &params);
	if (ret < 0)
		printf("%s: Cannot configure mux (%d, %d, %#x, %d)\n",
			__func__, port, index, mux_state, ret);

	return ret;
}

int cros_ec_get_typec_status(int port, struct ec_response_typec_status *status)
{
	struct ec_params_typec_status params;
	int ret;

	params.port = port;

	ret = ec_cmd_typec_status(cros_ec_get(), &params, status);
	if (ret < 0)
		printf("Cannot get type-C port status (%d, %d)\n", port, ret);

	return ret;
}

int cros_ec_i2c_passthru_protect(int i2c_port)
{
	struct ec_params_i2c_passthru_protect params = {
		.subcmd = EC_CMD_I2C_PASSTHRU_PROTECT_ENABLE,
		.port = i2c_port,
	};
	int result;

	result = ec_command(cros_ec_get(), EC_CMD_I2C_PASSTHRU_PROTECT, 0,
			    &params, sizeof(params), NULL, 0);
	if (result < 0)
		return result;

	return 0;
}

int cros_ec_i2c_passthru_protect_status(int i2c_port, int *status)
{
	struct ec_params_i2c_passthru_protect params = {
		.subcmd = EC_CMD_I2C_PASSTHRU_PROTECT_STATUS,
		.port = i2c_port,
	};
	struct ec_response_i2c_passthru_protect response;
	int result;

	result = ec_cmd_i2c_passthru_protect(cros_ec_get(), &params, &response);
	if (result < 0)
		return result;
	if (result < sizeof(response))
		return -1;

	*status = response.status;

	return 0;
}

int cros_ec_i2c_passthru_protect_tcpc_ports(void)
{
	struct ec_params_i2c_passthru_protect protect_p = {
		.subcmd = EC_CMD_I2C_PASSTHRU_PROTECT_ENABLE_TCPCS,
	};
	int ret;

	ret = ec_command(cros_ec_get(), EC_CMD_I2C_PASSTHRU_PROTECT, 0,
			 &protect_p, sizeof(protect_p), NULL, 0);
	if (ret < 0)
		return ret;

	return 0;
}

int cros_ec_set_ap_fw_state(uint32_t state)
{
	struct ec_params_ap_fw_state params;

	params.state = state;

	if (ec_cmd_ap_fw_state(cros_ec_get(), &params) < 0)
		return -1;
	return 0;
}
