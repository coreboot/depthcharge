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

#ifndef __DRIVERS_I2C_HID_H__
#define __DRIVERS_I2C_HID_H__

#include "base/list.h"
#include "base/cleanup_funcs.h"

#define I2C_HID_DESCROPTOR_LENGTH 30

#define HID_RESET           0x01
#define HID_GET_REPORT      0x02
#define HID_SET_REPORT      0x03
#define HID_GET_IDLE        0x04
#define HID_SET_IDLE        0x05
#define HID_GET_PROTOCOL    0x06
#define HID_SET_PROTOCOL    0x07
#define HID_SET_POWER       0x08

#define max_t(type, x, y) ({				\
	type __max1 = (x);					\
	type __max2 = (y);					\
	__max1 > __max2 ? __max1 : __max2; })

/*
 * We parse each description item into this structure. Short items data
 * values are expanded to 32-bit signed int, long items contain a pointer
 * into the data area.
 */

struct hid_item {
	unsigned     format;
	uint8_t      size;
	uint8_t      type;
	uint8_t      tag;
	union {
	    uint8_t   u8;
	    int8_t    s8;
	    uint16_t  u16;
	    int16_t   s16;
	    uint32_t  u32;
	    int32_t   s32;
	    uint8_t  *longdata;
	} data;
};

/*
 * HID report item format
 */

#define HID_ITEM_FORMAT_SHORT	0
#define HID_ITEM_FORMAT_LONG	1

/*
 * HID report descriptor item type (prefix bit 2,3)
 */

#define HID_ITEM_TYPE_MAIN		0
#define HID_ITEM_TYPE_GLOBAL		1
#define HID_ITEM_TYPE_LOCAL		2
#define HID_ITEM_TYPE_RESERVED		3

/*
 * HID report descriptor main item tags
 */

#define HID_MAIN_ITEM_TAG_INPUT			8
#define HID_MAIN_ITEM_TAG_OUTPUT		9
#define HID_MAIN_ITEM_TAG_FEATURE		11
#define HID_MAIN_ITEM_TAG_BEGIN_COLLECTION	10
#define HID_MAIN_ITEM_TAG_END_COLLECTION	12

/*
 * HID report descriptor main item contents
 */

#define HID_MAIN_ITEM_CONSTANT		0x001
#define HID_MAIN_ITEM_VARIABLE		0x002
#define HID_MAIN_ITEM_RELATIVE		0x004
#define HID_MAIN_ITEM_WRAP		0x008
#define HID_MAIN_ITEM_NONLINEAR		0x010
#define HID_MAIN_ITEM_NO_PREFERRED	0x020
#define HID_MAIN_ITEM_NULL_STATE	0x040
#define HID_MAIN_ITEM_VOLATILE		0x080
#define HID_MAIN_ITEM_BUFFERED_BYTE	0x100

/*
 * HID report descriptor collection item types
 */

#define HID_COLLECTION_PHYSICAL		0
#define HID_COLLECTION_APPLICATION	1
#define HID_COLLECTION_LOGICAL		2

/*
 * HID report descriptor global item tags
 */

#define HID_GLOBAL_ITEM_TAG_USAGE_PAGE		0
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM	1
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM	2
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM	3
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM	4
#define HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT	5
#define HID_GLOBAL_ITEM_TAG_UNIT		6
#define HID_GLOBAL_ITEM_TAG_REPORT_SIZE		7
#define HID_GLOBAL_ITEM_TAG_REPORT_ID		8
#define HID_GLOBAL_ITEM_TAG_REPORT_COUNT	9
#define HID_GLOBAL_ITEM_TAG_PUSH		10
#define HID_GLOBAL_ITEM_TAG_POP			11

/*
 * HID report descriptor local item tags
 */

