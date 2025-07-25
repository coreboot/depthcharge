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

#ifndef __DRIVERS_EC_CROS_EC_H__
#define __DRIVERS_EC_CROS_EC_H__

#include <commonlib/list.h>
#include <stdint.h>
#include <libpayload.h>

#include "drivers/ec/vboot_auxfw.h"
#include "drivers/ec/vboot_ec.h"
#include "drivers/ec/cros/commands.h"
#include "drivers/gpio/gpio.h"

typedef struct CrosEcBusOps
{
	int (*init)(struct CrosEcBusOps *me);

	/**
	 * Send a proto3 packet to a ChromeOS EC device and return the reply.
	 *
	 * @param bus		ChromeOS EC bus ops
	 * @param dout          Output data (including command)
	 * @param dout_len      Size of output data in bytes
	 * @param din           Buffer that input data will be returned in
	 * @param din_len       Maximum size off input buffer in bytes
	 * @return 0 on success or negative EC_RES_XXX code on error
	 */
	int (*send_packet)(struct CrosEcBusOps *me,
			   const void *dout, uint32_t dout_len,
			   void *din, uint32_t din_len);

	/**
	 * Byte I/O functions.
	 *
	 * Read or write a sequence a bytes over the desired protocol.
	 * Used to support protocol variants - currently only implemented
	 * for LPC.
	 *
	 * @param data		Read / write data buffer
	 * @param port		I/O port
	 * @size		Number of bytes to read / write
	 */
	void (*read)(uint8_t *data, uint16_t port, int size);
	void (*write)(const uint8_t *data, uint16_t port, int size);
} CrosEcBusOps;

typedef struct CrosEc
{
	VbootEcOps vboot;
	CrosEcBusOps *bus;
	GpioOps *interrupt_gpio;
	int initialized;
	int max_param_size;
	struct ec_host_request *proto3_request;
	int proto3_request_size;
	struct ec_host_response *proto3_response;
	int proto3_response_size;
} CrosEc;

/* Maximum wait time for EC flash erase completion */
#define CROS_EC_ERASE_TIMEOUT_MS 30000

typedef struct CrosEcAuxfwChipInfo
{
	/* List Node in the chip list */
	struct list_node list_node;
	/* Chip identifier as defined by the vendor_id:product_id */
	uint16_t vid;
	uint16_t pid;
	/*
	 * Function to create and register the firmware update operations
	 * for that chip.
	 */
	const VbootAuxfwOps * (*new_chip_aux_fw_ops)(
				struct ec_response_pd_chip_info_v2 *chip,
				uint8_t ec_pd_id);
} CrosEcAuxfwChipInfo;

/*
 * Global list of chips that require FW update. The drivers of the concerned
 * chips should add themselves to the list via INIT_FUNC.
 */
extern struct list_node ec_aux_fw_chip_list;

/**
 * Send an arbitrary EC command.
 *
 * @param ec		EC device
 * @param cmd		Command to send (EC_CMD_...)
 * @param cmd_version	Command version number (often 0)
 * @param dout		Outgoing data to EC
 * @param dout_len	Outgoing length in bytes
 * @param din		Where to put the incoming data from EC
 * @param din_len	Max number of bytes to accept from EC
 * @return negative EC_RES_xxx error code, or positive num bytes received.
 */
int ec_command(CrosEc *ec, int cmd, int cmd_version,
	       const void *dout, int dout_len,
	       void *din, int din_len);

/**
 * Get the handle to main/primary EC
 *
 * @return pointer to primary EC structure.
 */
CrosEc *cros_ec_get(void);

/*
 * Hard-code the number of columns we happen to know we have right now.  It
 * would be more correct to call cros_ec_mkbp_info() at startup and determine
 * the actual number of keyboard cols from there.
 */
#define CROS_EC_KEYSCAN_COLS 13

/* Information returned by a key scan */
struct cros_ec_keyscan {
	uint8_t data[CROS_EC_KEYSCAN_COLS];
	uint32_t buttons;
};

