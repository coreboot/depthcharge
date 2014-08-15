/*
 * Copyright 2014 Google Inc.
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

#include <stdint.h>
#include <stdio.h>

#include "drivers/net/mii.h"
#include "drivers/net/net.h"

static int mii_restart_autoneg(NetDevice *dev)
{
	uint16_t bmsr;
	if (dev->mdio_read(dev, MiiBmsr, &bmsr))
		return 1;

	if (!(bmsr & BmsrAnegCapable)) {
		printf("No AutoNeg, falling back to 100baseTX\n");
		return dev->mdio_write(dev, MiiBmcr,
			BmcrSpeedSel | BmcrDuplexMode);
	} else {
		return dev->mdio_write(dev, MiiBmcr,
			BmcrAutoNegEnable | BmcrRestartAutoNeg);
	}
}

int mii_phy_initialize(NetDevice *dev)
{
	if (dev->mdio_write(dev, MiiBmcr, BmcrReset))
		return 1;
	if (dev->mdio_write(dev, MiiAnar, AdvertiseAll | AdvertiseCsma))
		return 1;

	if (mii_restart_autoneg(dev))
		return 1;
	return 0;
}

int mii_ready(NetDevice *dev, int *ready)
{
	uint16_t link_status = 0;
	if (dev->mdio_read(dev, MiiBmsr, &link_status)) {
		printf("Failed to read BMSR.\n");
		return 1;
	}
	*ready = (link_status & BmsrLstatus);
	return 0;
}
