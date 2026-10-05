// SPDX-License-Identifier: GPL-2.0-only

#include <arch/io.h>
#include <assert.h>
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include "base/init_funcs.h"
#include "boot/fit.h"
#include "drivers/bus/i2c/qcom_qupv3_i2c.h"
#include "drivers/bus/spi/qcom_qspi.h"
#include "drivers/bus/spi/qcom_qupv3_spi.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/spi.h"
#include "drivers/flash/spi.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/qcom_gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/chromeec.h"
#include "drivers/power/psci.h"
#include "drivers/soc/dougie.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/qcom_ufs.h"
#include "drivers/storage/sdhci_msm.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/video/display.h"
#include "drivers/video/ec_pwm_backlight.h"
#include "variant.h"
#include "vboot/util/flag.h"

struct dougie_display {
	DisplayOps ops;
	GpioOps *backlight_gpio;
};

static const VpdDeviceTreeMap vpd_dt_map[] = {
	{ "bluetooth_mac0", "bluetooth0/local-bd-address" },
	{ "wifi_mac0", "wifi0/local-mac-address" },
	{}
};

static void usb_setup(void)
{
	UsbHostController *usb_host = new_usb_hc(XHCI, QCOM_XHCI_BASE);

	list_insert_after(&usb_host->list_node, &usb_host_controllers);
}

__weak bool variant_ufs_supported(void)
{
	return true;
}

__weak bool variant_emmc_supported(void)
{
	return true;
}

__weak bool variant_nvme_supported(void)
{
	return true;
}

static void storage_setup(void)
{
	if (CONFIG(DRIVER_STORAGE_UFS_QCOM) && variant_ufs_supported()) {
		struct qcom_ufs_ctlr *ufs = new_qcom_ufs_ctlr(QCOM_UFS_HCI_BASE);

		list_insert_after(&ufs->ufs.bctlr.list_node,
				  &fixed_block_dev_controllers);
	}

	if (CONFIG(DRIVER_STORAGE_SDHCI_MSM) && variant_emmc_supported()) {
		u32 emmc_platfm_flags = SDHCI_PLATFORM_EMMC_1V8_POWER |
					SDHCI_PLATFORM_EMMC_HARDWIRED_VCC |
					SDHCI_PLATFORM_NO_EMMC_HS200 |
					SDHCI_PLATFORM_SUPPORTS_HS400ES;
		SdhciHost *emmc = new_sdhci_msm_host(SDC1_HC_BASE,
						     emmc_platfm_flags,
						     384 * MHz, NULL);

		list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
				  &fixed_block_dev_controllers);
	}

	if (CONFIG(DRIVER_STORAGE_NVME) && variant_nvme_supported()) {
		NvmeCtrlr *nvme = new_nvme_ctrlr(variant_get_nvme_pcidev());

		list_insert_after(&nvme->ctrlr.list_node,
				  &fixed_block_dev_controllers);
	}
}

static void flash_setup(void)
{
	QcomQspi *spi_flash = new_qcom_qspi(QSPI_BASE,
					    (GpioOps *)&new_gpio_output(QSPI_CS)->ops);
	SpiFlash *flash = new_spi_flash(&spi_flash->ops);

	flash_set_ops(&flash->ops);
}

static void ec_setup(void)
{
	SpiOps *ec_spi = &new_qup_spi(variant_get_ec_spi_base())->ops;
	CrosEcBusOps *ec_bus = &new_cros_ec_spi_bus(ec_spi)->ops;
	GpioOps *ec_int = sysinfo_lookup_gpio("EC interrupt", 1,
					      new_gpio_input_from_coreboot);
	CrosEc *ec = new_cros_ec(ec_bus, ec_int);

	register_vboot_ec(&ec->vboot);
}

static int tpm_irq_status(void)
{
	static GpioOps *tpm_int;

	if (!tpm_int)
		tpm_int = sysinfo_lookup_gpio("TPM interrupt", 1,
					      new_gpio_latched_from_coreboot);
	return gpio_get(tpm_int);
}

static void tpm_setup(void)
{
	tpm_set_ops(&new_gsc_i2c(&new_qup_i2c(variant_get_gsc_i2c_base())->ops,
				 GSC_I2C_ADDR, tpm_irq_status)->base.ops);
}

static int dougie_backlight_update(DisplayOps *me, bool enable)
{
	struct dougie_display *display = container_of(me, struct dougie_display, ops);

	if (display->backlight_gpio)
		gpio_set(display->backlight_gpio, enable);

	return 0;
}

static int dougie_display_stop(DisplayOps *me)
{
	dougie_backlight_update(me, 0);
	write32(MDP_TIMING_ENGINE_EN, 0x0);
	mdelay(20);
	write32(EDP_SW_RESET, 0x1);
	mdelay(20);
	write32(EDP_SW_RESET, 0x0);
	return 0;
}

static int dougie_display_init(DisplayOps *me)
{
	uintptr_t phys_addr = lib_sysinfo.framebuffer.physical_address;

	write32(MDP_SSPP_SRC0_ADDR, phys_addr);
	write32(MDP_TIMING_ENGINE_EN, 0x01);
	return 0;
}

static void display_setup(void)
{
	struct dougie_display *display;

	if (lib_sysinfo.framebuffer.physical_address == 0)
		return;

	display = xzalloc(sizeof(*display));
	display->ops.init = dougie_display_init;
	display->ops.stop = dougie_display_stop;

	if (CONFIG(DRIVER_VIDEO_EC_PWM_BACKLIGHT)) {
		display->backlight_gpio = new_ec_pwm_backlight();
		display->ops.backlight_update = dougie_backlight_update;
	}

	display_set_ops(&display->ops);
}

static int board_setup(void)
{
	if (CONFIG(DRIVER_EC_CROS))
		flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	else
		flag_replace(FLAG_LIDSW, new_gpio_high());

	flag_replace(FLAG_PWRSW, new_gpio_low());

	if (CONFIG(DRIVER_POWER_CHROMEEC))
		power_set_ops(&chromec_power_ops);
	else
		power_set_ops(&psci_power_ops);

	usb_setup();
	dt_register_vpd_mac_fixup(vpd_dt_map);
	storage_setup();
	flash_setup();

	if (CONFIG(DRIVER_EC_CROS))
		ec_setup();

	if (CONFIG(DRIVER_TPM_GOOGLE_TI50))
		tpm_setup();

	display_setup();

	return 0;
}

INIT_FUNC(board_setup);
