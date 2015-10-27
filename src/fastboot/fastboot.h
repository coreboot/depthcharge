/*
 * Copyright 2015 Google Inc.
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

#ifndef __FASTBOOT_FASTBOOT_H__
#define __FASTBOOT_FASTBOOT_H__

#include <assert.h>

#include "fastboot/print.h"

#define FB_VERSION_STRING	"0.3"

/* Fastboot getvar variables */
typedef enum {
	FB_VERSION,
	FB_BOOTLOADER_VERSION,
	FB_BASEBAND_VERSION,
	FB_PRODUCT,
	FB_SERIAL_NO,
	FB_SECURE,
	FB_DWNLD_SIZE,
	FB_PART_TYPE,
	FB_PART_SIZE,
	FB_BDEV_SIZE,
	FB_UNLOCKED,
	FB_OFF_MODE_CHARGE,
	FB_VARIANT,
	FB_BATT_VOLTAGE,
} fb_getvar_t;

typedef enum fb_ret {
	FB_SUCCESS,
	FB_NORMAL_BOOT,
	FB_BOOT_MEMORY,
	FB_REBOOT,
	FB_REBOOT_BOOTLOADER,
	FB_POWEROFF,
	FB_CONTINUE_RECOVERY,
} fb_ret_type;


typedef enum {
	FB_SETENV_FORCE_ERASE,
} fb_setenv_t;

/*
 * IMPORTANT!!!!
 * Prefix len is set to 4 under the assumption that all command responses are of
 * 4 bytes. If a new response type is added below which is not 4 bytes, prefix
 * addition will not work properly.
 */
#define PREFIX_LEN	4
typedef enum fb_rsp {
	FB_NONE,
	FB_DATA,
	FB_FAIL,
	FB_INFO,
	FB_OKAY,
} fb_rsp_type;

typedef enum {
	FB_BUTTON_NONE = 0,
	FB_BUTTON_UP   = (1 << 0),
	FB_BUTTON_DOWN = (1 << 1),
	FB_BUTTON_SELECT = (1 << 2),
	FB_BUTTON_CONFIRM = (1 << 3),
	FB_BUTTON_CANCEL = (1 << 4),
} fb_button_type;

/*
 * fb_buffer defines either input / output buffer for a fastboot cmd.
 * For input buffer, data is the command received from host, head points to
 * start of unconsumed data, tail points to end of data and capacity equals the
 * length of the command string i.e. tail.
 * For output buffer, data is the response to be sent to the host. Head is the
 * start of data, tail is the end of data written in the buffer and capacity is
 * the maximum number of characters that the buffer can hold.
 */
struct fb_buffer {
	/* Pointer to data */
	char *data;
	/* Start of unconsumed data */
	size_t head;
	/* End of data */
	size_t tail;
	/* Total bytes that data can hold */
	size_t capacity;
};

/* Returns count of unconsumed data */
static inline size_t fb_buffer_length(struct fb_buffer *b)
{
	assert (b->tail >= b->head);
	return b->tail - b->head;
}

/* Enqueues data towards the end of buffer */
static inline void fb_buffer_push(struct fb_buffer *b, size_t len)
{
	b->tail += len;
	assert (b->tail <= b->capacity);
}

/* Return pointer to data at current head */
static inline char *fb_buffer_head(struct fb_buffer *b)
{
	return &b->data[b->head];
}

/* Return pointer to data at current head and advances current head by len */
static inline char *fb_buffer_pull(struct fb_buffer *b, size_t len)
{
	char *ret = fb_buffer_head(b);
	b->head += len;
	assert (b->head <= b->tail);
	return ret;
}

/* Returns number of free bytes remaining in the buffer */
static inline size_t fb_buffer_remaining(struct fb_buffer *b)
{
	return b->capacity - b->tail;
}

/* Returns pointer to first free byte in the buffer */
static inline char *fb_buffer_tail(struct fb_buffer *b)
{
	return &b->data[b->tail];
}

/* Resets head and tail to 0 */
static inline void fb_buffer_rewind(struct fb_buffer *b)
{
	b->head = b->tail = 0;
}

