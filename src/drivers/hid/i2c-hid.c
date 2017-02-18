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

#include <libpayload.h>
#include "drivers/bus/i2c/i2c.h"
#include "drivers/hid/i2c-hid.h"

static int i2c_read_block(
	I2cOps *ops, uint8_t chip, uint8_t *reg,
	int reg_len, uint8_t *data, int data_len)
{
	I2cSeg seg[2];
	uint8_t i;
	I2cSeg *segments;
	int seg_count;

	seg[0].read = 0;
	seg[0].chip = chip;
	seg[0].buf = reg;
	seg[0].len = reg_len;
	seg[1].read = 1;
	seg[1].chip = chip;
	seg[1].buf = data;
	seg[1].len = data_len;

	seg_count = ARRAY_SIZE(seg);
	if (reg == NULL && reg_len == 0) {
		seg_count--;
		segments = &seg[1];
	} else {
		printf("cmd = ");
		for (i = 0; i < reg_len; i++)
			printf("%02x ", *(reg + i));
		printf(", len = %d\n", reg_len);
		segments = seg;
	}
	return ops->transfer(ops, segments, seg_count);
}

static int i2c_write_block(I2cOps *ops, uint8_t chip, uint8_t *reg, int reg_len)
{
	I2cSeg seg;
	uint8_t i;

	seg.read = 0;
	seg.chip = chip;
	seg.buf = reg;
	seg.len = reg_len;

	printf("cmd = ");
	for (i = 0; i < reg_len; i++)
		printf("%02x ", *(reg + i));
	printf(", len = %d\n", reg_len);
	return ops->transfer(ops, &seg, 1);
}

static int hid_snto32(uint32_t value, uint32_t n)
{
	switch (n) {
	case 8:
		return (char)value;
	case 16:
		return (short)value;
	case 32:
		return (int)value;
	}
	return value & (1 << (n - 1)) ? value | (-1 << n) : value;
}

/*
 * Register a new report for a device.
 */

struct hid_report *hid_register_report(
		i2chiddev_t *device, unsigned type, unsigned id)
{
	struct hid_report_enum *report_enum = device->report_enum + type;
	struct hid_report *report;

	if (id >= HID_MAX_IDS)
		return NULL;
	if (report_enum->report_id_hash[id])
		return report_enum->report_id_hash[id];

	report = xzalloc(sizeof(struct hid_report));
	if (!report)
		return NULL;

	if (id != 0)
		report_enum->numbered = 1;

	report->id = id;
	report->type = type;
	report->size = 0;
	report->device = device;
	report_enum->report_id_hash[id] = report;

	list_insert_after(&report->list, &report_enum->report_list);

	return report;
}

/*
 * Register a new field for this report.
 */

static struct hid_field *hid_register_field(
	struct hid_report *report, unsigned usages, unsigned values)
{
	struct hid_field *field;

	if (report->maxfield == HID_MAX_FIELDS) {
		printf("too many fields in report\n");
		return NULL;
	}

	field = xzalloc(sizeof(struct hid_field) +
				usages * sizeof(struct hid_usage) +
				values * sizeof(unsigned));
	if (!field)
		return NULL;

	field->index = report->maxfield++;
	report->field[field->index] = field;
	field->usage = (struct hid_usage *)(field + 1);
	field->value = (int32_t *)(field->usage + usages);
	field->report = report;

	return field;
}

/*
 * Open a collection. The type/usage is pushed on the stack.
 */

