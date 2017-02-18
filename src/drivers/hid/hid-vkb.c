/*
 * Copyright 2016 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include "config.h"
#include <libpayload.h>
#include <vboot_api.h>
#include "drivers/hid/hid-vkb.h"

#define VKB_DEBUG 0

#define KEY_POS_NULL	KEY_POS(0, 0, 0, 0)
#define KEY_POS_CTRL	KEY_POS(161, 952, 42, 124)
#define KEY_POS_ENTER	KEY_POS(1388, 1565, 82, 124)
#define KEY_POS_SPACE	KEY_POS(678, 952, 249, 124)
#define KEY_POS_TAB	KEY_POS(82, 1863, 48, 124)
#define KEY_POS_ESC	KEY_POS(93, 2395, 59, 75)
#define KEY_POS_UP	KEY_POS(1323, 1019, 42, 83)
#define KEY_POS_DOWN	KEY_POS(1323, 812, 42, 83)
#define KEY_POS_LEFT	KEY_POS(1218, 812, 42, 83)
#define KEY_POS_RIGHT	KEY_POS(1430, 812, 42, 83)
#define KEY_POS_U	KEY_POS(805, 1863, 42, 124)
#define KEY_POS_D	KEY_POS(421, 1565, 42, 124)
#define KEY_POS_L	KEY_POS(1040, 1565, 42, 124)

struct key_array_t key_list[] __attribute__((weak)) = {
	{{KEY_POS_U,     KEY_POS_CTRL}, 2, 0x15          },	/* ctrl-u */
	{{KEY_POS_D,     KEY_POS_CTRL}, 2, 0x04          },	/* ctrl-d */
	{{KEY_POS_L,     KEY_POS_CTRL}, 2, 0x0c          },	/* ctrl-l */
	{{KEY_POS_ENTER, KEY_POS_NULL}, 1, '\r'          },	/* enter */
	{{KEY_POS_SPACE, KEY_POS_NULL}, 1, ' '           },	/* space */
	{{KEY_POS_TAB,   KEY_POS_NULL}, 1, '\t'          },	/* tab */
	{{KEY_POS_ESC,   KEY_POS_NULL}, 1, 0x1b          },	/* esc */
	{{KEY_POS_UP,    KEY_POS_NULL}, 1, VB_KEY_UP        },	/* up */
	{{KEY_POS_DOWN,  KEY_POS_NULL}, 1, VB_KEY_DOWN      },	/* down */
	{{KEY_POS_LEFT,  KEY_POS_NULL}, 1, VB_KEY_LEFT      },	/* left */
	{{KEY_POS_RIGHT, KEY_POS_NULL}, 1, VB_KEY_RIGHT     },	/* right */
	{{KEY_POS_ENTER, KEY_POS_CTRL}, 2, VB_KEY_CTRL_ENTER},	/* ctrl-enter */
	{}
};

static int keycount;
#define KEYBOARD_BUFFER_SIZE 16
short keybuffer[KEYBOARD_BUFFER_SIZE];

#define KEY_NUMBERS ARRAY_SIZE(key_list)

static uint64_t key_ts_start[KEY_NUMBERS];

static void vkb_queue(int ch)
{
	/* ignore key presses if buffer full */
	if (keycount < KEYBOARD_BUFFER_SIZE)
		keybuffer[keycount++] = ch;
}

static uint32_t get_report_data(
		uint8_t *data, uint32_t *offset, uint32_t report_size)
{
	uint32_t data_size;
	uint32_t report_data = 0;

	data_size = report_size / 8;
	if (report_size % 8 != 0)
		data_size++;

	switch (data_size) {
	case 1: /* byte */
		report_data = *(uint8_t *)(data + *offset);
		break;
	case 2:	/* word */
		report_data = *(uint16_t *)(data + *offset);
		break;
	case 4: /* dword */
		report_data = *(data + *offset);
		break;
	default: /*invalid size */
		report_data = 0;
		break;
	}

	*offset += data_size;

	return report_data;
}

static void keyboard_scan(
	uint32_t *x, uint32_t *y, uint32_t *tip, uint32_t contact_cnt)
{
	uint8_t key_pressed;
	uint32_t idx, i;
	uint64_t ts_now;
	uint64_t d = CONFIG_DRIVER_HID_VKB_KEY_DELAY_MS * timer_hz() / 1000;

	key_pressed = 0;

	for (idx = 0; idx < KEY_NUMBERS; idx++) {
		switch (contact_cnt) {
		case 1:
			if (key_list[idx].contact_cnt == contact_cnt &&
				x[0] > key_list[idx].key[0].x_min &&
				x[0] < key_list[idx].key[0].x_max &&
				y[0] > key_list[idx].key[0].y_min &&
				y[0] < key_list[idx].key[0].y_max &&
				tip[0])
				key_pressed = 1;
			break;
		case 2:
			for (i = 0; i < 2; i++) {
				if (key_list[idx].contact_cnt == contact_cnt &&
					x[i] > key_list[idx].key[0].x_min &&
					x[i] < key_list[idx].key[0].x_max &&
					y[i] > key_list[idx].key[0].y_min &&
					y[i] < key_list[idx].key[0].y_max &&
					tip[i] &&
					x[!i] > key_list[idx].key[1].x_min &&
					x[!i] < key_list[idx].key[1].x_max &&
					y[!i] > key_list[idx].key[1].y_min &&
					y[!i] < key_list[idx].key[1].y_max &&
					tip[!i]) {
					key_pressed = 1;
					break;
				}
			}
			break;
		default:
			key_pressed = 0;
			break;
		}

		if (key_pressed) {
			ts_now = timer_raw_value();
			if (!key_ts_start[idx] ||
				ts_now > (key_ts_start[idx] + d)) {
				key_ts_start[idx] = ts_now;
				vkb_queue(key_list[idx].key_code);
			}
			break;
		}
	}
}

