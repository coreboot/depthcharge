// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Depthcharge driver for using the Android Open Accessory (AOA) protocol to
 * implement a connected Android device as a storage backend for recovery.
 */

#include <assert.h>
#include <libpayload.h>
#include <usb/usb.h>
#include <usb/usbmsc.h>
#include <vb2_api.h>

#include "boot/commandline.h"
#include "drivers/bus/usb/usb.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/init_funcs.h"

#define USB_VID_GOOGLE		0x18d1
#define USB_PID_AOA		0x2d00
#define USB_PID_AOA_ADB		0x2d01

#define AOA_GET_PROTOCOL	51
#define AOA_SET_STRING		52
#define AOA_START		53

#define AOA_STRING_MANUFACTURER	0
#define AOA_STRING_MODEL	1
#define AOA_STRING_DESCRIPTION	2
#define AOA_STRING_VERSION	3
#define AOA_STRING_URI		4
#define AOA_STRING_SERIAL	5	/* no longer used since version 1.1 */

#define CROS_AOA_INFO_MAGIC "CrOSaoa1"
#define CROS_AOA_INFO_FLAG_IS_DEV_SIGNED	BIT(0)
#define CROS_AOA_INFO_FLAG_IN_DEVELOPER_MODE	BIT(1)

/* All values in little-endian. */
struct cros_aoa_info_block {
	uint8_t magic[8];
	uint16_t size;
	uint8_t flags;
	uint8_t recovery_reason;
	uint32_t kernel_rollback_version;
	uint32_t gbb_flags;
	char hwid[VB2_GBB_HWID_MAX_SIZE];	/* packet cuts off after '\0' */
};

struct cros_aoa_drvdata {
	struct cros_aoa_info_block info_block;
	endpoint_t *endpoint;
	uint64_t last_try;
};

const char manufacturer[] = "Google";
const char model[]	= "Chrome OS Recovery";
const char description[] = "Chrome OS device in Recovery Mode";
const char version[]	= "1.1";
const char uri[]	= "https://google.com/chromeos/recovery_android";

static void aoa_recovery_poll(usbdev_t *dev)
{
	uint64_t start = timer_us(0);
	struct cros_aoa_drvdata *drvdata =
				((GenericUsbDevice *)dev->data)->dev_data;

	/* Give system UI a one second "breather" between transfer attempts. */
	if (start - drvdata->last_try < 1 * USECS_PER_SEC)
		return;

	size_t size = le16toh(drvdata->info_block.size);
	if (dev->controller->bulk(drvdata->endpoint, size,
				  (void *)&drvdata->info_block, 0) == size) {
		printf("AOA device %d switching to MSC communication\n",
		       dev->address);
		free(dev->data);
		free(drvdata);
		// Will overwrite dev->poll so we'll never get called again.
		usb_msc_force_init(dev,
			USB_MSC_QUIRK_NO_LUNS | USB_MSC_QUIRK_NO_RESET);
		return;
	}

	drvdata->last_try = timer_us(0);
	if (drvdata->last_try - start >= USB_MAX_PROCESSING_TIME_US) {
		printf("AOA device %d didn't accept info block, will retry...\n",
		       dev->address);
	} else {
		printf("ERROR: Cannot send CrOS AOA info block to device %d\n",
		       dev->address);
		usb_detach_device(dev->controller, dev->address);
	}
}

static int aoa_recovery_setup(GenericUsbDevice *dev)
{
	struct cros_aoa_drvdata *drvdata = xzalloc(sizeof(*drvdata));
	struct vb2_context *ctx = vboot_get_context();
	uint32_t hwid_size = VB2_GBB_HWID_MAX_SIZE;

	printf("USB device %d connected in AOA mode!\n", dev->dev->address);

	for (drvdata->endpoint = dev->dev->endpoints;
	     drvdata->endpoint < dev->dev->endpoints + dev->dev->num_endp;
	     drvdata->endpoint++) {
		if (drvdata->endpoint->type == BULK &&
		    drvdata->endpoint->direction == OUT)
			break;
	}
	if (drvdata->endpoint >= dev->dev->endpoints + dev->dev->num_endp) {
		printf("ERROR: Cannot find BULK_OUT EP for AOA device %d\n",
		       dev->dev->address);
		free(drvdata);
		return 0;
	}
	drvdata->last_try = 0;

	struct cros_aoa_info_block *info_block = &drvdata->info_block;
	memcpy(info_block->magic, CROS_AOA_INFO_MAGIC,
	       sizeof(info_block->magic));
	if (vb2api_is_developer_signed(ctx))
		info_block->flags |= CROS_AOA_INFO_FLAG_IS_DEV_SIGNED;
	if (ctx->flags & VB2_CONTEXT_DEVELOPER_MODE)
		info_block->flags |= CROS_AOA_INFO_FLAG_IN_DEVELOPER_MODE;
	info_block->recovery_reason = vb2api_get_recovery_reason(ctx);
	info_block->kernel_rollback_version =
		htole32(vb2api_get_kernel_rollback_version(ctx));
	info_block->gbb_flags = htole32(vb2api_gbb_get_flags(ctx));
	if (vb2api_gbb_read_hwid(ctx, info_block->hwid, &hwid_size)) {
		info_block->hwid[0] = '\0';
		hwid_size = 1;
	}

	info_block->size = htole16(offsetof(struct cros_aoa_info_block, hwid)
				   + hwid_size);

	dev->dev->poll = aoa_recovery_poll;
	dev->dev_data = drvdata;
	return 1;
}