/**
 * Return non-zero if the EC supports the command and version
 *
 * @param cmd		Command to check
 * @param ver		Version to check
 * @return non-zero if command version supported; 0 if not.
 */
int cros_ec_cmd_version_supported(int cmd, int ver);

/**
 * Gets the SKU id from the EC
 *
 * @param id		The returned SKU id
 * @return 0 if ok, -1 on error
 */
int cros_ec_cbi_get_sku_id(uint32_t *id);

/**
 * Gets the OEM id from the EC
 *
 * @param id		The returned OEM id
 * @return 0 if ok, -1 on error
 */
int cros_ec_cbi_get_oem_id(uint32_t *id);

/**
 * Sends the specified PD control command on the specified PD port
 *
 * @param pd_port	The PD port id
 * @param cmd		The PD control command to send
 * @return 0 if ok, negative (EC return code) on error
 */
int cros_ec_pd_control(uint8_t pd_port, enum ec_pd_control_cmd cmd);

/**
 * Get the EC I2C bus speed.
 *
 * @param i2c__port	The I2C port number
 * @param speed		Pointer to speed return value in kHz. Speed value
 *			of EC_I2C_CONTROL_SPEED_UNKNOWN is returned if the
 *			speed is not known or tracked.
 * @return 0 if ok, negative (EC return code) on error
 */
int cros_ec_i2c_get_speed(uint8_t i2c_port, uint16_t *speed_khz);

/**
 * Set the EC I2C bus speed, returning the previous speed.
 *
 * @param i2c__port	The I2C port number
 * @param new_speed	The new I2C speed to configure in kHz
 * @param old_speed	If non-null, pointer to previous speed return
 *			value in kHz.  EC_I2C_CONTROL_SPEED_UNKNOWN is
 *			returned if the previous speed is not known or
 *			tracked.
 * @return 0 if ok, negative (EC return code) on error
 */
int cros_ec_i2c_set_speed(uint8_t i2c_port,
			  uint16_t new_speed_khz,
			  uint16_t *old_speed_khz);

/**
 * Read a keyboard scan from the ChromeOS EC device
 *
 * @param scan		Place to put the scan results
 * @return 0 if ok, -1 on error
 */
int cros_ec_scan_keyboard(struct cros_ec_keyscan *scan);

/**
 * Get the next pending MKBP event from the ChromeOS EC device.
 *
 * Send a message requesting the next event and return the result.
 *
 * @param event		Place to put the event.
 * @return number of bytes received if ok, else EC error response code.
 */
int cros_ec_get_next_event(struct ec_response_get_next_event_v1 *event);

/**
 * Check if the ChromeOS EC device has an interrupt pending.
 *
 * Read the status of the external interrupt connected to the ChromeOS EC
 * device. If no external interrupt is configured, this returns -1.
 *
 * @return 1 if interrupt pending, 0 if not pending, -1 if no interrupt pin.
 */
int cros_ec_interrupt_pending(void);

/**
 * Read information about the keyboard matrix
 *
 * @param info		Place to put the info structure
 */
int cros_ec_mkbp_info(struct ec_response_mkbp_info *info);

/**
 * Get the specified event mask
 *
 * @param type	Host event type from commands.h
 * @param mask	Mask for this host event
 * @return 0 if ok, <0 on error
 */
int cros_ec_get_event_mask(u8 type, uint32_t *mask);

/**
 * Set the specified event mask
 *
 * @param type	Host event type from commands.h
 * @param mask	Mask to set for this host event type
 * @return 0 if ok, <0 on error
 */
int cros_ec_set_event_mask(u8 type, uint32_t mask);

/**
 * Read the host event flags
 *
 * @param events_ptr	Destination for event flags.  Not changed on error.
 * @return 0 if ok, <0 on error
 */
int cros_ec_get_host_events(uint32_t *events_ptr);

/**
 * Clear the specified host event flags
 *
 * @param events	Event flags to clear
 * @return 0 if ok, <0 on error
 */
int cros_ec_clear_host_events(uint32_t events);

/* Internal interfaces */

