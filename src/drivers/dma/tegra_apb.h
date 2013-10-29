/*
 * (C) Copyright 2010,2011
 * NVIDIA Corporation <www.nvidia.com>
 *  Copyright (C) 2013 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DRIVERS_DMA_TEGRA_APB_H__
#define __DRIVERS_DMA_TEGRA_APB_H__

#include <stdint.h>

typedef struct {
	uint32_t csr;			// 0x00
	uint32_t sta;			// 0x04
	uint32_t dma_byte_sta;		// 0x08
	uint32_t csre;			// 0x0c
	uint32_t ahb_ptr;		// 0x10
	uint32_t ahb_seq;		// 0x14
	uint32_t apb_ptr;		// 0x18
	uint32_t apb_seq;		// 0x1c
	uint32_t wcount;		// 0x20
	uint32_t word_transfer;		// 0x24
} __attribute__((packed)) TegraApbDmaRegs;

enum {
	APBDMA_SLAVE_CNTR_REQ = 0,
	APBDMA_SLAVE_APBIF_CH0 = 1,
	APBDMA_SLAVE_APBIF_CH1 = 2,
	APBDMA_SLAVE_APBIF_CH2 = 3,
	APBDMA_SLAVE_APBIF_CH3 = 4,
	APBDMA_SLAVE_HSI = 5,
	APBDMA_SLAVE_APBIF_CH4 = 6,
	APBDMA_SLAVE_APBIF_CH5 = 7,
	APBDMA_SLAVE_UART_A = 8,
	APBDMA_SLAVE_UART_B = 9,
	APBDMA_SLAVE_UART_C = 10,
	APBDMA_SLAVE_DTV = 11,
	APBDMA_SLAVE_APBIF_CH6 = 12,
	APBDMA_SLAVE_APBIF_CH7 = 13,
	APBDMA_SLAVE_APBIF_CH8 = 14,
	APBDMA_SLAVE_SL2B1 = 15,
	APBDMA_SLAVE_SL2B2 = 16,
	APBDMA_SLAVE_SL2B3 = 17,
	APBDMA_SLAVE_SL2B4 = 18,
	APBDMA_SLAVE_UART_D = 19,
	APBDMA_SLAVE_UART_E = 20,
	APBDMA_SLAVE_I2C = 21,
	APBDMA_SLAVE_I2C2 = 22,
	APBDMA_SLAVE_I2C3 = 23,
	APBDMA_SLAVE_DVC_I2C = 24,
	APBDMA_SLAVE_OWR = 25,
	APBDMA_SLAVE_I2C4 = 26,
	APBDMA_SLAVE_SL2B5 = 27,
	APBDMA_SLAVE_SL2B6 = 28,
	APBDMA_SLAVE_APBIF_CH9 = 29,
	APBDMA_SLAVE_I2C6 = 30,
	APBDMA_SLAVE_NA31 = 31
};

enum {
	APBDMACHAN_CSR_ENB = 0x1 << 31,
	APBDMACHAN_CSR_IE_EOC = 0x1 << 30,
	APBDMACHAN_CSR_HOLD = 0x1 << 29,
	APBDMACHAN_CSR_DIR = 1 << 28,
	APBDMACHAN_CSR_ONCE = 1 << 27,
	APBDMACHAN_CSR_FLOW = 1 << 21,
	APBDMACHAN_CSR_REQ_SEL_SHIFT = 16,
	APBDMACHAN_CSR_REQ_SEL_MASK = 0x1f << APBDMACHAN_CSR_REQ_SEL_SHIFT,
	APBDMACHAN_CSR_REQ_SEL_CNTR_REQ = 0,

	APBDMACHAN_STA_BSY = 1 << 31,
	APBDMACHAN_STA_ISE_EOC = 1 << 30,
	APBDMACHAN_STA_HALT = 1 << 29,
	APBDMACHAN_STA_PING_PONG_STA = 1 << 28,
	APBDMACHAN_STA_DMA_ACTIVITY = 1 << 27,
	APBDMACHAN_STA_CHANNEL_PAUSE = 1 << 26,

	APBDMACHAN_CSRE_CHANNEL_PAUSE = 1 << 31,
	APBDMACHAN_CSRE_TRIG_SEL_MASK = 0x3f,
	APBDMACHAN_CSRE_TRIG_SEL_SHIFT = 14,

	APBDMACHAN_AHB_PTR_SHIFT = 2,
	APBDMACHAN_AHB_PTR_MASK = 0x3fffffff << APBDMACHAN_AHB_PTR_SHIFT,

	APBDMACHAN_AHB_SEQ_INTR_ENB = 1 << 31,
	APBDMACHAN_AHB_SEQ_AHB_BUS_WIDTH_SHIFT = 28,
	APBDMACHAN_AHB_SEQ_AHB_BUS_WIDTH_MASK =
		0x7 << APBDMACHAN_AHB_SEQ_AHB_BUS_WIDTH_SHIFT,
	APBDMACHAN_AHB_SEQ_AHB_DATA_SWAP = 1 << 27,
	APBDMACHAN_AHB_SEQ_AHB_BURST_SHIFT = 24,
	APBDMACHAN_AHB_SEQ_AHB_BURST_MASK =
		0x7 << APBDMACHAN_AHB_SEQ_AHB_BURST_SHIFT,
	APBDMACHAN_AHB_SEQ_DBL_BUF = 1 << 19,
	APBDMACHAN_AHB_SEQ_WRAP_SHIFT = 16,
	APBDMACHAN_AHB_SEQ_WRAP_MASK = 0x7 << APBDMACHAN_AHB_SEQ_WRAP_SHIFT,

	APBDMACHAN_APB_PTR_SHIFT = 2,
	APBDMACHAN_APB_PTR_MASK = 0x3fffffff << APBDMACHAN_APB_PTR_SHIFT,

	APBDMACHAN_APB_SEQ_APB_BUS_WIDTH_SHIFT = 28,
	APBDMACHAN_APB_SEQ_APB_BUS_WIDTH_MASK =
		0x7 << APBDMACHAN_APB_SEQ_APB_BUS_WIDTH_SHIFT,
	APBDMACHAN_APB_SEQ_APB_DATA_SWAP = 1 << 27,
	APBDMACHAN_APB_SEQ_APB_ADDR_WRAP_SHIFT = 16,
	APBDMACHAN_APB_SEQ_APB_ADDR_WRAP_MASK =
		0x7 << APBDMACHAN_APB_SEQ_APB_ADDR_WRAP_SHIFT,

	APBDMACHAN_WORD_TRANSFER_SHIFT = 2,
	APBDMACHAN_WORD_TRANSFER_MASK =
		0x0fffffff << APBDMACHAN_WORD_TRANSFER_SHIFT
};

typedef struct {
	uint32_t command;		/* 0x00 */
	uint32_t status;		/* 0x04 */
	uint8_t _rsv0[8];
	uint32_t cntrl_reg;		/* 0x10 */
	uint32_t irq_sta_cpu;		/* 0x14 */
	uint32_t irq_sta_cop;		/* 0x18 */
	uint32_t irq_mask;		/* 0x1c */
	uint32_t irq_mask_set;		/* 0x20 */
	uint32_t irq_mask_clr;		/* 0x24 */
	uint32_t trig_reg;		/* 0x28 */
	uint32_t channel_trig_reg;	/* 0x2c */
	uint32_t dma_status;		/* 0x30 */
	uint32_t channel_en_reg;	/* 0x34 */
	uint32_t security_reg;		/* 0x38 */
	uint32_t channel_swid;		/* 0x3c */
	uint8_t _rsv1[4];
	uint32_t chan_wt_reg0;		/* 0x44 */
	uint32_t chan_wt_reg1;		/* 0x48 */
	uint32_t chan_wt_reg2;		/* 0x4c */
	uint32_t chan_wr_reg3;		/* 0x50 */
	uint32_t channel_swid1;		/* 0x54 */
} __attribute__((packed)) TegraApbDmaMainRegs;

