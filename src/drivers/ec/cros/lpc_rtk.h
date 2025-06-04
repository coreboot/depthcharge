/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2025 Google LLC.  */

#ifndef __DRIVERS_EC_CROS_LPC_RTK_H__
#define __DRIVERS_EC_CROS_LPC_RTK_H__

/* For RTK, using mailbox interface instead of directly access io ranges 0x800 - 0x9ff . */
#define RTK_EMI_RANGE_START EC_HOST_CMD_REGION0
#define RTK_EMI_RANGE_END   (EC_LPC_ADDR_MEMMAP + EC_MEMMAP_SIZE)

#define RTK_SHARED_MEM_BASE ((uintptr_t)0xfe0b0000)

#endif /* __DRIVERS_EC_CROS_LPC_RTK_H__ */