static int open_collection(struct hid_parser *parser, unsigned type)
{
	struct hid_collection *collection;
	unsigned usage;

	usage = parser->local.usage[0];

	if (parser->collection_stack_ptr == HID_COLLECTION_STACK_SIZE) {
		printf("collection stack overflow\n");
		return -1;
	}

	if (parser->device->maxcollection == parser->device->collection_size) {
		collection = xzalloc(sizeof(struct hid_collection) *
				parser->device->collection_size * 2);
		if (collection == NULL) {
			printf("failed to reallocate collection array\n");
			return -1;
		}
		memcpy(collection, parser->device->collection,
			sizeof(struct hid_collection) *
			parser->device->collection_size);
		memset(collection + parser->device->collection_size, 0,
			sizeof(struct hid_collection) *
			parser->device->collection_size);
		free(parser->device->collection);
		parser->device->collection = collection;
		parser->device->collection_size *= 2;
	}

	parser->collection_stack[parser->collection_stack_ptr++] =
		parser->device->maxcollection;

	collection = parser->device->collection +
		parser->device->maxcollection++;
	collection->type = type;
	collection->usage = usage;
	collection->level = parser->collection_stack_ptr - 1;

	if (type == HID_COLLECTION_APPLICATION)
		parser->device->maxapplication++;

	return 0;
}

/*
 * Close a collection.
 */

static int close_collection(struct hid_parser *parser)
{
	if (!parser->collection_stack_ptr) {
		printf("collection stack underflow\n");
		return -1;
	}
	parser->collection_stack_ptr--;
	return 0;
}

/*
 * Climb up the stack, search for the specified collection type
 * and return the usage.
 */

static unsigned hid_lookup_collection(struct hid_parser *parser, unsigned type)
{
	struct hid_collection *collection = parser->device->collection;
	int n;

	for (n = parser->collection_stack_ptr - 1; n >= 0; n--) {
		unsigned index = parser->collection_stack[n];

		if (collection[index].type == type)
			return collection[index].usage;
	}
	return 0; /* we know nothing about this usage type */
}

/*
 * Add a usage to the temporary parser table.
 */

static int hid_add_usage(struct hid_parser *parser, unsigned usage)
{
	if (parser->local.usage_index >= HID_MAX_USAGES) {
		printf("usage index exceeded\n");
		return -1;
	}
	parser->local.usage[parser->local.usage_index] = usage;
	parser->local.collection_index[parser->local.usage_index] =
		parser->collection_stack_ptr ?
		parser->collection_stack[parser->collection_stack_ptr - 1] : 0;
	parser->local.usage_index++;
	return 0;
}

/*
 * Register a new field for this report.
 */

static int hid_add_field(
		struct hid_parser *parser, unsigned report_type, unsigned flags)
{
	struct hid_report *report;
	struct hid_field *field;
	unsigned usages;
	unsigned offset;
	unsigned i;

	report = hid_register_report(
			parser->device, report_type, parser->global.report_id);
	if (!report) {
		printf("hid_register_report failed\n");
		return -1;
	}

	/* Handle both signed and unsigned cases properly */
	if ((parser->global.logical_minimum < 0 &&
		parser->global.logical_maximum <
		parser->global.logical_minimum) ||
		(parser->global.logical_minimum >= 0 &&
		(uint32_t)parser->global.logical_maximum <
		(uint32_t)parser->global.logical_minimum)) {
		printf("logical range invalid 0x%x 0x%x\n",
			parser->global.logical_minimum,
			parser->global.logical_maximum);
		return -1;
	}

	offset = report->size;
	report->size +=
		parser->global.report_size * parser->global.report_count;

	if (!parser->local.usage_index) /* Ignore padding fields */
		return 0;

	usages = max_t(unsigned, parser->local.usage_index,
				parser->global.report_count);

	field = hid_register_field(report, usages, parser->global.report_count);
	if (!field)
		return 0;

	field->physical =
		hid_lookup_collection(parser, HID_COLLECTION_PHYSICAL);
	field->logical =
		hid_lookup_collection(parser, HID_COLLECTION_LOGICAL);
	field->application =
		hid_lookup_collection(parser, HID_COLLECTION_APPLICATION);

	for (i = 0; i < usages; i++) {
		unsigned j = i;
		/*
		 * Duplicate the last usage we parsed if we have excess values
		 */
		if (i >= parser->local.usage_index)
			j = parser->local.usage_index - 1;
		field->usage[i].hid = parser->local.usage[j];
		field->usage[i].collection_index =
			parser->local.collection_index[j];
		field->usage[i].usage_index = i;
	}

