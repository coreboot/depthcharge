/*
 * Copyright 2015 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <arch/io.h>
#include "boot/fit.h"
#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/bus/i2c/armada38x_i2c.h"
#include "drivers/tpm/slb9635_i2c.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/bus/spi/armada38x_spi.h"

#define CYCLONE_COMPAT_STR "google,cyclone-proto1"

#define DDR_BASE_CS_LOW_MASK 0xffff0000
#define DDR_SIZE_MASK 0xffff0000

#define USB_WIN_ENABLE_BIT 0
#define USB_WIN_ENABLE_MASK (1 << USB_WIN_ENABLE_BIT)
#define USB_WIN_TARGET_OFFSET 4
#define USB_WIN_TARGET_MASK (1 << USB_WIN_TARGET_OFFSET)
#define USB_WIN_ATTR_OFFSET 8
#define USB_WIN_ATTR_MASK (1 << USB_WIN_ATTR_OFFSET)
#define USB_WIN_SIZE_OFFSET 16
#define USB_WIN_SIZE_MASK (1 << USB_WIN_SIZE_OFFSET)

#define USB_REG_BASE ((void *)0xF1058000)
#define USB_CORE_MODE_REG (USB_REG_BASE + 0x1A8)
#define USB_CORE_MODE_OFFSET 0
#define USB_CORE_MODE_MASK (3 << USB_CORE_MODE_OFFSET)
#define USB_CORE_MODE_HOST (3 << USB_CORE_MODE_OFFSET)
#define USB_CORE_MODE_DEVICE (2 << USB_CORE_MODE_OFFSET)
#define USB_CORE_CMD_REG (USB_REG_BASE + 0x140)
#define USB_CORE_CMD_RUN_BIT 0
#define USB_CORE_CMD_RUN_MASK (1 << USB_CORE_CMD_RUN_BIT)
#define USB_CORE_CMD_RESET_BIT 1
#define USB_CORE_CMD_RESET_MASK (1 << USB_CORE_CMD_RESET_BIT)
#define USB_BRIDGE_INTR_CAUSE_REG (USB_REG_BASE + 0x310)
#define USB_BRIDGE_INTR_MASK_REG (USB_REG_BASE + 0x314)
#define USB_BRIDGE_IPG_REG (USB_REG_BASE + 0x360)

#define MV_BOARD_TCLK_250MHZ 250000000
#define MV_SPI_DEFAULT_SPEED 50000000

#define MV_USB3_HOST_BASE 0xF10F8000
#define MV_SPI_BASE 0xF1010600
#define MV_I2C_BASE 0xF1011000

static void enable_usb(int target)
{
	u32 base_reg, size_reg, reg_val;
	u32 base, size;
	u8 attr;

	/* setup memory windows */
	size_reg = readl((void *)(0xF1020184 + target * 0x8));
	base_reg = readl((void *)(0xF1020180 + target * 0x8));
	base = base_reg & DDR_BASE_CS_LOW_MASK;
	size = (size_reg & DDR_SIZE_MASK) >> 16;
	attr = 0xf & ~(1 << target);

	printf("%s: base_reg 0x%x, size_reg 0x%x\n", __func__, base_reg,
	       size_reg);
	printf("%s: base 0x%x, size 0x%x, attr 0x%x\n", __func__, base, size,
	       attr);

	size_reg =
	    ((target << USB_WIN_TARGET_OFFSET) | (attr << USB_WIN_ATTR_OFFSET) |
	     (size << USB_WIN_SIZE_OFFSET) | USB_WIN_ENABLE_MASK);

	base_reg = base;

	// enable xhci
	writel(size_reg, (void *)(0xF10FC000 + target * 0x8));
	writel(base_reg, (void *)(0xF10FC004 + target * 0x8));
	// enable ehci
	writel(size_reg, (void *)(0xF1058320 + target * 0x8));
	writel(base_reg, (void *)(0xF1058324 + target * 0x8));

	/* Wait 100 usec */
	udelay(100);

	/* Clear Interrupt Cause and Mask registers */
	writel(0, USB_BRIDGE_INTR_CAUSE_REG);
	writel(0, USB_BRIDGE_INTR_MASK_REG);

	/* Reset controller */
	reg_val = readl(USB_CORE_CMD_REG);
	writel(reg_val | USB_CORE_CMD_RESET_MASK, USB_CORE_CMD_REG);

	while (readl(USB_CORE_CMD_REG) & USB_CORE_CMD_RESET_MASK)
		;

	/* Change value of new register 0x360 */
	reg_val = readl(USB_BRIDGE_IPG_REG);

	/*  Change bits[14:8] - IPG for non Start of Frame Packets
	 *  from 0x9(default) to 0xD
	 */
	reg_val &= ~(0x7F << 8);
	reg_val |= (0xD << 8);

	writel(reg_val, USB_BRIDGE_IPG_REG);

	/* Set Mode register (Stop and Reset USB Core before) */
	/* Stop the controller */
	reg_val = readl(USB_CORE_CMD_REG);
	reg_val &= ~USB_CORE_CMD_RUN_MASK;
	writel(reg_val, USB_CORE_CMD_REG);

	/* Reset the controller to get default values */
	reg_val = readl(USB_CORE_CMD_REG);
	reg_val |= USB_CORE_CMD_RESET_MASK;
	writel(reg_val, USB_CORE_CMD_REG);

	/* Wait for the controller reset to complete */
	do {
		reg_val = readl(USB_CORE_CMD_REG);
	} while (reg_val & USB_CORE_CMD_RESET_MASK);

	/* Set USB_MODE register */
	reg_val = USB_CORE_MODE_HOST;

	writel(reg_val, USB_CORE_MODE_REG);
}
static int board_setup(void)
{
	UsbHostController *usb_host30;
	SpiController *spi;
	Armada38xI2c *i2c;

	sysinfo_install_flags(NULL);

	fit_set_compat(CYCLONE_COMPAT_STR);

	enable_usb(0);

	usb_host30 = new_usb_hc(XHCI, MV_USB3_HOST_BASE);
	list_insert_after(&usb_host30->list_node, &usb_host_controllers);

	spi = new_armada38x_spi(MV_SPI_BASE, MV_BOARD_TCLK_250MHZ,
				1, 0, MV_SPI_DEFAULT_SPEED);
	flash_set_ops(&new_spi_flash(&spi->ops)->ops);

	i2c = new_armada38x_i2c(MV_I2C_BASE, MV_BOARD_TCLK_250MHZ, 1);
	tpm_set_ops(&new_slb9635_i2c(&i2c->ops, 0x20)->base.ops);
	return 0;
}

int get_mach_id(void)
{
	return 0x6810;
}

INIT_FUNC(board_setup);
