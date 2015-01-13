/*
 * Copyright 2014 Google Inc.
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

#ifndef __SRC_DRIVERS_BUS_SPI_IPQ806X_H__
#define __SRC_DRIVERS_BUS_SPI_IPQ806X_H__

#include "drivers/flash/spi.h"
#include "drivers/bus/spi/spi.h"

#define QUP5_BASE       ((unsigned char *)0x1a280000)
#define QUP6_BASE       ((unsigned char *)0x16580000)
#define QUP7_BASE       ((unsigned char *)0x16680000)
#define GSBI5_BASE      ((unsigned char *)0x1a200000)
#define GSBI6_BASE      ((unsigned char *)0x16500000)
#define GSBI7_BASE      ((unsigned char *)0x16600000)

#define BIOS_ERR        3   /* error conditions                     */
#define CLK_ALWAYS_ON                           (0 << 9)
#define CLK_ROOT_DIS                            (0 << 11)
#define CLK_ROOT_ENA                            (1 << 11)
#define CLK_ROOT_ENA_MSK                        (1 << 11)
#define GSBI1_RESET                             (1 << 0)
#define GSBI1_RESET_MSK                         0x1
#define GSBI5_GSBI_CTRL_REG_REG         (GSBI5_REG_BASE + 0x00000000)
#define GSBI5_HCLK                              23
#define GSBI5_QUP5_REG_BASE              (QUP5_BASE + 0x00000000)
#define GSBI5_QUP_APPS_CLK                      20
#define GSBI5_QUP_CONFIG_REG            (GSBI5_QUP5_REG_BASE + 0x00000000)
#define GSBI5_QUP_ERROR_FLAGS_EN_REG    (GSBI5_QUP5_REG_BASE + 0x00000020)
#define GSBI5_QUP_ERROR_FLAGS_REG       (GSBI5_QUP5_REG_BASE      + 0x0000001c)
#define GSBI5_QUP_IO_MODES_REG          (GSBI5_QUP5_REG_BASE + 0x00000008)
#define GSBI5_QUP_MX_INPUT_COUNT_REG    (GSBI5_QUP5_REG_BASE + 0x00000200)
#define GSBI5_QUP_MX_OUTPUT_COUNT_REG   (GSBI5_QUP5_REG_BASE + 0x00000100)
#define GSBI5_QUP_OPERATIONAL_REG       (GSBI5_QUP5_REG_BASE + 0x00000018)
#define GSBI5_QUP_STATE_REG             (GSBI5_QUP5_REG_BASE + 0x00000004)
#define GSBI5_QUP_SW_RESET_REG          (GSBI5_QUP5_REG_BASE + 0x0000000c)
#define GSBI5_REG_BASE                   (GSBI5_BASE + 0x00000000)
#define GSBI5_SPI                               0
#define GSBI5_SPI_CLK                           21
#define GSBI5_SPI_CONFIG_REG            (GSBI5_QUP5_REG_BASE + 0x00000300)
#define GSBI5_SPI_CS_0                          20
#define GSBI5_SPI_CS_1                          61
#define GSBI5_SPI_CS_2                          62
#define GSBI5_SPI_CS_3                          2
#define GSBI5_SPI_ERROR_FLAGS_EN_REG    (GSBI5_QUP5_REG_BASE + 0x0000030c)
#define GSBI5_SPI_ERROR_FLAGS_REG       (GSBI5_QUP5_REG_BASE + 0x00000308)
#define GSBI5_SPI_IO_CONTROL_REG        (GSBI5_QUP5_REG_BASE + 0x00000304)
#define GSBI5_SPI_MISO                          19
#define GSBI5_SPI_MOSI                          18
#define GSBI6_GSBI_CTRL_REG_REG         (GSBI6_REG_BASE + 0x00000000)
#define GSBI6_HCLK                              19
#define GSBI6_QUP6_REG_BASE              (QUP6_BASE + 0x00000000)
#define GSBI6_QUP_APPS_CLK                      16
#define GSBI6_QUP_CONFIG_REG            (GSBI6_QUP6_REG_BASE + 0x00000000)
#define GSBI6_QUP_ERROR_FLAGS_EN_REG    (GSBI6_QUP6_REG_BASE + 0x00000020)
#define GSBI6_QUP_ERROR_FLAGS_REG       (GSBI6_QUP6_REG_BASE      + 0x0000001c)
#define GSBI6_QUP_IO_MODES_REG          (GSBI6_QUP6_REG_BASE + 0x00000008)
#define GSBI6_QUP_MX_INPUT_COUNT_REG    (GSBI6_QUP6_REG_BASE + 0x00000200)
#define GSBI6_QUP_MX_OUTPUT_COUNT_REG   (GSBI6_QUP6_REG_BASE + 0x00000100)
#define GSBI6_QUP_OPERATIONAL_REG       (GSBI6_QUP6_REG_BASE + 0x00000018)
#define GSBI6_QUP_STATE_REG             (GSBI6_QUP6_REG_BASE + 0x00000004)
#define GSBI6_QUP_SW_RESET_REG          (GSBI6_QUP6_REG_BASE + 0x0000000c)
#define GSBI6_REG_BASE                   (GSBI6_BASE + 0x00000000)
#define GSBI6_SPI                               1
#define GSBI6_SPI_CLK                           30
#define GSBI6_SPI_CONFIG_REG            (GSBI6_QUP6_REG_BASE + 0x00000300)
#define GSBI6_SPI_CS_0                          29
#define GSBI6_SPI_ERROR_FLAGS_EN_REG    (GSBI6_QUP6_REG_BASE + 0x0000030c)
#define GSBI6_SPI_ERROR_FLAGS_REG       (GSBI6_QUP6_REG_BASE + 0x00000308)
#define GSBI6_SPI_IO_CONTROL_REG        (GSBI6_QUP6_REG_BASE + 0x00000304)
#define GSBI6_SPI_MISO                          28
#define GSBI6_SPI_MOSI                          27
#define GSBI7_GSBI_CTRL_REG_REG         (GSBI7_REG_BASE + 0x00000000)
#define GSBI7_HCLK                              15
#define GSBI7_QUP7_REG_BASE              (QUP7_BASE + 0x00000000)
#define GSBI7_QUP_APPS_CLK                      12
#define GSBI7_QUP_CONFIG_REG            (GSBI7_QUP7_REG_BASE + 0x00000000)
#define GSBI7_QUP_ERROR_FLAGS_EN_REG    (GSBI7_QUP7_REG_BASE + 0x00000020)
#define GSBI7_QUP_ERROR_FLAGS_REG       (GSBI7_QUP7_REG_BASE      + 0x0000001c)
#define GSBI7_QUP_IO_MODES_REG          (GSBI7_QUP7_REG_BASE + 0x00000008)
#define GSBI7_QUP_MX_INPUT_COUNT_REG    (GSBI7_QUP7_REG_BASE + 0x00000200)
#define GSBI7_QUP_MX_OUTPUT_COUNT_REG   (GSBI7_QUP7_REG_BASE + 0x00000100)
#define GSBI7_QUP_OPERATIONAL_REG       (GSBI7_QUP7_REG_BASE + 0x00000018)
#define GSBI7_QUP_STATE_REG             (GSBI7_QUP7_REG_BASE + 0x00000004)
#define GSBI7_QUP_SW_RESET_REG          (GSBI7_QUP7_REG_BASE + 0x0000000c)
#define GSBI7_REG_BASE                   (GSBI7_BASE + 0x00000000)
#define GSBI7_SPI                               2
#define GSBI7_SPI_CLK                           9
#define GSBI7_SPI_CONFIG_REG            (GSBI7_QUP7_REG_BASE + 0x00000300)
#define GSBI7_SPI_CS_0                          8
#define GSBI7_SPI_ERROR_FLAGS_EN_REG    (GSBI7_QUP7_REG_BASE + 0x0000030c)
#define GSBI7_SPI_ERROR_FLAGS_REG       (GSBI7_QUP7_REG_BASE + 0x00000308)
#define GSBI7_SPI_IO_CONTROL_REG        (GSBI7_QUP7_REG_BASE + 0x00000304)
#define GSBI7_SPI_MISO                          7
#define GSBI7_SPI_MOSI                          6
#define GSBI_CLK_BRANCH_ENA                     (1 << 4)
#define GSBI_CLK_BRANCH_ENA_MSK                 (1 << 4)
#define GSBI_D_VAL_SHFT                         0
#define GSBI_M_VAL_SHFT                         16
#define GSBI_N_VAL_SHFT                         16
#define GSBI_PRE_DIV_SEL_SHFT                   3
#define GSBI_SPI_MODE_0                         0
#define GSBI_SPI_MODE_1                         1
#define GSBI_SPI_MODE_2                         2
#define GSBI_SPI_MODE_3                         3
#define GSBIn_D_VAL_MSK                         (0xFF << GSBI_D_VAL_SHFT)
#define GSBIn_M_VAL_MSK                         (0xFF << GSBI_M_VAL_SHFT)
#define GSBIn_N_VAL_MSK                         (0xFF << GSBI_N_VAL_SHFT)
#define GSBIn_PLL_SRC_MSK                       (0x03 << 0)
#define GSBIn_PLL_SRC_PLL8                      (0x3 << 0)
#define GSBIn_PRE_DIV_SEL_MSK                   (0x3 << GSBI_PRE_DIV_SEL_SHFT)
#define INPUT_BLOCK_MODE                        (0x01 << 12)
#define INPUT_BLOCK_MODE_MSK                    (0x03 << 12)
#define LOOP_BACK_MSK                           (1 << 8)
#define MNCNTR_DIS                              (0 << 8)
#define MNCNTR_EN                               (1 << 8)
#define MNCNTR_ENABLE                   (0x1 << 8)
#define MNCNTR_MODE_DUAL_EDGE           (0x2 << 5)
#define MNCNTR_MODE_MSK                         (0x3 << 5)
#define MNCNTR_MSK                              (1 << 8)
#define MNCNTR_RST_DIS                          (0 << 7)
#define MNCNTR_RST_ENA                          (1 << 7)
#define MNCNTR_RST_MSK                          (1 << 7)
#define MX_CS_MODE                              (0 << 8)
#define NO_LOOP_BACK                            (0 << 8)
#define NO_TRI_STATE                            (1 << 0)
#define OUTPUT_BIT_SHIFT_EN                     (1 << 16)
#define OUTPUT_BIT_SHIFT_MSK                    (1 << 16)
#define OUTPUT_BLOCK_MODE                       (0x01 << 10)
#define OUTPUT_BLOCK_MODE_MSK                   (0x03 << 10)
#define PROTOCOL_CODE_MSK                       (0x07 << 4)
#define PROTOCOL_CODE_SPI                       (0x03 << 4)
#define QUP_CLK_BRANCH_DIS                      (0 << 9)
#define QUP_CLK_BRANCH_ENA                      (1 << 9)
#define QUP_CLK_BRANCH_ENA_MSK                  (1 << 9)
#define QUP_CONFIG_MINI_CORE_MSK                (0x0F << 8)
#define QUP_CONFIG_MINI_CORE_SPI                (1 << 8)
#define QUP_INPUT_FIFO_NOT_EMPTY                (1 << 5)
#define QUP_OUTPUT_FIFO_FULL                    (1 << 6)
#define QUP_OUTPUT_FIFO_NOT_EMPTY               (1 << 4)
#define QUP_STATE_MASK                          0x3
#define QUP_STATE_PAUSE_STATE                   0x3
#define QUP_STATE_RESET_STATE                   0x0
#define QUP_STATE_RUN_STATE                     0x1
#define QUP_STATE_VALID                         1
#define QUP_STATE_VALID_BIT                     2
#define SLAVE_OPERATION                         (0 << 5)
#define SLAVE_OPERATION_MSK                     (1 << 5)
#define SPI_8_BIT_WORD                          0x07
#define SPI_BIT_WORD_MSK                        0x1F
#define SPI_INPUT_FIRST_MODE                    (1 << 9)
#define SPI_IO_CONTROL_CLOCK_IDLE_HIGH          (1 << 10)
#define SPI_PAUSE_STATE                         3
#define SPI_QUP_CONF_INPUT_ENA                  (0 << 7)
#define SPI_QUP_CONF_INPUT_MSK                  (1 << 7)
#define SPI_QUP_CONF_NO_INPUT                   (1 << 7)
#define SPI_QUP_CONF_NO_OUTPUT                  (1 << 6)
#define SPI_QUP_CONF_OUTPUT_ENA                 (0 << 6)
#define SPI_QUP_CONF_OUTPUT_MSK                 (1 << 6)
#define SPI_RESET_STATE                         0
#define SPI_RUN_STATE                           1
#define CLK_CTL_REG_BASE                ((unsigned char *)0x00900000)
#define CLK_HALT_CFPB_STATEB_REG               (CLK_CTL_REG_BASE      + 0x00002fd0)