	field->maxusage = usages;
	field->flags = flags;
	field->report_offset = offset;
	field->report_type = report_type;
	field->report_size = parser->global.report_size;
	field->report_count = parser->global.report_count;
	field->logical_minimum = parser->global.logical_minimum;
	field->logical_maximum = parser->global.logical_maximum;
	field->physical_minimum = parser->global.physical_minimum;
	field->physical_maximum = parser->global.physical_maximum;
	field->unit_exponent = parser->global.unit_exponent;
	field->unit = parser->global.unit;

	return 0;
}

/*
 * Read data value from item.
 */

static uint32_t item_udata(struct hid_item *item)
{
	switch (item->size) {
	case 1:
		return item->data.u8;
	case 2:
		return item->data.u16;
	case 3:
		return item->data.u32;
	}
	return 0;
}

static int item_sdata(struct hid_item *item)
{
	switch (item->size) {
	case 1:
		return item->data.s8;
	case 2:
		return item->data.s16;
	case 3:
		return item->data.s32;
	}
	return 0;
}

/*
 * Process a global item.
 */

static int hid_parser_global(struct hid_parser *parser, struct hid_item *item)
{
	int raw_value;

	switch (item->tag) {
	case HID_GLOBAL_ITEM_TAG_PUSH:
		if (parser->global_stack_ptr == HID_GLOBAL_STACK_SIZE) {
			printf("global environment stack overflow\n");
			return -1;
		}
		memcpy(parser->global_stack + parser->global_stack_ptr++,
			&parser->global, sizeof(struct hid_global));
		return 0;
	case HID_GLOBAL_ITEM_TAG_POP:
		if (!parser->global_stack_ptr) {
			printf("global environment stack underflow\n");
			return -1;
		}
		memcpy(&parser->global, parser->global_stack +
			--parser->global_stack_ptr, sizeof(struct hid_global));
		return 0;
	case HID_GLOBAL_ITEM_TAG_USAGE_PAGE:
		parser->global.usage_page = item_udata(item);
		return 0;
	case HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM:
		parser->global.logical_minimum = item_sdata(item);
		return 0;
	case HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM:
		if (parser->global.logical_minimum < 0)
			parser->global.logical_maximum = item_sdata(item);
		else
			parser->global.logical_maximum = item_udata(item);
		return 0;
	case HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM:
		parser->global.physical_minimum = item_sdata(item);
		return 0;
	case HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM:
		if (parser->global.physical_minimum < 0)
			parser->global.physical_maximum = item_sdata(item);
		else
			parser->global.physical_maximum = item_udata(item);
		return 0;
	case HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT:
		/* Many devices provide unit exponent as a two's complement
		 * nibble due to the common misunderstanding of HID
		 * specification 1.11, 6.2.2.7 Global Items. Attempt to handle
		 * both this and the standard encoding. */
		raw_value = item_sdata(item);
		if (!(raw_value & 0xfffffff0))
			parser->global.unit_exponent = hid_snto32(raw_value, 4);
		else
			parser->global.unit_exponent = raw_value;
		return 0;
	case HID_GLOBAL_ITEM_TAG_UNIT:
		parser->global.unit = item_udata(item);
		return 0;
	case HID_GLOBAL_ITEM_TAG_REPORT_SIZE:
		parser->global.report_size = item_udata(item);
		if (parser->global.report_size > 128) {
			printf("invalid report_size %d\n",
					parser->global.report_size);
			return -1;
		}
		return 0;
	case HID_GLOBAL_ITEM_TAG_REPORT_COUNT:
		parser->global.report_count = item_udata(item);
		if (parser->global.report_count > HID_MAX_USAGES) {
			printf("invalid report_count %d\n",
					parser->global.report_count);
			return -1;
		}
		return 0;
	case HID_GLOBAL_ITEM_TAG_REPORT_ID:
		parser->global.report_id = item_udata(item);
		if (parser->global.report_id == 0 ||
			parser->global.report_id >= HID_MAX_IDS) {
			printf("report_id %u is invalid\n",
				parser->global.report_id);
			return -1;
		}
		return 0;
	default:
		printf("unknown global tag 0x%x\n", item->tag);
		return -1;
	}
}

