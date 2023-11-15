/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DRIVERS_BUS_I2C_MT8173_H__
#define __DRIVERS_BUS_I2C_MT8173_H__

#include <libpayload.h>
#include <stddef.h>

/* I2C Register */
struct mtk_i2c_regs {
	uint32_t data_port;
	uint32_t slave_addr;
	uint32_t intr_mask;
	uint32_t intr_stat;
	uint32_t control;
	uint32_t transfer_len;
	uint32_t transac_len;
	uint32_t delay_len;
	uint32_t timing;
	uint32_t start;
	uint32_t ext_conf;
	uint32_t reserved1;
	uint32_t fifo_stat;
	uint32_t fifo_thresh;
	uint32_t fifo_addr_clr;
	uint32_t reserved2;
	uint32_t io_config;
	uint32_t debug;
	uint32_t hs;
	uint32_t reserved3;
	uint32_t softreset;
	uint32_t dcm;
	uint32_t reserved4[3];
	uint32_t debug_stat;
	uint32_t debug_ctrl;
	uint32_t transfer_aux_len;
};

check_member(mtk_i2c_regs, debug_stat, 0x64);

#endif /* __DRIVERS_BUS_I2C_MT8173_H__ */
