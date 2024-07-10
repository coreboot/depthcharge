/*
 * Copyright 2024 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "base/late_init_funcs.h"
#include "drivers/soc/intel_common.h"

#include "cse_internal.h"
#include "cse.h"
#include "me.h"

#define MMIO_BIOS_GPR0	0x98

union spi_bios_gpr0 {
	struct {
		/* Specified write protection is enabled */
		/*
		 * This field corresponds to flash address bits 26:12
		 * and specifies the lower limit of protected range.
		 */
		uint32_t protect_range_base:15;

		/* Specifies read protection is enabled */
		uint32_t read_protect_en:1;

		/*
		 * This field corresponds to flash address bits 26:12
		 * and specifies the upper limit of the protected range
		 */
		uint32_t protect_range_limit:15;

		uint32_t write_protect_en:1;
	} __packed fields;

	uint32_t data;
};

/* Read SPI BAR 0 from PCI configuration space */
static uintptr_t get_spi_bar(pcidev_t dev)
{
	uintptr_t bar;

	bar = pci_read_config32(dev, PCI_BASE_ADDRESS_0);
	assert(bar != 0);
	/*
	 * Bits 31-12 are the base address as per EDS for SPI,
	 * Don't care about 0-11 bit
	 */
	return bar & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK;
}

static uint32_t spi_read_bar(pcidev_t dev, uint32_t offset)
{
	return read32((void *)get_spi_bar(dev) + offset);
}
static uint32_t spi_read_bios_gpr0(void)
{
	return spi_read_bar(PCH_DEV_CSE, MMIO_BIOS_GPR0);
}

static uint32_t spi_get_wp_cse_ro_start_offset(union spi_bios_gpr0 bios_gpr0)
{
	return bios_gpr0.fields.protect_range_base << 12;
}

static uint32_t spi_get_wp_cse_ro_limit(union spi_bios_gpr0 bios_gpr0)
{
	return bios_gpr0.fields.protect_range_limit << 12 | 0xfff;
}

static bool is_spi_wp_cse_ro_en(void)
{
	union spi_bios_gpr0 bios_gpr0;

	bios_gpr0.data = spi_read_bios_gpr0();
	return !!bios_gpr0.fields.write_protect_en;
}

static void spi_get_wp_cse_ro_range(uint32_t *base, uint32_t *limit)
{
	union spi_bios_gpr0 bios_gpr0;

	bios_gpr0.data = spi_read_bios_gpr0();
	*base = spi_get_wp_cse_ro_start_offset(bios_gpr0);
	*limit = spi_get_wp_cse_ro_limit(bios_gpr0);
}

void cse_log_ro_write_protection_info(bool mfg_mode)
{
	bool cse_ro_wp_en = is_spi_wp_cse_ro_en();

	printk(BIOS_DEBUG, "ME: WP for RO is enabled        : %s\n",
			cse_ro_wp_en ? "YES" : "NO");

	if (cse_ro_wp_en) {
		uint32_t base, limit;
		spi_get_wp_cse_ro_range(&base, &limit);
		printk(BIOS_DEBUG, "ME: RO write protection scope - Start=0x%X, End=0x%X\n",
				base, limit);
	}

	/*
	 * If manufacturing mode is disabled, but CSE RO is not write protected,
	 * log error.
	 */
	if (!mfg_mode && !cse_ro_wp_en)
		printk(BIOS_ERR, "ME: Write protection for CSE RO is not enabled\n");
}

/*
 * This function returns state of manufacturing mode.
 *
 * ME manufacturing mode is disabled if the descriptor is locked and fuses
 * are programmed. Additionally, if the SoC supports manufacturing variable, should be locked.
 */
static bool is_manufacturing_mode(union me_hfsts1 hfsts1, union me_hfsts6 hfsts6)
{
#if CONFIG_ME_SPEC <= 13
	return !(hfsts1.fields.mfg_mode == 0);
#elif CONFIG_ME_SPEC <= 15
	return !((hfsts1.fields.mfg_mode == 0) &&
		 (hfsts6.fields.fpf_soc_lock == 1));
#else
	return !((hfsts1.fields.mfg_mode == 0) &&
		 (hfsts6.fields.fpf_soc_lock == 1) &&
		 (hfsts6.fields.manuf_lock == 1));
#endif
}

