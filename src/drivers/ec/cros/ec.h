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

#ifndef __DRIVERS_EC_CROS_EC_H__
#define __DRIVERS_EC_CROS_EC_H__

#include <stdint.h>

#include "drivers/ec/vboot_ec.h"
#include "drivers/ec/cros/commands.h"
#include "drivers/gpio/gpio.h"

typedef struct CrosEcBusOps
{
	int (*init)(struct CrosEcBusOps *me);

	/**
	 * Send a command to a ChromeOS EC device and return the reply.
	 *
	 * The device's internal input/output buffers are used.
	 *
	 * @param bus		ChromeOS EC bus ops
	 * @param cmd		Command to send (EC_CMD_...)
	 * @param cmd_version	Version of command to send (EC_VER_...)
	 * @param dout          Output data (may be NULL If dout_len=0)
	 * @param dout_len      Size of output data in bytes
	 * @param dinp          Returns pointer to response data
	 * @param din_len       Maximum size of response in bytes
	 * @return number of bytes in response, or -1 on error
	 */
	int (*send_command)(struct CrosEcBusOps *me, uint8_t cmd,
			    int cmd_version,
			    const void *dout, uint32_t dout_len,
			    void *din, uint32_t din_len);

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

struct CrosEc;
typedef int (*CrosEcSendCommandFunc)(struct CrosEc *me, int cmd,
				     int cmd_version, const void *dout,
				     int dout_len, void *dinp, int din_len);

typedef struct CrosEc
{
	VbootEcOps vboot;
	CrosEcBusOps *bus;
	int devidx;
	GpioOps *interrupt_gpio;
	CrosEcSendCommandFunc send_command;
	int initialized;
	int max_param_size;
	struct ec_host_request *proto3_request;
	int proto3_request_size;
	struct ec_host_response *proto3_response;
	int proto3_response_size;
} CrosEc;

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
 * @return 0 if ok, -1 on error
 */
int ec_command(CrosEc *ec, int cmd, int cmd_version,
	       const void *dout, int dout_len,
	       void *din, int din_len);

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
 * Read a keyboard scan from the ChromeOS EC device
 *
 * Send a message requesting a keyboard scan and return the result
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
int cros_ec_get_next_event(struct ec_response_get_next_event *event);

/**
 * Check if the ChromeOS EC device has an interrupt pending.
 *
 * Read the status of the external interrupt connected to the ChromeOS EC
 * device. If no external interrupt is configured, this always returns 1.
 *
 * @return 0 if no interrupt is pending
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
 * Read/write VbNvContext from/to a ChromeOS EC device.
 *
 * @param block		Buffer of VbNvContext to be read/write
 * @return 0 if ok, -1 on error
 */
int cros_ec_read_vbnvcontext(uint8_t *block);
int cros_ec_write_vbnvcontext(const uint8_t *block);

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
int cros_ec_read_lid_switch(uint32_t *lid);

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
int cros_ec_read_power_btn(uint32_t *pwr_btn);

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
 * Reboots main EC
 *
 * @param flags
 * @return 0 if ok, -1 on error
 */
int cros_ec_reboot(uint8_t flags);

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

CrosEc *new_cros_ec(CrosEcBusOps *bus, int devidx, GpioOps *interrupt_gpio);

#endif
