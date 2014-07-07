/*
 * Copyright (C) 2014 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __DRIVERS_STORAGE_IPQ806X_CLOCKS_H__
#define __DRIVERS_STORAGE_IPQ806X_CLOCKS_H__

/* MMC clocks */
#define MSM_CLK_CTL_BASE                    ((unsigned char *)0x00900000)

#define REG(off)                            (MSM_CLK_CTL_BASE + (off))

#define SDCn_APPS_CLK_MD_REG(n)             REG(0x2828+(0x20*((n)-1)))
#define SDCn_APPS_CLK_NS_REG(n)             REG(0x282C+(0x20*((n)-1)))
#define SDCn_HCLK_CTL_REG(n)                REG(0x2820+(0x20*((n)-1)))
#define SDCn_RESET_REG(n)                   REG(0x2830+(0x20*((n)-1)))

#define CLK_HALT_DFAB_STATE_REG             REG(0x2FC8)

#endif /* __DRIVERS_STORAGE_IPQ806X_CLOCKS_H__ */
