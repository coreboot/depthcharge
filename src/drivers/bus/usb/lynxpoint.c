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

#include <assert.h>
#include <arch/io.h>

#include "base/cleanup_funcs.h"
#include "base/init_funcs.h"

static int lynxpoint_route_to_xhci(struct CleanupFunc *cleanup,
				   CleanupType type)
{
	// Issue SMI to Coreboot to route all USB ports to XHCI.
	printf("Routing USB ports to XHCI controller\n");
	outb(0xca, 0xb2);
	return 0;
}

static int lynxpoint_route_to_xhci_install(void)
{
	static CleanupFunc dev =
	{
		&lynxpoint_route_to_xhci,
		CleanupOnHandoff,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(lynxpoint_route_to_xhci_install);