/* Clones given buffer into new buffer */
static inline void fb_buffer_clone(struct fb_buffer *b, struct fb_buffer *newb)
{
	newb->data = b->data;
	newb->head = b->head;
	newb->tail = b->tail;
	newb->capacity = b->capacity;
}

/*
 * fb_cmd is used to represent both input and output of the fastboot command. It
 * can be easily passed across different function calls to allow appending of
 * string to the output buffer if required.
 */
struct fb_cmd {
	struct fb_buffer input;
	struct fb_buffer output;
	fb_rsp_type type;
};

/*
 * Returns 0 if device is unlocked or non-zero otherwise.
 */
int fb_device_unlocked(void);

fb_ret_type device_mode_enter(void);
/*
 * Function to add string to output buffer.
 * format is printf style format specifier. str cannot be NULL. args can be set
 * to NULL.
 */
void fb_add_string(struct fb_buffer *buff, const char *str, const char *args);
/*
 * Function to add number to output buffer.
 * format is printf style format specifier for the number. (mandatory)
 * number to be put in the output buffer.
 */
void fb_add_number(struct fb_buffer *buff, const char *format,
		   unsigned long long num);

/******* Functions to be implemented by board wanting to use fastboot *******/

typedef struct {
	/*
	 * Read value of a variable from board. Board is expected to fill in the
	 * value of the variable in cmd->output buffer.
	 *
	 * Return value:
	 *  0 = success
	 * -1 = error
	 */
	int (*get_var)(struct fb_cmd *cmd, fb_getvar_t var);

	/*
	 * Check if board should enter device mode for fastboot. (Device mode
	 * means that the board is ready to act as a device for USB connection).
	 *
	 * Return value:
	 * 1 = enter device mode
	 * 0 = otherwise
	 */
	int (*enter_device_mode)(void);

	/*
	 * Read keyboard mask to check if fastboot event is requested.
	 *
	 * Return value:
	 * 1 = user requested entry into fastboot mode
	 * 0 = no request for entry into fastboot mode
	 */
	int (*keyboard_mask)(void);

	/*
	 * Set/clear boot on ac detect value. Boot on ac detect determines
	 * whether the device should start booting up if AC is plugged in
	 * off-mode.
	 *
	 * Params:
	 * boot_on_ac_detect : 1=set, 0=clear
	 *
	 * Return value:
	 * 0 = success
	 * -1 = error
	 */
	int (*set_boot_on_ac_detect)(int boot_on_ac);

	/*
	 * Print message on display screen.
	 *
	 * Params:
	 * msg: Message to be printed
	 * len: Message len
	 * type: Type of message (one of the fb_msg_t enums)
	 *
	 * No return value.
	 */
	void (*print_screen)(fb_msg_t type, const char *msg, size_t len);

	/*
	 * Handle fastboot menu.
	 *
	 * Return value:
	 * 1 = enter fastboot mode
	 * 0 = continue with USB recovery
	 *
	 * Handler is expected to perform reboot/poweroff on its own.
	 */
	int (*enter_menu)(void);

	/*
	 * Read battery voltage.
	 *
	 * Return value:
	 * 0 = success
	 * -1 = error
	 *
	 * Voltage is returned in param mV.
	 */
	int (*read_batt_volt)(uint32_t *mV);

	/*
	 * Cut-off battery.
	 *
	 * Return value:
	 * 0 = success
	 * -1 = error
	 */
	int (*battery_cutoff)(void);

	/*
	 * Read user input in fb_button_type format.
	 *
	 * Params:
	 * button_flags: Indicates buttons that are expected.
	 * timeout_us: Time to wait for user input in microseconds.
	 *
	 * Return:
	 * Button out of the button flags that was pressed.
	 * FB_BUTTON_NONE if no button pressed until timeout or input error.
	 */
	fb_button_type (*read_input)(uint32_t button_flags,
				     uint64_t timeout_us);

	/*
	 * String to be displayed corresponding to a button.
	 *
	 * Params:
	 * button: Button of type fb_button_type for which string is required.
	 *
	 * Return:
	 * String corresponding to the button. NULL if failure.
	 */
	const char * (*get_button_str)(fb_button_type button);

} fb_callback_t;

extern fb_callback_t fb_board_handler;

#endif /* __FASTBOOT_FASTBOOT_H__ */
