/*
 * This file is part of the depthcharge project.
 *
 * Copyright (C) 2014 - 2015 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __GSBI_TYPES_H__
#define __GSBI_TYPES_H__

#define MSM_CLK_CTL_BASE	((void *)0x00900000)

#define GSBI1_BASE		((void *)0x12440000)
#define GSBI2_BASE		((void *)0x12480000)
#define GSBI3_BASE		((void *)0x16200000)
#define GSBI4_BASE		((void *)0x16300000)

#define GSBI1_CTL_REG		(GSBI1_BASE + (0x0))
#define GSBI2_CTL_REG		(GSBI2_BASE + (0x0))
#define GSBI3_CTL_REG		(GSBI3_BASE + (0x0))
#define GSBI4_CTL_REG		(GSBI4_BASE + (0x0))
#define GSBI5_CTL_REG		(GSBI5_BASE + (0x0))
#define GSBI6_CTL_REG		(GSBI6_BASE + (0x0))
#define GSBI7_CTL_REG		(GSBI7_BASE + (0x0))

#define GSBI_QUP1_BASE		(GSBI1_BASE + 0x20000)
#define GSBI_QUP2_BASE		(GSBI2_BASE + 0x20000)
#define GSBI_QUP3_BASE		(GSBI3_BASE + 0x80000)
#define GSBI_QUP4_BASE		(GSBI4_BASE + 0x80000)
#define GSBI_QUP5_BASE		(GSBI5_BASE + 0x80000)
#define GSBI_QUP6_BASE		(GSBI6_BASE + 0x80000)
#define GSBI_QUP7_BASE		(GSBI7_BASE + 0x80000)

#define GSBI_CTL_PROTO_I2C              2
#define GSBI_CTL_PROTO_CODE_SFT         4
#define GSBI_CTL_PROTO_CODE_MSK         0x7
#define GSBI_HCLK_CTL_GATE_ENA          6
#define GSBI_HCLK_CTL_BRANCH_ENA        4
#define GSBI_QUP_APPS_M_SHFT            16
#define GSBI_QUP_APPS_M_MASK            0xFF
#define GSBI_QUP_APPS_D_SHFT            0
#define GSBI_QUP_APPS_D_MASK            0xFF
#define GSBI_QUP_APPS_N_SHFT            16
#define GSBI_QUP_APPS_N_MASK            0xFF
#define GSBI_QUP_APPS_ROOT_ENA_SFT      11
#define GSBI_QUP_APPS_BRANCH_ENA_SFT    9
#define GSBI_QUP_APPS_MNCTR_EN_SFT      8
#define GSBI_QUP_APPS_MNCTR_MODE_MSK    0x3
#define GSBI_QUP_APPS_MNCTR_MODE_SFT    5
#define GSBI_QUP_APPS_PRE_DIV_MSK       0x3
#define GSBI_QUP_APPS_PRE_DIV_SFT       3
#define GSBI_QUP_APPS_SRC_SEL_MSK       0x7


#define GSBI_QUP_APSS_MD_REG(gsbi_n)	((MSM_CLK_CTL_BASE + 0x29c8) + \
							(32*(gsbi_n-1)))
#define GSBI_QUP_APSS_NS_REG(gsbi_n)	((MSM_CLK_CTL_BASE + 0x29cc) + \
							(32*(gsbi_n-1)))
#define GSBI_HCLK_CTL(n)		((MSM_CLK_CTL_BASE + 0x29C0) + \
							(32*(n-1)))

typedef enum {
	GSBI_ID_1 = 1,
	GSBI_ID_2,
	GSBI_ID_3,
	GSBI_ID_4,
	GSBI_ID_5,
	GSBI_ID_6,
	GSBI_ID_7,
} gsbi_id_t;

typedef enum {
	GSBI_SUCCESS = 0,
	GSBI_ID_ERROR,
	GSBI_ERROR,
	GSBI_UNSUPPORTED
} gsbi_return_t;

typedef enum {
	GSBI_PROTO_I2C_UIM = 1,
	GSBI_PROTO_I2C_ONLY,
	GSBI_PROTO_SPI_ONLY,
	GSBI_PROTO_UART_FLOW_CTL,
	GSBI_PROTO_UIM,
	GSBI_PROTO_I2C_UART,
} gsbi_protocol_t;

gsbi_return_t gsbi_init(gsbi_id_t gsbi_id, gsbi_protocol_t protocol);
#endif