// usbdev_t->destroy is overridden by the MSC stack, so this is only called if
// we disconnect while probing and we still need to clean up our drvdata.
static void aoa_recovery_remove(GenericUsbDevice *dev)
{
	free(dev->dev_data);
}

static int aoa_get_protocol(GenericUsbDevice *dev)
{
	uint16_t protocol;
	dev_req_t get_protocol = {
		.req_recp = dev_recp,
		.req_type = vendor_type,
		.data_dir = device_to_host,
		.bRequest = AOA_GET_PROTOCOL,
		.wValue = 0,
		.wIndex = 0,
		.wLength = sizeof(protocol),
	};

	int ret = dev->dev->controller->control(dev->dev, IN,
					sizeof(get_protocol), &get_protocol,
					sizeof(protocol), (void *)&protocol);

	if (ret < 0)
		return ret;
	else
		return protocol;
}

static int aoa_set_string(GenericUsbDevice *dev, uint16_t type, const char *str)
{
	size_t len = strlen(str) + 1;
	assert(len < 1 << (sizeof(uint16_t) * 8));
	dev_req_t set_string = {
		.req_recp = dev_recp,
		.req_type = vendor_type,
		.data_dir = host_to_device,
		.bRequest = AOA_SET_STRING,
		.wValue = 0,
		.wIndex = type,
		.wLength = len,
	};

	return dev->dev->controller->control(dev->dev, OUT, sizeof(set_string),
					     &set_string, len, (void *)str);
}

static int aoa_start(GenericUsbDevice *dev)
{
	dev_req_t aoa_start = {
		.req_recp = dev_recp,
		.req_type = vendor_type,
		.data_dir = host_to_device,
		.bRequest = AOA_START,
		.wValue = 0,
		.wIndex = 0,
		.wLength = 0,
	};
	return dev->dev->controller->control(dev->dev, OUT, sizeof(aoa_start),
					     &aoa_start, 0, NULL);
}

static int aoa_try_enter(GenericUsbDevice *dev)
{
	// Ideally we'd just check the return code, but libpayload's USB HC
	// drivers don't share common return codes... so if we want to check for
	// timeout we'll have to do it manually.
	uint64_t time = timer_us(0);
	if (aoa_get_protocol(dev) <= 0) {
		device_descriptor_t *dd = dev->dev->descriptor;
		if (timer_us(time) >= USB_MAX_PROCESSING_TIME_US)
			printf("ERROR: AOA probe timed out on USB #%d (%04x:%04x).\n",
			       dev->dev->address, dd->idVendor, dd->idProduct);
		return -1; // Doesn't support AOA (probably no Android device)
	}

	printf("USB device %d supports Android Open Accessory! "
	       "Trying to connect to Chrome OS Recovery app...\n",
	       dev->dev->address);

	if (aoa_set_string(dev, AOA_STRING_MANUFACTURER, manufacturer) < 0 ||
	    aoa_set_string(dev, AOA_STRING_MODEL, model) < 0 ||
	    aoa_set_string(dev, AOA_STRING_DESCRIPTION, description) < 0 ||
	    aoa_set_string(dev, AOA_STRING_VERSION, version) < 0 ||
	    aoa_set_string(dev, AOA_STRING_URI, uri) < 0) {
		printf("Error sending AOA identification strings\n");
		return -1;
	}

	if (aoa_start(dev)) {
		printf("Error switching device into AOA mode\n");
		return -1;
	}

	return 0;
}

static int aoa_recovery_probe(GenericUsbDevice *dev)
{
	device_descriptor_t *dd = dev->dev->descriptor;

	if (dd->idVendor == USB_VID_GOOGLE &&
	    (dd->idProduct == USB_PID_AOA || dd->idProduct == USB_PID_AOA_ADB))
		return aoa_recovery_setup(dev);

	if (aoa_try_enter(dev) == 0) {
		// Note: This will break if you connect two phones at the same
		// time but only the first serves a valid image. It's very hard
		// to avoid, so... uhh... just don't do that?
		char param[32];
		snprintf(param, sizeof(param), "cros_aoa=%.4x:%.4x",
			 dd->idVendor, dd->idProduct);
		commandline_append(param);
		// If the AOA device disconnects and reconnects again after this
		// we'll have more than one instance of the param. The kernel
		// should only react to the last one, so this should be fine
		// unless we blow the commandline buffer size, which should be
		// too unlikely in practice to really become a problem.
	}

	// If we succeeded entering, device will drop off the bus and
	// reenumerate, so still returning 0 here.
	return 0;
}

static GenericUsbDriver aoa_recovery_driver = {
	.probe = &aoa_recovery_probe,
	.remove = &aoa_recovery_remove,
};

static int aoa_recovery_driver_register(VbootInitFunc *unused)
{
	if (vb2api_phone_recovery_enabled(vboot_get_context()))
		list_insert_after(&aoa_recovery_driver.list_node,
				  &generic_usb_drivers);
	return 0;
}

/*
 * This is intentionally registered in a VBOOT_INIT_FUNC(), not a normal
 * INIT_FUNC(). This means that it will not be available during the first
 * usb_poll() that's triggered by vboot_check_enable_usb().
 * We only want this driver to run on devices that get plugged in after we're up
 * and running in recovery mode, not on anything that was already plugged in
 * during boot, to reduce the chance of sending AOA probe commands to devices
 * that don't expect it and cause unintended behavior.
 */
VBOOT_INIT_FUNC(aoa_recovery_driver_register);