#define HID_LOCAL_ITEM_TAG_USAGE		0
#define HID_LOCAL_ITEM_TAG_USAGE_MINIMUM	1
#define HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM	2
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_INDEX	3
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MINIMUM	4
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MAXIMUM	5
#define HID_LOCAL_ITEM_TAG_STRING_INDEX		7
#define HID_LOCAL_ITEM_TAG_STRING_MINIMUM	8
#define HID_LOCAL_ITEM_TAG_STRING_MAXIMUM	9
#define HID_LOCAL_ITEM_TAG_DELIMITER		10

/*
 * HID usage tables
 */

#define HID_USAGE_PAGE		0xffff0000

#define HID_UP_UNDEFINED	0x00000000
#define HID_UP_GENDESK		0x00010000
#define HID_UP_SIMULATION	0x00020000
#define HID_UP_GENDEVCTRLS	0x00060000
#define HID_UP_KEYBOARD		0x00070000
#define HID_UP_LED		0x00080000
#define HID_UP_BUTTON		0x00090000
#define HID_UP_ORDINAL		0x000a0000
#define HID_UP_TELEPHONY	0x000b0000
#define HID_UP_CONSUMER		0x000c0000
#define HID_UP_DIGITIZER	0x000d0000
#define HID_UP_PID		0x000f0000
#define HID_UP_HPVENDOR         0xff7f0000
#define HID_UP_HPVENDOR2        0xff010000
#define HID_UP_MSVENDOR		0xff000000
#define HID_UP_CUSTOM		0x00ff0000
#define HID_UP_LOGIVENDOR	0xffbc0000
#define HID_UP_LNVENDOR		0xffa00000
#define HID_UP_SENSOR		0x00200000

#define HID_USAGE		0x0000ffff

#define HID_GD_POINTER		0x00010001
#define HID_GD_MOUSE		0x00010002
#define HID_GD_JOYSTICK		0x00010004
#define HID_GD_GAMEPAD		0x00010005
#define HID_GD_KEYBOARD		0x00010006
#define HID_GD_KEYPAD		0x00010007
#define HID_GD_MULTIAXIS	0x00010008
#define HID_GD_X		0x00010030
#define HID_GD_Y		0x00010031
#define HID_GD_Z		0x00010032
#define HID_GD_RX		0x00010033
#define HID_GD_RY		0x00010034
#define HID_GD_RZ		0x00010035
#define HID_GD_SLIDER		0x00010036
#define HID_GD_DIAL		0x00010037
#define HID_GD_WHEEL		0x00010038
#define HID_GD_HATSWITCH	0x00010039
#define HID_GD_BUFFER		0x0001003a
#define HID_GD_BYTECOUNT	0x0001003b
#define HID_GD_MOTION		0x0001003c
#define HID_GD_START		0x0001003d
#define HID_GD_SELECT		0x0001003e
#define HID_GD_VX		0x00010040
#define HID_GD_VY		0x00010041
#define HID_GD_VZ		0x00010042
#define HID_GD_VBRX		0x00010043
#define HID_GD_VBRY		0x00010044
#define HID_GD_VBRZ		0x00010045
#define HID_GD_VNO		0x00010046
#define HID_GD_FEATURE		0x00010047
#define HID_GD_SYSTEM_CONTROL	0x00010080
#define HID_GD_UP		0x00010090
#define HID_GD_DOWN		0x00010091
#define HID_GD_RIGHT		0x00010092
#define HID_GD_LEFT		0x00010093

#define HID_DC_BATTERYSTRENGTH	0x00060020

#define HID_CP_CONSUMER_CONTROL	0x000c0001

