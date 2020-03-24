// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC.
 *
 * Vboot callbacks for auxiliary firmware (auxfw) sync.
 */

#include <libpayload.h>
#include <vb2_api.h>

#include "drivers/ec/vboot_ec.h"
#include "drivers/ec/vboot_auxfw.h"

/**
 * Send the EC the command to close the auxfw tunnels.
 *
 * @return VB2_ERROR_UNKNOWN on error, VB2_SUCCESS on success.
 */
static vb2_error_t protect_fw_tunnels(void)
{
	VbootEcOps *ec = vboot_get_ec();

	/*
	 * The entire auxfw update is complete at this point. AP firmware
	 * won't need to communicate to peripherals past this point, so protect
	 * the remote bus/tunnel to prevent OS from accessing it later.
	 */
	if (ec->protect_tcpc_ports && ec->protect_tcpc_ports(ec)) {
		printf("Some remote tunnels in EC may be unprotected\n");
		return VB2_ERROR_UNKNOWN;
	}

	return VB2_SUCCESS;
}

/*
 * Vboot callback to check for severity of auxfw update.
 */
vb2_error_t vb2ex_auxfw_check(enum vb2_auxfw_update_severity *severity)
{
	return check_vboot_auxfw(severity);
}

/*
 * Vboot callback to update auxfw.
 */
vb2_error_t vb2ex_auxfw_update(void)
{
	return update_vboot_auxfw();
}

/*
 * Vboot callback to protect auxfw tunnels after sync.
 */
vb2_error_t vb2ex_auxfw_finalize(struct vb2_context *ctx)
{
	return protect_fw_tunnels();
}
