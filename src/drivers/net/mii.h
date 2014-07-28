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

#ifndef __DRIVERS_NET_MII_H__
#define __DRIVERS_NET_MII_H__

enum {
	MiiBmcr = 0x0,
	MiiBmsr = 0x1,
	MiiPhyIdr1 = 0x2,
	MiiPhyIdr2 = 0x3,
	MiiAnar = 0x4,
	MiiAnlpar = 0x5,
	MiiAner = 0x6
};

enum {
	BmcrCollisionTest = 0x1 << 7,
	BmcrDuplexMode = 0x1 << 8,
	BmcrRestartAutoNeg = 0x1 << 9,
	BmcrIsolate = 0x1 << 10,
	BmcrPowerDown = 0x1 << 11,
	BmcrAutoNegEnable = 0x1 << 12,
	BmcrSpeedSel = 0x1 << 13,
	BmcrLoopback = 0x1 << 14,
	BmcrReset = 0x1 << 15
};

enum {
	BmsrErcap = 0x1 << 0,
	BmsrJcd = 0x1 << 1,
	BmsrLstatus = 0x1 << 2,
	BmsrAnegCapable = 0x1 << 3,
	BmsrRfault = 0x1 << 4,
	BmsrAnegComplete = 0x1 << 5,
	BmsrEstaten = 0x1 << 8,
	Bmsr100Half2 = 0x1 << 9,
	Bmsr100Full2 = 0x1 << 10,
	Bmsr10Half = 0x1 << 11,
	Bmsr10Full = 0x1 << 12,
	Bmsr100Half = 0x1 << 13,
	Bmsr100Full = 0x1 << 14,
	Bmsr100Base4 = 0x1 << 15
};


enum {
	AdvertiseSelect = 0x1f,
	AdvertiseCsma = 0x1,
	Advertise10Half = 0x1 << 5,
	Advertise10Full = 0x1 << 6,
	Advertise100Half = 0x1 << 7,
	Advertise100Full = 0x1 << 8,
	AdvertiseAll = (Advertise10Half | Advertise10Full |
			Advertise100Half | Advertise100Full),
	AdvertisePauseCap = 0x1 << 10,
	AdvertisePauseAsym = 0x1 << 11
};


#endif /* __DRIVERS_NET_MII_H__ */