/*
 * Process a local item.
 */

static int hid_parser_local(struct hid_parser *parser, struct hid_item *item)
{
	uint32_t data;
	unsigned n;

	data = item_udata(item);

	switch (item->tag) {
	case HID_LOCAL_ITEM_TAG_DELIMITER:
		if (data) {
			/*
			 * We treat items before the first delimiter
			 * as global to all usage sets (branch 0).
			 * In the moment we process only these global
			 * items and the first delimiter set.
			 */
			if (parser->local.delimiter_depth != 0) {
				printf("nested delimiters\n");
				return -1;
			}
			parser->local.delimiter_depth++;
			parser->local.delimiter_branch++;
		} else {
			if (parser->local.delimiter_depth < 1) {
				printf("bogus close delimiter\n");
				return -1;
			}
			parser->local.delimiter_depth--;
		}
		return 0;
	case HID_LOCAL_ITEM_TAG_USAGE:
		if (parser->local.delimiter_branch > 1) {
			printf("alternative usage ignored\n");
			return 0;
		}
		if (item->size <= 2)
			data = (parser->global.usage_page << 16) + data;
		return hid_add_usage(parser, data);
	case HID_LOCAL_ITEM_TAG_USAGE_MINIMUM:
		if (parser->local.delimiter_branch > 1) {
			printf("alternative usage ignored\n");
			return 0;
		}
		if (item->size <= 2)
			data = (parser->global.usage_page << 16) + data;
		parser->local.usage_minimum = data;
		return 0;
	case HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM:
		if (parser->local.delimiter_branch > 1) {
			printf("alternative usage ignored\n");
			return 0;
		}
		if (item->size <= 2)
			data = (parser->global.usage_page << 16) + data;
		for (n = parser->local.usage_minimum; n <= data; n++) {
			if (hid_add_usage(parser, n)) {
				printf("hid_add_usage failed\n");
				return -1;
			}
		}
		return 0;
	default:
		printf("unknown local item tag 0x%x\n", item->tag);
		return 0;
	}
	return 0;
}

/*
 * Process a main item.
 */

static int hid_parser_main(struct hid_parser *parser, struct hid_item *item)
{
	uint32_t data;
	int ret;

	data = item_udata(item);

	switch (item->tag) {
	case HID_MAIN_ITEM_TAG_BEGIN_COLLECTION:
		ret = open_collection(parser, data & 0xff);
		break;
	case HID_MAIN_ITEM_TAG_END_COLLECTION:
		ret = close_collection(parser);
		break;
	case HID_MAIN_ITEM_TAG_INPUT:
		ret = hid_add_field(parser, HID_INPUT_REPORT, data);
		break;
	case HID_MAIN_ITEM_TAG_OUTPUT:
		ret = hid_add_field(parser, HID_OUTPUT_REPORT, data);
		break;
	case HID_MAIN_ITEM_TAG_FEATURE:
		ret = hid_add_field(parser, HID_FEATURE_REPORT, data);
		break;
	default:
		printf("unknown main item tag 0x%x\n", item->tag);
		ret = 0;
		break;
	}

	/* Reset the local parser environment */
	memset(&parser->local, 0, sizeof(parser->local));

	return ret;
}

/*
 * Process a reserved item.
 */

static int hid_parser_reserved(struct hid_parser *parser, struct hid_item *item)
{
	printf("reserved item type, tag 0x%x\n", item->tag);
	return 0;
}

static int get_hid_descriptor(i2chiddev_t *dev)
{
	return i2c_read_block(
			dev->i2c,
			dev->slave_address,
			(uint8_t *)&dev->hid_desc_address,
			2,
			(uint8_t *)&dev->hid_descriptor,
			I2C_HID_DESCROPTOR_LENGTH
			);
}