static void dump_me_status(void *unused)
{
	union me_hfsts1 hfsts1;
	union me_hfsts2 hfsts2;
	union me_hfsts3 hfsts3;
	union me_hfsts4 hfsts4;
	union me_hfsts5 hfsts5;
	union me_hfsts6 hfsts6;
	bool manufacturing_mode;

	if (!is_cse_enabled())
		return;

	hfsts1.data = me_read_config32(PCI_ME_HFSTS1);
	hfsts2.data = me_read_config32(PCI_ME_HFSTS2);
	hfsts3.data = me_read_config32(PCI_ME_HFSTS3);
	hfsts4.data = me_read_config32(PCI_ME_HFSTS4);
	hfsts5.data = me_read_config32(PCI_ME_HFSTS5);
	hfsts6.data = me_read_config32(PCI_ME_HFSTS6);

	printk(BIOS_DEBUG, "ME: HFSTS1                      : 0x%08X\n", hfsts1.data);
	printk(BIOS_DEBUG, "ME: HFSTS2                      : 0x%08X\n", hfsts2.data);
	printk(BIOS_DEBUG, "ME: HFSTS3                      : 0x%08X\n", hfsts3.data);
	printk(BIOS_DEBUG, "ME: HFSTS4                      : 0x%08X\n", hfsts4.data);
	printk(BIOS_DEBUG, "ME: HFSTS5                      : 0x%08X\n", hfsts5.data);
	printk(BIOS_DEBUG, "ME: HFSTS6                      : 0x%08X\n", hfsts6.data);

	manufacturing_mode = is_manufacturing_mode(hfsts1, hfsts6);
	printk(BIOS_DEBUG, "ME: Manufacturing Mode          : %s\n",
		manufacturing_mode ? "YES" : "NO");
	/*
	 * The SPI Protection Mode bit reflects SPI descriptor
	 * locked(0) or unlocked(1).
	 */
	printk(BIOS_DEBUG, "ME: SPI Protection Mode Enabled : %s\n",
		hfsts1.fields.mfg_mode ? "NO" : "YES");
	printk(BIOS_DEBUG, "ME: FW Partition Table          : %s\n",
		hfsts1.fields.fpt_bad ? "BAD" : "OK");
	printk(BIOS_DEBUG, "ME: Bringup Loader Failure      : %s\n",
		hfsts1.fields.ft_bup_ld_flr ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: Firmware Init Complete      : %s\n",
		hfsts1.fields.fw_init_complete ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: Boot Options Present        : %s\n",
		hfsts1.fields.boot_options_present ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: Update In Progress          : %s\n",
		hfsts1.fields.update_in_progress ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: D0i3 Support                : %s\n",
		hfsts1.fields.d0i3_support_valid ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: Low Power State Enabled     : %s\n",
		hfsts2.fields.low_power_state ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: CPU Replaced                : %s\n",
		hfsts2.fields.cpu_replaced  ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: CPU Replacement Valid       : %s\n",
		hfsts2.fields.cpu_replaced_valid ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: Current Working State       : %u\n",
		hfsts1.fields.working_state);
	printk(BIOS_DEBUG, "ME: Current Operation State     : %u\n",
		hfsts1.fields.operation_state);
	printk(BIOS_DEBUG, "ME: Current Operation Mode      : %u\n",
		hfsts1.fields.operation_mode);
	printk(BIOS_DEBUG, "ME: Error Code                  : %u\n",
		hfsts1.fields.error_code);
#if CONFIG_ME_SPEC >= 15
	printk(BIOS_DEBUG, "ME: FPFs Committed              : %s\n",
		hfsts6.fields.fpf_soc_lock ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: Enhanced Debug Mode         : %s\n",
		hfsts1.fields.invoke_enhance_dbg_mode ? "YES" : "NO");
#endif

#if CONFIG_ME_SPEC <= 16
	printk(BIOS_DEBUG, "ME: CPU Debug Disabled          : %s\n",
		hfsts6.fields.cpu_debug_disable ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: TXT Support                 : %s\n",
		hfsts6.fields.txt_support ? "YES" : "NO");
#else
	printk(BIOS_DEBUG, "ME: CPU Debug Disabled          : %s\n",
		hfsts5.fields.cpu_debug_disabled ? "YES" : "NO");
	printk(BIOS_DEBUG, "ME: TXT Support                 : %s\n",
		hfsts5.fields.txt_support ? "YES" : "NO");
#endif

#if CONFIG_ME_SPEC >= 16
	printk(BIOS_DEBUG, "ME: Manufacturing Vars Locked   : %s\n",
		hfsts6.fields.manuf_lock ? "YES" : "NO");
	if (CONFIG(SOC_INTEL_CSE_LITE_SKU))
		cse_log_ro_write_protection_info(manufacturing_mode);
#endif
}

static int intel_cse_spec_log(struct LateInitFunc *init)
{
	print_me_fw_version(NULL);
	dump_me_status(NULL);
	return 0;
}

LATE_INIT_FUNC(intel_cse_spec_log);