/**
 * Dump a block of data for a command.
 *
 * @param name	Name for data (e.g. 'in', 'out')
 * @param cmd	Command number associated with data, or -1 for none
 * @param data	Data block to dump
 * @param len	Length of data block to dump
 */
void cros_ec_dump_data(const char *name, int cmd, const void *data, int len);

/**
 * Calculate a simple 8-bit checksum of a data block
 *
 * @param data	Data block to checksum
 * @param size	Size of data block in bytes
 * @return checksum value (0 to 255)
 */
uint8_t cros_ec_calc_checksum(const void *data, int size);

/**
 * Enable / disable motion sense activity.
 *
 * @param activity	indicating the activity to set.
 * @param value	indicating enable/disable activity.
 * @return 0 if ok, -1 on error
 */
int cros_ec_set_motion_sense_activity(uint32_t activity, uint32_t value);

/**
 * Cut-off battery on ChromeOS EC device.
 *
 * @param flags		Flag to indicate cut-off options.
 * @return 0 if ok, -1 on error
 */
int cros_ec_battery_cutoff(uint8_t flags);

/**
 * set sustain battery on ChromeOS EC device.
 *
 * @param lower		Lower thresholds for battery sustainer.
 * @param upper		Upper thresholds for battery sustainer.
 * @return 0 if ok, -1 on error
 */
int cros_ec_battery_sustain(unsigned char lower, unsigned char upper);

/**
 * Read the value of present battery voltage.
 *
 * @param volt		Buffer to read present battery voltage.
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_batt_volt(uint32_t *volt);

/**
 * Read the value of battery state-of-charge.
 *
 * @param: state	Buffer to read battery state-of-charge.
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_batt_state_of_charge(uint32_t *state);

/**
 * Read limit power request from the EC
 *
 * @param limit_power	Pointer to result destination
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_limit_power_request(int *limit_power);


/**
 * Set duty cycle of the display panel.
 *
 * @param percent	Desired duty cycle, in 0..99 range.
 * @return 0 if ok, -1 on error
 */
int cros_ec_set_bl_pwm_duty(uint32_t percent);

/**
 * Read the value of the 'lid open' switch.
 *
 * @param lid		Buffer to read lid open flag (returns 0 or 1)
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_lid_switch(int *lid);

/**
 * Return GpioOps that will read the 'lid open' switch.
 */
GpioOps *cros_ec_lid_switch_flag(void);

/**
 * Read the value of power button.
 *
 * @param pwr_btn	Buffer to read lid open flag (returns 0 or 1)
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_power_btn(int *pwr_btn);

/**
 * Return GpioOps that will read the power button.
 */
GpioOps *cros_ec_power_btn_flag(void);

/**
 * Enable/Disable execution of SMI pulse on x86 systems
 *
 * @param flag		bitmask of power button configs
 *                      bit 0: enable smi pulse
 * @return 0 if ok, -1 on error
 */
int cros_ec_config_powerbtn(uint32_t enable);

/**
 * Reboots the EC
 *
 * @param flags		Reboot flags
 * @return 0 if ok, -1 on error
 */
int cros_ec_reboot(uint8_t flags);

/**
 * Reboot the EC with specified parameters
 *
 * @param ec		EC device
 * @param cmd		Reboot command
 * @param flags		Reboot flags
 * @return 0 if ok, -1 on error
 */
int cros_ec_reboot_param(CrosEc *me, enum ec_reboot_cmd cmd, uint8_t flags);

/**
 * Read the value of lid shutdown mask on x86 systems
 *
 * @return 1 if enabled, 0 if disabled, -1 on error
 */
int cros_ec_get_lid_shutdown_mask(void);

/**
 * Set the value of lid shutdown mask on x86 systems
 *
 * @param enable       Set to enable lid shutdown
 * @return 0 if ok, -1 on error
 */
int cros_ec_set_lid_shutdown_mask(int enable);

/* -----------------------------------------------------------------------
 * Locate TCPC chip on EC and retrieve its remote bus information
 *   port: TCPC port index whose information is being retrieved
 *   r: Response structure where the information is stored
 *   Returns: 0 on success, < 0 on error.
 */