static int get_report_descriptor(i2chiddev_t *dev)
{
	return i2c_read_block(
			dev->i2c,
			dev->slave_address,
			(uint8_t *)&dev->hid_descriptor.wReportDescRegister,
			2,
			(uint8_t *)dev->rdesc,
			dev->hid_descriptor.wReportDescLength
			);
}

static int hid_open_report(i2chiddev_t *dev)
{
	struct hid_parser *parser;
	uint16_t len;
	uint8_t *report_descriptor;
	struct hid_item item;
	uint8_t data;
	hid_parse_proc *dispatch_type[] = {
			hid_parser_main,
			hid_parser_global,
			hid_parser_local,
			hid_parser_reserved
		};

	parser = xzalloc(sizeof(struct hid_parser));
	if (!parser)
		return -1;

	parser->device = dev;

	report_descriptor = dev->rdesc;

	for (len = dev->hid_descriptor.wReportDescLength; len > 0; len--) {
		data = *report_descriptor++;

		/* fetch item */
		item.size = data & 3;
		item.type = (data >> 2) & 3;
		item.tag = (data >> 4) & 15;

		switch (item.size) {
		case 1:
			item.data.u8 = *report_descriptor++;
			len--;
			break;
		case 2:
			item.data.u16 = *(uint16_t *)report_descriptor;
			report_descriptor += 2;
			len -= 2;
			break;
		case 3:
			item.data.u32 = *(uint32_t *)report_descriptor;
			report_descriptor += 4;
			len -= 4;
			break;
		default:
			break;
		}

		/* parsing hid item */
		dispatch_type[item.type](parser, &item);
	}

	free(parser);

	return 0;
}

int i2c_hid_get_input_data(i2chiddev_t *dev, uint8_t *buffer)
{
	return i2c_read_block(
			dev->i2c,
			dev->slave_address,
			NULL,
			0,
			buffer,
			dev->hid_descriptor.wMaxInputLength
			);
}

static int get_report(i2chiddev_t *dev, uint8_t reportID,
		uint8_t reportType, uint8_t *args, int args_len,
		uint8_t *buf_recv, int data_len)

{
	uint8_t write_data[7];
	uint16_t *cmd_reg = (uint16_t *)write_data;

	*cmd_reg = dev->hid_descriptor.wCommandRegister;
	write_data[2] = reportID | reportType << 4;
	write_data[3] = HID_GET_REPORT;
	memcpy(write_data + 4, args, args_len);

	return i2c_read_block(
			dev->i2c,
			dev->slave_address,
			write_data,
			args_len + 4,
			buf_recv,
			data_len
			);
}

static int i2c_hid_get_report(i2chiddev_t *dev, uint8_t reportType,
		uint8_t reportID, uint8_t *buf_recv, int data_len)
{
	uint8_t args[3];
	int ret;
	int args_len = 0;
	uint16_t readRegister = letohw(dev->hid_descriptor.wDataRegister);

	if (reportID >= 0x0F) {
		args[args_len++] = reportID;
		reportID = 0x0F;
	}

	args[args_len++] = readRegister & 0xFF;
	args[args_len++] = readRegister >> 8;

	ret = get_report(dev, reportID,
			reportType, args, args_len, buf_recv, data_len);
	if (ret) {
		printf("failed to retrieve report from device.\n");
		return ret;
	}

	return 0;
}

static int i2c_hid_get_report_length(struct hid_report *report)
{
	return ((report->size - 1) >> 3) + 1 +
		report->device->report_enum[report->type].numbered + 2;
}

static void i2c_hid_init_report(struct hid_report *report, uint8_t *buffer,
	size_t bufsize)
{
	i2chiddev_t *dev = report->device;
	unsigned int size, ret_size;

	size = i2c_hid_get_report_length(report);
	if (i2c_hid_get_report(dev,
			report->type == HID_FEATURE_REPORT ? 0x03 : 0x01,
			report->id, buffer, size))
		return;

	ret_size = buffer[0] | (buffer[1] << 8);

	if (ret_size != size) {
		printf("error in %s size:%d / ret_size:%d\n",
			__func__, size, ret_size);
		return;
	}
}