#define HID_DG_DIGITIZER	0x000d0001
#define HID_DG_PEN		0x000d0002
#define HID_DG_LIGHTPEN		0x000d0003
#define HID_DG_TOUCHSCREEN	0x000d0004
#define HID_DG_TOUCHPAD		0x000d0005
#define HID_DG_STYLUS		0x000d0020
#define HID_DG_PUCK		0x000d0021
#define HID_DG_FINGER		0x000d0022
#define HID_DG_TIPPRESSURE	0x000d0030
#define HID_DG_BARRELPRESSURE	0x000d0031
#define HID_DG_INRANGE		0x000d0032
#define HID_DG_TOUCH		0x000d0033
#define HID_DG_UNTOUCH		0x000d0034
#define HID_DG_TAP		0x000d0035
#define HID_DG_TABLETFUNCTIONKEY	0x000d0039
#define HID_DG_PROGRAMCHANGEKEY	0x000d003a
#define HID_DG_INVERT		0x000d003c
#define HID_DG_TIPSWITCH	0x000d0042
#define HID_DG_TIPSWITCH2	0x000d0043
#define HID_DG_BARRELSWITCH	0x000d0044
#define HID_DG_ERASER		0x000d0045
#define HID_DG_TABLETPICK	0x000d0046
#define HID_DG_CONFIDENCE	0x000d0047
#define HID_DG_WIDTH		0x000d0048
#define HID_DG_HEIGHT		0x000d0049
#define HID_DG_CONTACTID	0x000d0051
#define HID_DG_INPUTMODE	0x000d0052
#define HID_DG_DEVICEINDEX	0x000d0053
#define HID_DG_CONTACTCOUNT	0x000d0054
#define HID_DG_CONTACTMAX	0x000d0055
#define HID_DG_BARRELSWITCH2	0x000d005a
#define HID_DG_TOOLSERIALNUMBER	0x000d005b

#define HID_CP_CONSUMERCONTROL	0x000c0001

/*
 * HID report types --- Ouch! HID spec says 1 2 3!
 */

#define HID_INPUT_REPORT	0
#define HID_OUTPUT_REPORT	1
#define HID_FEATURE_REPORT	2

#define HID_REPORT_TYPES	3

/*
 * This is the global environment of the parser. This information is
 * persistent for main-items. The global environment can be saved and
 * restored with PUSH/POP statements.
 */

struct hid_global {
	unsigned usage_page;
	int32_t logical_minimum;
	int32_t logical_maximum;
	int32_t physical_minimum;
	int32_t physical_maximum;
	int32_t unit_exponent;
	unsigned unit;
	unsigned report_id;
	unsigned report_size;
	unsigned report_count;
};

/*
 * This is the local environment. It is persistent up the next main-item.
 */

#define HID_MAX_USAGES			12288
#define HID_DEFAULT_NUM_COLLECTIONS	16

struct hid_local {
	unsigned usage[HID_MAX_USAGES]; /* usage array */
	unsigned collection_index[HID_MAX_USAGES]; /* collection index array */
	unsigned usage_index;
	unsigned usage_minimum;
	unsigned delimiter_depth;
	unsigned delimiter_branch;
};

/*
 * This is the collection stack. We climb up the stack to determine
 * application and function of each field.
 */

struct hid_collection {
	unsigned type;
	unsigned usage;
	unsigned level;
};

struct hid_usage {
	unsigned  hid;			/* hid usage code */
	unsigned  collection_index;	/* index into collection array */
	unsigned  usage_index;		/* index into usage array */
	/* hidinput data */
	uint16_t     code;		/* input driver code */
	uint8_t      type;		/* input driver type */
	int8_t       hat_min;	/* hat switch fun */
	int8_t       hat_max;	/* ditto */
	int8_t       hat_dir;	/* ditto */
};

struct hid_field {
	unsigned  physical;	/* physical usage for this field */
	unsigned  logical;	/* logical usage for this field */
	unsigned  application;		/* application usage for this field */
	struct hid_usage *usage;	/* usage table for this function */
	unsigned  maxusage;	/* maximum usage index */
	unsigned  flags;	/* main-item flags */
	unsigned  report_offset;	/* bit offset in the report */
	unsigned  report_size;		/* size of this field in the report */
	unsigned  report_count;		/* number of this field in the report */
	unsigned  report_type;		/* (input,output,feature) */
	int32_t  *value;			/* last known value(s) */
	int32_t   logical_minimum;
	int32_t   logical_maximum;
	int32_t   physical_minimum;
	int32_t   physical_maximum;
	int32_t   unit_exponent;
	unsigned  unit;
	struct hid_report *report;	/* associated report */
	unsigned index;			/* index into report->field[] */
};

