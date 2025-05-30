/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DRIVERS_BUS_I2C_MT8189_H__
#define __DRIVERS_BUS_I2C_MT8189_H__

#include <libpayload.h>
#include <stddef.h>

/* I2C Register */
struct mtk_i2c_regs {
	uint32_t data_port;
	uint32_t reserved0[1];
	uint32_t intr_mask;
	uint32_t intr_stat;
	uint32_t control;
	uint32_t transfer_len;
	uint32_t transac_len;
	uint32_t delay_len;
	uint32_t timing;
	uint32_t start;
	uint32_t ext_conf;
	uint32_t ltiming;
	uint32_t hs;
	uint32_t io_config;
	uint32_t fifo_addr_clr;
	uint32_t reserved1[2];
	uint32_t transfer_aux_len;
	uint32_t clock_div;
	uint32_t time_out;
	uint32_t softreset;
	uint32_t reserved2[16];
	uint32_t slave_addr;
	uint32_t reserved3[19];
	uint32_t debug_stat;
	uint32_t debug_ctrl;
	uint32_t reserved4[2];
	uint32_t fifo_stat;
	uint32_t fifo_thresh;
	uint32_t reserved5[897];
	uint32_t sec_control;
	uint32_t reserved6[31];
	uint32_t channel_lock;
	uint32_t channel_sec;
	uint32_t hw_cg_en;
	uint32_t reserved7[1];
	uint32_t dma_req;
	uint32_t dma_nreq;
};

check_member(mtk_i2c_regs, fifo_stat, 0xF4);

#endif /* __DRIVERS_BUS_I2C_MT8189_H__ */
