/*
 * Copyright (C) 2012 Samsung Electronics
 * R. Chandrasekar <rcsekar@samsung.com>
 *
 * Taken from the kernel code
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_BUS_I2S_EXYNOS5_REGS_H__
#define __DRIVERS_BUS_I2S_EXYNOS5_REGS_H__

#define CON_MULTI_RSTCLR	(1 << 31)

#define CON_MULTI_TXFIFO_EMPTY	(1 << 10)
#define CON_MULTI_TXFIFO_FULL	(1 << 8)
#define CON_MULTI_TXCH_PAUSE	(1 << 4)
#define CON_MULTI_ACTIVE	(1 << 0)

#define MOD_MULTI_BLCS_SHIFT	26
#define MOD_MULTI_BLCS_16BIT	(0 << MOD_MULTI_BLCS_SHIFT)
#define MOD_MULTI_BLCS_8BIT	(1 << MOD_MULTI_BLCS_SHIFT)
#define MOD_MULTI_BLCS_24BIT	(2 << MOD_MULTI_BLCS_SHIFT)
#define MOD_MULTI_BLCS_MASK	(3 << MOD_MULTI_BLCS_SHIFT)

#define MOD_MULTI_BLCP_SHIFT	24
#define MOD_MULTI_BLCP_16BIT	(0 << MOD_MULTI_BLCP_SHIFT)
#define MOD_MULTI_BLCP_8BIT	(1 << MOD_MULTI_BLCP_SHIFT)
#define MOD_MULTI_BLCP_24BIT	(2 << MOD_MULTI_BLCP_SHIFT)
#define MOD_MULTI_BLCP_MASK	(3 << MOD_MULTI_BLCP_SHIFT)

#define MOD_MULTI_BLC_16BIT	(0 << 13)
#define MOD_MULTI_BLC_8BIT	(1 << 13)
#define MOD_MULTI_BLC_24BIT	(2 << 13)
#define MOD_MULTI_BLC_MASK	(3 << 13)

#define MOD_MULTI_OP_CLK_COUT	(0 << 30)
#define MOD_MULTI_OP_CLK_CIN	(1 << 30)
#define MOD_MULTI_OP_CLK_BIT	(2 << 30)
#define MOD_MULTI_OP_CLK_AUDIO	(3 << 30)
#define MOD_MULTI_OP_CLK_MASK	(3 << 30)
#define MOD_MULTI_SLAVE		(1 << 11)
#define MOD_MULTI_RCLKSRC	(1 << 10)
#define MOD_MULTI_TXONLY	(0 << 8)
#define MOD_MULTI_RXONLY	(1 << 8)
#define MOD_MULTI_TXRX		(2 << 8)
#define MOD_MULTI_MASK		(3 << 8)
#define MOD_MULTI_LR_LLOW	(0 << 15)
#define MOD_MULTI_LR_RLOW	(1 << 15)
#define MOD_MULTI_SDF_IIS	(0 << 6)
#define MOD_MULTI_SDF_MSB	(1 << 6)
#define MOD_MULTI_SDF_LSB	(2 << 6)
#define MOD_MULTI_SDF_MASK	(3 << 6)
#define MOD_MULTI_RCLK_256FS	(0 << 4)
#define MOD_MULTI_RCLK_512FS	(1 << 4)
#define MOD_MULTI_RCLK_384FS	(2 << 4)
#define MOD_MULTI_RCLK_768FS	(3 << 4)
#define MOD_MULTI_RCLK_MASK	(3 << 4)
#define MOD_MULTI_BCLK_32FS	(0 << 0)
#define MOD_MULTI_BCLK_48FS	(1 << 0)
#define MOD_MULTI_BCLK_16FS	(2 << 0)
#define MOD_MULTI_BCLK_24FS	(3 << 0)
#define MOD_MULTI_BCLK_MASK	(15 << 0)

#define PSR_MULTI_PSREN		(1 << 15)



#define FIC_MULTI_TXFLUSH	(1 << 15)

#define CON_TXFIFO_EMPTY	(1 << 10)
#define CON_TXFIFO_FULL		(1 << 8)
#define CON_TXCH_PAUSE		(1 << 4)
#define CON_ACTIVE		(1 << 0)

#define MOD_BLC_16BIT		(0 << 13)
#define MOD_BLC_8BIT		(1 << 13)
#define MOD_BLC_24BIT		(2 << 13)
#define MOD_BLC_MASK		(3 << 13)

#define MOD_SLAVE		(1 << 11)
#define MOD_TXONLY		(0 << 8)
#define MOD_RXONLY		(1 << 8)
#define MOD_TXRX		(2 << 8)
#define MOD_MASK		(3 << 8)
#define MOD_LR_LLOW		(0 << 7)
#define MOD_LR_RLOW		(1 << 7)
#define MOD_SDF_IIS		(0 << 5)
#define MOD_SDF_MSB		(1 << 5)
#define MOD_SDF_LSB		(2 << 5)
#define MOD_SDF_MASK		(3 << 5)
#define MOD_RCLK_256FS		(0 << 3)
#define MOD_RCLK_512FS		(1 << 3)
#define MOD_RCLK_384FS		(2 << 3)
#define MOD_RCLK_768FS		(3 << 3)
#define MOD_RCLK_MASK		(3 << 3)
#define MOD_BCLK_32FS		(0 << 1)
#define MOD_BCLK_48FS		(1 << 1)
#define MOD_BCLK_16FS		(2 << 1)
#define MOD_BCLK_24FS		(3 << 1)
#define MOD_BCLK_MASK		(3 << 1)

#define FIC_TXFLUSH		(1 << 15)

#endif /* __DRIVERS_BUS_I2S_EXYNOS5_REGS_H__ */
