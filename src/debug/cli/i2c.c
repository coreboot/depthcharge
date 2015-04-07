/*
 * Command for controlling i2c interfaces.
 *
 * Copyright 2015 Chromium OS Authors
 */
#include "common.h"
#include "base/list.h"
#include "drivers/bus/i2c/i2c.h"

static ListNode i2c_bus_controllers;

/* Use this structure to link i2c controllers' descriptiors together. */
typedef struct {
	ListNode list_node;
	char i2c_name[20]; /* Optional, interface symbolic name. */
	I2cOps *ops;
} I2cBusController;

static I2cBusController *picked_i2c_bus_controller;

void add_i2c_controller_to_list(I2cOps *ops, const char *fmt, ...)
{
	I2cBusController *list_el = xzalloc(sizeof(*list_el));
	va_list args;

	va_start(args, fmt);
	vsnprintf(list_el->i2c_name, sizeof(list_el->i2c_name), fmt, args);
	va_end(args);

	list_el->ops = ops;
	list_insert_after(&list_el->list_node, &i2c_bus_controllers);
}

static int pick_bus(int argc, const char *bus_name)
{
	I2cBusController *controller;

	if (argc < 3) {
		/* Show currently used controller */
		if (picked_i2c_bus_controller)
			printf("Using bus %s\n",
			       picked_i2c_bus_controller->i2c_name);
		else
			printf("No i2c bus picked yet\n");
		return CMD_RET_SUCCESS;
	}

	list_for_each(controller, i2c_bus_controllers, list_node)
		if (!strcmp(bus_name, controller->i2c_name)) {
			picked_i2c_bus_controller = controller;
			printf("Will use bus %s\n",
			       picked_i2c_bus_controller->i2c_name);
			return CMD_RET_SUCCESS;
		}

	printf("Bus %s not found\n", bus_name);
	return CMD_RET_FAILURE;
}

static int read_bus(uint8_t dev, uint8_t reg, int print_enabled)
{
	uint8_t data;
	int ret;
	I2cSeg seg[2];

	seg[0].read = 0;
	seg[0].buf = &reg;
	seg[0].len = 1;
	seg[0].chip = dev;

	seg[1].read = 1;
	seg[1].buf = &data;
	seg[1].len = 1;
	seg[1].chip = dev;

	ret = picked_i2c_bus_controller->ops->transfer
		(picked_i2c_bus_controller->ops, seg, ARRAY_SIZE(seg));
	if (ret) {
		if (print_enabled)
			printf ("i2c read of dev %#2.2x reg %#x failed (%d)\n",
				dev, reg, ret);
		return CMD_RET_FAILURE;
	}

	if (print_enabled)
		printf ("i2c read %#2.2x from dev %#2.2x reg %#x\n",
			data, dev, reg);

	return CMD_RET_SUCCESS;
}

static int scan_bus(void)
{
	int address;

	printf("Will scan bus %s\n", picked_i2c_bus_controller->i2c_name);

	if (picked_i2c_bus_controller->ops->scan_mode_on_off)
		picked_i2c_bus_controller->ops->scan_mode_on_off
			(picked_i2c_bus_controller->ops, 1);

	/* Valid addresses on the i2c bus are in the 8..119 range. */
	for (address = 8; address < 120; address++)
		if (read_bus(address, 0, 0) == CMD_RET_SUCCESS)
			printf ("%#2.2x ", address);

	if (picked_i2c_bus_controller->ops->scan_mode_on_off)
		picked_i2c_bus_controller->ops->scan_mode_on_off
			(picked_i2c_bus_controller->ops, 0);

	printf("\n");
	return CMD_RET_SUCCESS;
}

static int write_bus(uint8_t dev, uint8_t reg,
		     char * const *params, int count)
{
	int ret;
	I2cSeg seg;
	uint8_t buffer[1 + count];
	int i;

	buffer[0] = reg;

	for (i = 0; i < count; i++)
		buffer[i + 1] = strtoul(params[i], 0, 16);

	seg.read = 0;
	seg.buf = buffer;
	seg.len = count + 1;
	seg.chip = dev;

	ret = picked_i2c_bus_controller->ops->transfer
		(picked_i2c_bus_controller->ops, &seg, 1);
	if (ret) {
		printf ("i2c write of dev %#2.2x reg %#x failed (%d)\n",
			dev, reg, ret);
		return CMD_RET_FAILURE;
	}

	printf ("i2c wrote");

	for (i = 0; i < count; i++)
		printf(" %2.2x", buffer[i + 1]);

	printf(" to dev %#2.2x reg %#x\n", dev, reg);

	return CMD_RET_SUCCESS;
}

static int do_i2c(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "list")) {
		I2cBusController *controller;

		printf("Availabe i2c interfaces:\n");

		list_for_each(controller, i2c_bus_controllers, list_node)
			printf("  %s\n", controller->i2c_name);

		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[1], "pick"))
		return pick_bus(argc, argv[2]);

	if (!picked_i2c_bus_controller) {
		picked_i2c_bus_controller = container_of
			(i2c_bus_controllers.next,
			 I2cBusController, list_node);

		printf("will use bus %s\n",
		       picked_i2c_bus_controller->i2c_name);
	}

	if (!strcmp(argv[1], "scan"))
		return scan_bus();

	if (!strcmp(argv[1], "read")) {
		if (argc < 4)
			return CMD_RET_USAGE;
		return read_bus((uint8_t)strtoul(argv[2], 0, 16),
				(uint8_t)strtoul(argv[3], 0, 16),
				1);
	}

	if (!strcmp(argv[1], "write")) {
		if (argc < 5)
			return CMD_RET_USAGE;
		return write_bus((uint8_t)strtoul(argv[2], 0, 16),
				(uint8_t)strtoul(argv[3], 0, 16),
				 argv + 4, argc - 4);
	}
	return CMD_RET_FAILURE;
}

U_BOOT_CMD(
	i2c,	100,	1,
	"i2c interfaces utilities",
	"\n"
	"  list             - show available i2c interfaces\n"
	"  pick [<if name>] - pick interface to work with\n"
	"  read <dev addr> <reg>\n"
	"                   - read one byte from <dev addr> <reg>\n"
	"  scan             - scan interface for devices\n"
	"  write <dev addr> <reg> <data>\n"
	"                   - write <data> into <reg> on <dev addr>\n"
);
