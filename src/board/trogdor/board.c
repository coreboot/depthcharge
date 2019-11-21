/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2019, The Linux Foundation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/gpio/gpio.h"
#include "vboot/util/flag.h"
#include "boot/fit.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/spi/qcom_qupv3_spi.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/storage/sdhci_msm.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/gpio/sc7180.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/tpm/spi.h"

#define SDC1_HC_BASE          0x7C4000
#define SDC1_TLMM_CFG_ADDR    0x3D7A000
#define SDC2_TLMM_CFG_ADDR 0x3D7B000
#define SDC2_HC_BASE 0x08804000

static int board_setup(void)
{
	/* stub out required GPIOs for vboot */
	flag_replace(FLAG_LIDSW, new_gpio_high());
	flag_replace(FLAG_WPSW,  new_gpio_high());
	flag_replace(FLAG_PWRSW, new_gpio_low());

	/* Support USB3.0 XHCI controller in firmware. */
	UsbHostController *usb_host = new_usb_hc(XHCI, 0xa600000);
	list_insert_after(&usb_host->list_node, &usb_host_controllers);

	/*eMMC card support */
	SdhciHost *emmc = new_sdhci_msm_host(SDC1_HC_BASE,
			SDHCI_PLATFORM_EMMC_1V8_POWER |
			SDHCI_PLATFORM_NO_EMMC_HS200,
			100*MHz, SDC1_TLMM_CFG_ADDR,
			NULL);

	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD card support */
	Sc7180GpioCfg *cd_gpio_cfg = new_sc7180_gpio_input(GPIO(69));
        SdhciHost *sd = new_sdhci_msm_host(SDC2_HC_BASE,
                                SDHCI_PLATFORM_REMOVABLE,
                                50*MHz, SDC2_TLMM_CFG_ADDR,
                                new_gpio_not(&cd_gpio_cfg->ops));
        list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
                        &removable_block_dev_controllers);

	SpiOps *spi_qup0se0 = &new_sc7180_Qup_spi(0x880000)->ops;
	SpiOps *spi_qup1se0 = &new_sc7180_Qup_spi(0xa80000)->ops;
	SpiOps *ec_spi = lib_sysinfo.board_id < 1 ? spi_qup0se0 : spi_qup1se0;
	SpiOps *tpm_spi = lib_sysinfo.board_id < 1 ? spi_qup1se0 : spi_qup0se0;

	if (CONFIG(DRIVER_TPM_SPI))
		tpm_set_ops(&new_tpm_spi(tpm_spi, NULL)->ops);

	if (CONFIG(DRIVER_EC_CROS)) {
		CrosEcBusOps *ec_bus = &new_cros_ec_spi_bus(ec_spi)->ops;
		GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					new_sc7180_gpio_input_from_coreboot);
		CrosEc *ec = new_cros_ec(ec_bus, 0, ec_int);
		register_vboot_ec(&ec->vboot, 0);

		CrosECTunnelI2c *tcpc0_tunnel = new_cros_ec_tunnel_i2c(ec, 1);
		CrosECTunnelI2c *tcpc1_tunnel = new_cros_ec_tunnel_i2c(ec, 2);
		register_vboot_aux_fw(&new_ps8805(tcpc0_tunnel, 0)->fw_ops);
		register_vboot_aux_fw(&new_ps8805(tcpc1_tunnel, 1)->fw_ops);
	}

	return 0;
}

INIT_FUNC(board_setup);
