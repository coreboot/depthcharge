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

#ifndef __NETBOOT_NETBOOT_H__
#define __NETBOOT_NETBOOT_H__

#include "net/uip.h"

/* argsfile takes precedence before args. All parameters can be NULL. */
void netboot(uip_ipaddr_t *tftp_ip, char *bootfile, char *argsfile, char *args);
int netboot_entry(void);

#endif /* __NETBOOT_NETBOOT_H__ */