#define GSBI5_QUP_INPUT_FIFOc_REG(c)  (GSBI5_QUP5_REG_BASE + 0x00000218 + 4 * (c))
#define GSBI5_QUP_OUTPUT_FIFOc_REG(c) (GSBI5_QUP5_REG_BASE + 0x00000110 + 4 * (c))
#define GSBI6_QUP_INPUT_FIFOc_REG(c)  (GSBI6_QUP6_REG_BASE + 0x00000218 + 4 * (c))
#define GSBI6_QUP_OUTPUT_FIFOc_REG(c) (GSBI6_QUP6_REG_BASE + 0x00000110 + 4 * (c))
#define GSBI7_QUP_INPUT_FIFOc_REG(c) (GSBI7_QUP7_REG_BASE + 0x00000218 + 4 * (c))
#define GSBI7_QUP_OUTPUT_FIFOc_REG(c) (GSBI7_QUP7_REG_BASE + 0x00000110 + 4 * (c))
#define GSBIn_HCLK_CTL_REG(n) (CLK_CTL_REG_BASE      + 0x000029c0 + 32 * ((n)-1))
#define GSBIn_QUP_APPS_MD_REG(n) (CLK_CTL_REG_BASE      + 0x000029c8 + 32 * ((n)-1))
#define GSBIn_QUP_APPS_NS_REG(n) (CLK_CTL_REG_BASE      + 0x000029cc + 32 * ((n)-1))
#define GSBIn_RESET_REG(n)  (CLK_CTL_REG_BASE      + 0x000029dc + 32 * ((n)-1))


struct spi_slave {
        unsigned int    bus;
        unsigned int    cs;
        unsigned int    rw;
};

typedef struct {
        struct spi_slave slave;
        const struct gsbi_spi *regs;
        unsigned mode;
        unsigned initialized;
        unsigned long freq;
} IpqSpiSlave;

typedef struct {
	SpiOps ops;
	IpqSpiSlave ipq_spi_slave;
} SpiController;

extern SpiController *new_spi(unsigned bus_num, unsigned cs);

#endif /* __SRC_DRIVERS_BUS_SPI_IPQ806X_H__ */