/*
 * Initialize all reports
 */
static void i2c_hid_init_reports(i2chiddev_t *dev, uint32_t bufsize)
{
	struct hid_report *report;
	uint8_t *inbuf = xzalloc(bufsize);

	if (!inbuf) {
		printf("can not retrieve initial reports\n");
		free(inbuf);
		return;
	}

	list_for_each(report,
		dev->report_enum[HID_FEATURE_REPORT].report_list, list)
		i2c_hid_init_report(report, inbuf, bufsize);

	free(inbuf);
}

static void i2c_hid_find_max_report(i2chiddev_t *dev, unsigned int type,
		unsigned int *max)
{
	struct hid_report *report;
	unsigned int size;

	/* We should not rely on wMaxInputLength, as some devices may set it to
	 * a wrong length. */
	list_for_each(report, dev->report_enum[type].report_list, list) {
		size = i2c_hid_get_report_length(report);
		if (*max < size)
			*max = size;
	}
}

static int i2c_hid_start(i2chiddev_t *dev)
{
	unsigned int bufsize = HID_MIN_BUFFER_SIZE;

	i2c_hid_find_max_report(dev, HID_INPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(dev, HID_OUTPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(dev, HID_FEATURE_REPORT, &bufsize);

	i2c_hid_init_reports(dev, bufsize);

	return 0;
}

static int configure_hid_device(i2chiddev_t *dev)
{
	dev->rdesc = xzalloc(dev->hid_descriptor.wReportDescLength);
	if (dev->rdesc == NULL)
		return -1;

	if (get_report_descriptor(dev))
		return -1;

	dev->collection = xzalloc(sizeof(struct hid_collection) *
		HID_DEFAULT_NUM_COLLECTIONS);
	if (dev->collection == NULL)
		return -1;

	dev->collection_size = HID_DEFAULT_NUM_COLLECTIONS;

	if (hid_open_report(dev))
		return -1;

	return 0;
}

int i2c_hid_clean_data(i2chiddev_t *dev)
{
	int ret = 0;
	uint8_t timeout = 30;
	uint8_t *in_buf;


	while (!dev->int_sts()) {
		printf("waiting for interrupt ...\n");
		mdelay(1);
		timeout--;
		if (!timeout)
			return -1;
	}

	in_buf = xzalloc(dev->hid_descriptor.wMaxInputLength);

	timeout = 30;
	/*
	 * After device reset, read data from device until INT pin become high.
	 * Clean up all data in device.
	 */
	while (dev->int_sts()) {
		ret = i2c_hid_get_input_data(dev, in_buf);
		if (ret) {
			printf("get vkb data error: 0x%x\n", ret);
			free(in_buf);
			return ret;
		}

		timeout--;
		/*
		 * If there are too much data in device and can't clean them
		 * all. Just leave it, don't hang here. Let Application handle
		 * these data.
		 */
		if (!timeout) {
			printf("too much data keep in device during initial\n");
			break;
		}
	}

	free(in_buf);
	return ret;
}

int i2c_hid_set_power(i2chiddev_t *dev, uint8_t power_state)
{
	uint8_t write_data[4];
	uint16_t *cmd_reg = (uint16_t *)write_data;

	*cmd_reg = dev->hid_descriptor.wCommandRegister;
	write_data[2] = power_state;
	write_data[3] = HID_SET_POWER;

	return i2c_write_block(
			dev->i2c,
			dev->slave_address,
			write_data,
			4
			);
}

int i2c_hid_set_reset(i2chiddev_t *dev)
{
	uint8_t write_data[4];
	uint16_t *cmd_reg = (uint16_t *)write_data;

	*cmd_reg = dev->hid_descriptor.wCommandRegister;
	write_data[2] = 0x00;
	write_data[3] = HID_RESET;

	return i2c_write_block(
			dev->i2c,
			dev->slave_address,
			write_data,
			4
			);

}

int i2c_hid_set_idle(i2chiddev_t *dev)
{
	return 0;
}

static int i2c_hid_fetch_hid_descriptor(i2chiddev_t *dev)
{
	uint32_t dsize;

	/* i2c hid fetch using a fixed descriptor size (30 bytes) */
	if (get_hid_descriptor(dev))
		return -1;

	/* Validate the length of HID descriptor, the 4 first bytes:
	 * bytes 0-1 -> length
	 * bytes 2-3 -> bcdVersion (has to be 1.00) */
	/* check bcdVersion == 1.0 */
	if (letohw(dev->hid_descriptor.bcdVersion) != 0x0100) {
		printf("unexpected HID descriptor bcdVersion (0x%04hx)\n",
			letohw(dev->hid_descriptor.bcdVersion));
		return -1;
	}

	/* Descriptor length should be 30 bytes as per the specification */
	dsize = letohw(dev->hid_descriptor.wHIDDescLength);
	if (dsize != sizeof(i2c_hid_descriptor_t)) {
		printf("weird size of HID descriptor (%u)\n", dsize);
		return -1;
	}
	return 0;
}

static int i2c_hid_init(i2chiddev_t *dev)
{
	if (i2c_hid_fetch_hid_descriptor(dev)) {
		printf("fetching HID descriptor failed\n");
		return -1;
	}

	if (i2c_hid_set_power(dev, 0))
		printf("set power state to ON failed\n");

	if (i2c_hid_set_reset(dev))
		printf("reset virtual keyboarde failed\n");

	i2c_hid_clean_data(dev);

	if (!configure_hid_device(dev))
		i2c_hid_start(dev);

	return 0;
}

/*
 * Free a report and all registered fields. The field->usage and
 * field->value table's are allocated behind the field, so we need
 * only to free(field) itself.
 */

static void hid_free_report(struct hid_report *report)
{
	unsigned n;

	for (n = 0; n < report->maxfield; n++)
		free(report->field[n]);
	free(report);
}

static int i2c_hid_cleanup(CleanupFunc *cleanup, CleanupType type)
{
	unsigned i, j;
	i2chiddev_t *device = cleanup->data;

	for (i = 0; i < HID_REPORT_TYPES; i++) {
		struct hid_report_enum *report_enum = device->report_enum + i;

		for (j = 0; j < HID_MAX_IDS; j++) {
			struct hid_report *report =
				report_enum->report_id_hash[j];

			if (report)
				hid_free_report(report);
		}
		memset(report_enum, 0, sizeof(*report_enum));
	}

	free(device->rdesc);
	device->rdesc = NULL;

	free(device->collection);
	device->collection = NULL;
	device->collection_size = 0;
	device->maxcollection = 0;
	device->maxapplication = 0;

	return 0;
}

static void register_i2c_hid_cleanup(i2chiddev_t *i2chiddev)
{
	/* Register callback to free memory on exit */
	i2chiddev->cleanup.cleanup = &i2c_hid_cleanup;
	i2chiddev->cleanup.types = CleanupOnHandoff | CleanupOnLegacy;
	i2chiddev->cleanup.data = i2chiddev;
	list_insert_after(&i2chiddev->cleanup.list_node, &cleanup_funcs);
}

i2chiddev_t *new_i2c_hid(
	I2cOps *i2c, uint8_t chip, uint16_t desc_addr,
	i2c_hid_dev_int_sts int_sts, i2c_hid_dev_hw_reset hw_reset)
{
	i2chiddev_t *dev = xzalloc(sizeof(*dev));

	if (dev != NULL) {
		dev->i2c = i2c;
		dev->slave_address = chip;
		dev->hid_desc_address = desc_addr;
		dev->int_sts = int_sts;
		dev->hw_reset = hw_reset;

		if (i2c_hid_init(dev)) {
			free(dev);
			dev = NULL;
		} else {
			register_i2c_hid_cleanup(dev);
		}
	}

	return dev;
}
