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

#include "base/init_funcs.h"
#include "boot/commandline.h"
#include "drivers/bus/usb/usb.h"
#include "vboot/util/commonparams.h"

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
#define AOA_STRING_SERIAL	5

const char manufacturer[] = "Google";
const char model[]	= "Chrome OS Recovery";
const char description[] = "Chrome OS device in Recovery Mode";
const char version[]	= "1.0";
const char uri[]	= "https://google.com/chromeos/recovery_android";

static int aoa_recovery_connect(GenericUsbDevice *dev)
{
	printf("USB device %d connected in AOA mode!\n", dev->dev->address);

	usb_msc_force_init(dev->dev,
			   USB_MSC_QUIRK_NO_LUNS | USB_MSC_QUIRK_NO_RESET);

	// ******** DANGER ******** Ugly hack ahead! ******** DANGER ********
	// We want to keep this device registered, so we *should* return 1. But
	// the problem is that we're reusing the libpayload MSC driver in the
	// context of depthcharge's driver framework. Libpayload's usbdev_t has
	// 'data', 'init' and 'destroy' pointers for use by the individual USB
	// driver... however, for payload-specific drivers, the depthcharge
	// framework takes control of these pointers, installs it's own custom
	// GenericUsbDevice type in usbdev->data and then expects its custom
	// USB drivers to work with that type. Libpayload's MSC stack can't
	// work with GenericUsbDevice, it expects to have full control over the
	// usbdev_t itself.
	//
	// The solution here is to return 0 so depthcharge "thinks" we did not
	// succeed and avoids touching the usbdev_t. Libpayload decides whether
	// a driver was found by looking if usbdev->data is set, which the MSC
	// driver did, so it will be happy. The MSC driver installed its own
	// teardown code in usbdev->destroy, so teardown will still work fine
	// even though it bypasses the depthcharge framework. An ugly but
	// harmless consequence is that depthcharge will still run through all
	// other driver probes in the list (but they should all fail).
	return 0;
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
	char hwid[VB2_GBB_HWID_MAX_SIZE];
	uint32_t hwid_size = sizeof(hwid);

	if (aoa_get_protocol(dev) <= 0)
		return -1; // Doesn't support AOA (probably no Android device)

	if (vb2api_gbb_read_hwid(vboot_get_context(), hwid, &hwid_size))
		hwid[0] = '\0';

	printf("USB device %d supports Android Open Accessory! "
	       "Trying to connect to Chrome OS Recovery app...\n",
	       dev->dev->address);

	if (aoa_set_string(dev, AOA_STRING_MANUFACTURER, manufacturer) < 0 ||
	    aoa_set_string(dev, AOA_STRING_MODEL, model) < 0 ||
	    aoa_set_string(dev, AOA_STRING_DESCRIPTION, description) < 0 ||
	    aoa_set_string(dev, AOA_STRING_VERSION, version) < 0 ||
	    aoa_set_string(dev, AOA_STRING_URI, uri) < 0 ||
	    aoa_set_string(dev, AOA_STRING_SERIAL, hwid) < 0) {
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
		return aoa_recovery_connect(dev);

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
};

static int aoa_recovery_driver_register(void)
{
	list_insert_after(&aoa_recovery_driver.list_node, &generic_usb_drivers);
	return 0;
}

INIT_FUNC(aoa_recovery_driver_register);