int cros_ec_locate_tcpc_chip(uint8_t port, struct ec_response_locate_chip *r);

/* -----------------------------------------------------------------------
 * Locate PDC chip on EC and retrieve its remote bus information
 *   port: PDC port index whose information is being retrieved
 *   r: Response structure where the information is stored
 *   Returns: 0 on success, < 0 on error.
 */
int cros_ec_locate_pdc_chip(uint8_t port, struct ec_response_locate_chip *r);

CrosEc *new_cros_ec(CrosEcBusOps *bus, GpioOps *interrupt_gpio);

/**
 * Probe EC to gather chip info that require FW update and register
 * auxfw operations with the vboot_auxfw driver.
 */
void cros_ec_probe_aux_fw_chips(void);

/**
 * Return a port's PD chip information.
 *
 * @param port		The desired port number
 * @param renew		Refresh cached value
 * @param r		Result buffer for chip info
 *
 * @return 0 if ok, -1 on error
 */
int cros_ec_pd_chip_info(int port, int renew,
			 struct ec_response_pd_chip_info_v2 *r);

/**
 * Return a port's mux state.
 *
 * @param port		the desired port number
 * @param mux_state	pointer to the mux state (bitmask) result
 *
 * @return 0 if ok, -1 on error
 */
int cros_ec_get_usb_pd_mux_info(int port, uint8_t *mux_state);

/**
 * Return a port's UFP and DBG-ACC state.
 *
 * @param port		the desired port number
 * @param ufp		pointer to the UFP state result
 * @param dbg_acc	pointer to the debug-acc state result
 * @return 0 if ok, -1 on error
 */
int cros_ec_get_usb_pd_control(int port, int *ufp, int *dbg_acc);

/**
 * Return the EC's feature flags.
 *
 * @param flags0	Feature flags bitset 0
 * @param flags1	Feature flags bitset 1
 * @return 0 if ok, -1 on error
 */
int cros_ec_get_features(uint32_t *flags0, uint32_t *flags1);

/**
 * Return the number of ports
 *
 * @param num_ports		pointer to the number of ports
 * @return 0 if ok, -1 on error
 */
int cros_ec_get_usb_pd_ports(int *num_ports);

/**
 * Configure a type-C mux
 *
 * @param port		the desired port number
 * @param index		the index of the desired mux
 * @param mux_state	the state to set the mux to
 * @return 0 if ok, -1 on error
 */
int cros_ec_set_typec_mux(int port, int index, uint8_t mux_state);

/**
 * Return the status of the given port
 *
 * @param port		port to query
 * @param status	port status
 * @return 0 if ok, -1 on error
 */
int cros_ec_get_typec_status(int port, struct ec_response_typec_status *status);

/**
 * Disable I2C command pass-through on all I2C ports used for TCPCs.
 *
 * @return 0 if ok, -1 on error
 */
int cros_ec_i2c_passthru_protect_tcpc_ports(void);

/**
 * Disable I2C command pass-through on the specified I2C port on the EC.
 *
 * @param i2c_port	I2C port to query
 * @return 0 if ok, -1 on error
 */
int cros_ec_i2c_passthru_protect(int i2c_port);

/**
 * Query the I2C command pass-through status on the specified I2C port
 * on the EC.
 *
 * @param i2c_port	I2C port to query
 * @param status	pointer to the status result (0: unlocked, 1: locked)
 * @return 0 if ok, -1 on error
 */
int cros_ec_i2c_passthru_protect_status(int i2c_port, int *status);

/**
 * Set AP firmware state for reading by FAFT (Fully Automated Firmware Tests).
 * We use this to indicate the currently displayed firmware screen.
 *
 * @param state		State value
 * @return 0 if ok, -1 on error
 */
int cros_ec_set_ap_fw_state(uint32_t state);

/**
 * Print message on the EC console.
 *
 * @param fmt		Format string
 * @param ...		printf like arguments for format string
 * @return 0 if ok, negative on error
 */
int cros_ec_print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* __DRIVERS_EC_CROS_EC_H__ */
