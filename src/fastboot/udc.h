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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __FASTBOOT_UDC_H__
#define __FASTBOOT_UDC_H__

#include <udc/udc.h>

/* Initializes the usb gadget driver. Returns 1 on success and 0 on error */
int usb_gadget_init(void);
/* Send a packet to host using gadget driver. Returns number of bytes sent */
size_t usb_gadget_send(const char *msg, size_t size);
/* Recv a pack from host using gadget driver. Returns number of bytes rcvd */
size_t usb_gadget_recv(void *pkt, size_t size);
/* Clean up the gadget driver. */
void usb_gadget_stop(void);

/***** Functions to be implemented by the board *****/
void fastboot_chipset_init(struct usbdev_ctrl **udc, device_descriptor_t *dd);

#endif /* __FASTBOOT_UDC_H__ */