/*
 * Note: Many APB DMA controller registers are laid out such that each
 * bit controls or represents the status for the corresponding channel.
 * So we will not bother to list each individual bit in this case.
 */
enum {
	APBDMA_COMMAND_GEN = 1 << 31,

	APBDMA_CNTRL_REG_COUNT_VALUE_MASK = 0xffff,
	APBDMA_CNTRL_REG_COUNT_VALUE_SHIFT = 0
};

typedef struct TegraApbDmaChannel {
	int (*start)(struct TegraApbDmaChannel *me);
	int (*finish)(struct TegraApbDmaChannel *me);
	int (*busy)(struct TegraApbDmaChannel *me);

	TegraApbDmaRegs *regs;

	int claimed;
} TegraApbDmaChannel;

typedef struct TegraApbDmaController {
	TegraApbDmaChannel *(*claim)(struct TegraApbDmaController *me);
	int (*release)(struct TegraApbDmaController *me,
		       TegraApbDmaChannel *channel);

	TegraApbDmaMainRegs *regs;

	TegraApbDmaChannel *channels;
	int count;
} TegraApbDmaController;

TegraApbDmaController *new_tegra_apb_dma(void *main, void *channels[],
					 int count);

#endif	/* __DRIVERS_DMA_TEGRA_APB_H__ */