#define HID_MAX_FIELDS 256

struct hid_report {
	ListNode list;
	unsigned id;		/* id of this report */
	unsigned type;		/* report type */
	struct hid_field *field[HID_MAX_FIELDS]; /* fields of the report */
	unsigned maxfield;	/* maximum valid field index */
	unsigned size;		/* size of the report (bits) */
	struct i2chiddev *device;	/* associated device */
};

#define HID_MAX_IDS 256

struct hid_report_enum {
	unsigned numbered;
	ListNode report_list;
	struct hid_report *report_id_hash[HID_MAX_IDS];
};

#define HID_MIN_BUFFER_SIZE	64
#define HID_MAX_BUFFER_SIZE	4096

#define HID_GLOBAL_STACK_SIZE 4
#define HID_COLLECTION_STACK_SIZE 4

struct hid_parser {
	struct hid_global     global;
	struct hid_global     global_stack[HID_GLOBAL_STACK_SIZE];
	unsigned              global_stack_ptr;
	struct hid_local      local;
	unsigned              collection_stack[HID_COLLECTION_STACK_SIZE];
	unsigned              collection_stack_ptr;
	struct i2chiddev     *device;
	unsigned              scan_flags;
};

typedef struct {
	uint32_t current_x;
	uint32_t current_y;
	uint32_t current_z;
	uint32_t active_buttons;
	uint32_t contact_id;
} absolute_pointer_state;

typedef struct {
	uint16_t wHIDDescLength;
	uint16_t bcdVersion;
	uint16_t wReportDescLength;
	uint16_t wReportDescRegister;
	uint16_t wInputRegister;
	uint16_t wMaxInputLength;
	uint16_t wOutputRegister;
	uint16_t wMaxOutputLength;
	uint16_t wCommandRegister;
	uint16_t wDataRegister;
	uint16_t wVendorID;
	uint16_t wProductID;
	uint16_t wVersionID;
	uint32_t reserved;
} __attribute__ ((packed)) i2c_hid_descriptor_t;

typedef int (*i2c_hid_dev_int_sts) (void);
typedef int (*i2c_hid_dev_hw_reset) (void);
typedef int hid_parse_proc(struct hid_parser *parser, struct hid_item *item);

struct i2chiddev;
typedef struct i2chiddev i2chiddev_t;

struct i2chiddev {
	I2cOps *i2c;
	uint8_t slave_address;
	uint16_t hid_desc_address;
	i2c_hid_descriptor_t hid_descriptor;
	uint8_t *rdesc;
	struct hid_report_enum report_enum[HID_REPORT_TYPES];
	struct hid_collection *collection;
	uint32_t collection_size;
	uint32_t maxcollection;
	uint32_t maxapplication;
	i2c_hid_dev_int_sts int_sts;
	i2c_hid_dev_hw_reset hw_reset;
	CleanupFunc cleanup;
};

int i2c_hid_clean_data(i2chiddev_t *dev);
int i2c_hid_get_input_data(i2chiddev_t *dev, uint8_t *buffer);
int i2c_hid_set_idle(i2chiddev_t *dev);
int i2c_hid_set_power(i2chiddev_t *dev, uint8_t power_state);
int i2c_hid_set_reset(i2chiddev_t *dev);

i2chiddev_t *new_i2c_hid(
		I2cOps *i2c, uint8_t chip, uint16_t desc_addr,
		i2c_hid_dev_int_sts int_sts, i2c_hid_dev_hw_reset hw_reset);

#endif
