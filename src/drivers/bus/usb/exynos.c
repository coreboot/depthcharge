/*
 * Copyright 2013 Google Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "drivers/bus/usb/usb.h"

/*
 * This is an ugly hack to work around a hardware bug in the Exynos5420. They
 * changed the silicon process from 32nm to 28nm after the 5250 and forgot to
 * adjust the Loss of Signal bias and Vboost levels of the SuperSpeed PHY
 * accordingly. We can override those values at runtime, but we have to redo it
 * every time the xHC is reset (which currently happens once in
 * dc_usb_initialize()), and we have to use this clunky CR-port register
 * interface to do it. See http://crosbug.com/p/19989
 */

static void crport_handshake(void *phy, u32 data, u32 cmd)
{
	u32 *phyreg0 = (u32 *)(phy + 0x14);
	u32 *phyreg1 = (u32 *)(phy + 0x18);
	u32 usec = 100;

	writel(data | cmd, phyreg0);

	do {	/* wait until hardware sets acknowledge bit */
		if (readl(phyreg1) & (0x1 << 0))
			break;
		udelay(1);
	} while (usec-- > 0);

	if (!usec)
		printf("CRPORT handshake timeout1 (0x%08x)\n", data | cmd);

	usec = 100;

	writel(data, phyreg0);

	do {	/* wait until hardware clears acknowledge bit */
		if (!(readl(phyreg1) & (0x1 << 0)))
			break;
		udelay(1);
	} while (usec-- > 0);

	if (!usec)
		printf("CRPORT handshake timeout2 (0x%08x)\n", data | cmd);
}

static void crport_ctrl_write(void *phy, u32 addr, u32 data)
{
	/* Fill data field with address and pulse Capture Address bit */
	crport_handshake(phy, addr << 2, 0x1 << 0);

	/* Fill data field with data and pulse Capture Data bit */
	crport_handshake(phy, data << 2, 0x1 << 1);

	/* Pulse Write Data bit to commit the write access */
	crport_handshake(phy, 0, 0x1 << 19);
}

void exynos5420_usbss_phy_tune(UsbHostController *hc)
{
	void *phy = hc->bar + 0x00100000;  /* get PHY regs from XHCI base */

	crport_ctrl_write(phy, 0x15,	/* write to PHY SS LoS override IN */
			  0x5 << 13 |	/* Set LoS bias to 5 (default 0) */
			  0x1 << 10 |	/* Set override enable bit */
			  0x9 << 0);	/* Set LoS level to 9 (unchanged) */

	crport_ctrl_write(phy, 0x12,	/* write to PHY SS Tx Vboost override */
			  0x5 << 13);	/* Set Tx Vboost to 5 (default 4) */
}
