/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOOT_ANDROID_MTE_H_
#define _BOOT_ANDROID_MTE_H_

/*
 * Initialize MTE.
 *
 * Reserve memory in device tree using the hardcoded MTE base address
 * and size from Kconfig.
 */
void mte_initialize(void);

#endif /* _BOOT_ANDROID_MTE_H_ */
