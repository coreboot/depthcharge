// SPDX-License-Identifier: GPL-2.0
#include "drivers/ec/cros/ec.h"
#include "base/init_funcs.h"
#include "usb.h"

#if CONFIG(DRIVER_USB_AP_MUX_CONTROL_CROS_DEBUG)
#define debug(msg, args...) printf("%s: " msg, __func__, ##args)
#else
#define debug(msg, args...)
#endif
#define error(msg, args...) printf("%s: " msg, __func__, ##args)

static bool mux_control_enabled;

static void usb_typec_cros_ec_mux_init(void *data)
{
	uint32_t flags0;
	uint32_t flags1;

	if (cros_ec_get_features(&flags0, &flags1) < 0) {
		error("Failed to get EC feature flags\n");
		return;
	}

	mux_control_enabled = flags1 &
			       EC_FEATURE_MASK_1(EC_FEATURE_TYPEC_AP_MUX_SET);
	debug("EC AP mux %s\n", mux_control_enabled ? "enabled" : "disabled");
}

static void usb_typec_cros_ec_mux_configure(void *data)
{
	int ret;
	int num_ports;
	struct ec_response_typec_status status;

	if (!mux_control_enabled)
		return;

	ret = cros_ec_get_usb_pd_ports(&num_ports);
	if (ret < 0) {
		error("Cannot get number of ports, ret:%d\n", ret);
		return;
	}

	debug("Number of type-C ports: %d\n", num_ports);

	for (int i = 0; i < num_ports; i++) {
		ret = cros_ec_get_typec_status(i, &status);
		if (ret < 0) {
			error("Cannot query port %d status, ret: %d\n", i, ret);
			continue;
		}

		// Only configure ports with a connected device
		if (!status.dev_connected) {
			debug("Skipping port %d, nothing connected\n", i);
			continue;
		}

		uint8_t mux_state = status.mux_state;
		if (!(mux_state & USB_PD_MUX_DP_ENABLED) &&
		    mux_state & USB_PD_MUX_USB_ENABLED) {
			// Mux is already configured correctly
			debug("Skpping Port %d, already configured to %#x\n",
				i, mux_state);
			continue;
		}

		// Clear DP and set USB flag to enable USB SS
		mux_state &= ~USB_PD_MUX_DP_ENABLED;
		mux_state |= USB_PD_MUX_USB_ENABLED;

		ret = cros_ec_set_typec_mux(i, TYPEC_USB_MUX_SET_ALL_CHIPS,
					    mux_state);
		if (ret < 0) {
			error("Cannot configure port %d, ret: %d\n", i, ret);
			continue;
		}

		debug("Configured port %d mux to %#x\n", i, mux_state);
	}
}

static UsbCallbackData mux_init_callback_data = {
	.callback = &usb_typec_cros_ec_mux_init,
	.data = NULL
};

static UsbCallbackData mux_poll_callback_data = {
	.callback = &usb_typec_cros_ec_mux_configure,
	.data = NULL
};

static int mux_init(void)
{
	usb_register_init_callback(&mux_init_callback_data);
	usb_register_poll_callback(&mux_poll_callback_data);
	return 0;
}

INIT_FUNC(mux_init);