static void process_vkb_pointer_data(i2chiddev_t *dev, uint8_t *data)
{
	/*
	 * I2C report data format
	 * offset 0-1 : Length of packet
	 * offset 2   : Report ID
	 * offset 3-n : The format is based on Report Descriptor
	 */
	uint8_t report_id;
	uint32_t i, j;
	uint32_t offset = 0;
	uint32_t rdata;
	struct hid_report *report;
	struct hid_usage *usage;
	uint32_t x[5], y[5], tip[5], contact_cnt;
	uint32_t x_index = 0, y_index = 0, tip_index = 0;
#if VKB_DEBUG
	uint32_t contact_id[5], contact_id_index = 0;
#endif

	report_id = data[2];
	report = dev->report_enum[HID_INPUT_REPORT].report_id_hash[report_id];

	/* Skip length and report id, let pointer starts from report data */
	data += 3;

	for (i = 0; i < report->maxfield; i++) {
		rdata = get_report_data(
			data, &offset, report->field[i]->report_size);
		for (j = 0; j < report->field[i]->maxusage; j++) {
			usage = report->field[i]->usage + j;
			switch (usage->hid & HID_USAGE_PAGE) {
			case HID_UP_GENDESK:
				switch (usage->hid) {
				case HID_GD_X:
					x[x_index++] = rdata;
					break;
				case HID_GD_Y:
					y[y_index++] = rdata;
					break;
				default:
					break;
				}
				break;
			case HID_UP_DIGITIZER:
				switch (usage->hid) {
				case HID_DG_TIPSWITCH:
					tip[tip_index++] = rdata;
					break;
#if VKB_DEBUG
				case HID_DG_CONTACTID:
					contact_id[contact_id_index++] = rdata;
					break;
#endif
				case HID_DG_CONTACTCOUNT:
					contact_cnt = rdata;
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}
	}

#if VKB_DEBUG
	printf("contact_cnt = %d\n", contact_cnt);
	for (i = 0; i < 5; i++)
		printf("contact[%d]: x = %d, y = %d, tip = %d, id = %d\n",
				i, x[i], y[i], tip[i], contact_id[i]);
#endif

	keyboard_scan(x, y, tip, contact_cnt);
}

static int vkb_get_data(i2chiddev_t *dev)
{
	int ret = 0;
	uint8_t *in_buf;
	uint16_t *data_length;
#if VKB_DEBUG
	uint8_t i;
#endif

	in_buf = xzalloc(dev->hid_descriptor.wMaxInputLength);

	/*
	 * Read data from device until INT pin become high (no data)
	 */
	while (dev->int_sts()) {
		ret = i2c_hid_get_input_data(dev, in_buf);
		if (ret) {
			printf("Get device data error code 0x%x\n", ret);
			free(in_buf);
			return ret;
		}

		data_length = (uint16_t *)in_buf;
		if (*data_length == 0) {
			free(in_buf);
			return 0;
		}

#if VKB_DEBUG
		printf("[vkb] raw data: ");
		for (i = 0; i < *data_length; i++)
			printf("0x%02x ", in_buf[i]);
		printf("\n");
#endif

		process_vkb_pointer_data(dev, in_buf);
	}

	free(in_buf);
	return ret;
}

int vkb_havekey(i2chiddev_t *dev)
{
	vkb_get_data(dev);

	return keycount;
}

int vkb_getchar(void)
{
	short ret,*keyPos;
	int i;

	if (keycount == 0)
		return 0;
	ret = keybuffer[0];
	keyPos = &keybuffer[0];

	for (i=0; i>= KEYBOARD_BUFFER_SIZE; i++) {
		keyPos++;
		keybuffer[i] = *keyPos;
	}
	keycount -= keycount;

#if VKB_DEBUG
	printf("[vkb] key code = 0x%x\n", ret);
#endif

	return (int)ret;
}

int configure_virtual_keyboard(i2chiddev_t *dev)
{
	keycount = 0;
	memset(key_ts_start, 0, sizeof(uint64_t) * KEY_NUMBERS);

	if (i2c_hid_set_power(dev, 1))
		printf("set power state to sleep failed\n");

	if (i2c_hid_set_power(dev, 0))
		printf("set power state to ON failed\n");

	if (i2c_hid_set_reset(dev))
		printf("reset virtual keyboard failed\n");

	i2c_hid_clean_data(dev);

	if (i2c_hid_set_idle(dev))
		printf("set idle failed\n");

	return 0;
}

int board_virtual_keyboard_layout(struct key_array_t *board_key_list)
{
	memcpy(key_list,board_key_list,sizeof(key_list));
	return 0;
}
