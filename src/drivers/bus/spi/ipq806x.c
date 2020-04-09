/*
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
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
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/gpio/ipq806x.h"
#include "ipq806x.h"

#define SUCCESS		0

#define DUMMY_DATA_VAL		0
#define TIMEOUT_CNT		100
#define CS_ASSERT		1
#define CS_DEASSERT		0
#define NUM_PORTS		3
#define NUM_GSBI_PINS		3
#define TLMM_ARGS		6
#define NUM_CS			4
#define GSBI_PIN_IDX		0
#define FUNC_SEL_IDX		1
#define GPIO_DIR_IDX		2
#define PULL_CONF_IDX		3
#define DRV_STR_IDX		4
#define GPIO_EN_IDX		5

/* Arbitrarily assigned error code values */
#define ETIMEDOUT -10
#define EINVAL -11
#define EIO -12

#define GSBI_IDX_TO_GSBI(idx)   (idx + 5)
struct gsbi_spi {
        void *spi_config;
        void *io_control;
        void *error_flags;
        void *error_flags_en;
        void *gsbi_ctrl;
        void *qup_config;
        void *qup_error_flags;
        void *qup_error_flags_en;
        void *qup_operational;
        void *qup_io_modes;
        void *qup_state;
        void *qup_input_fifo;
        void *qup_output_fifo;
        void *qup_mx_input_count;
        void *qup_mx_output_count;
        void *qup_sw_reset;
        void *qup_ns_reg;
        void *qup_md_reg;
};

static IpqSpiSlave *to_ipq_spi(SpiOps *spi_ops)
{
	SpiController *controller;

	controller = container_of(spi_ops, SpiController, ops);

	return &controller->ipq_spi_slave;
}
/*
 * TLMM Configuration for SPI NOR
 * gsbi_pin_conf[bus_num][GPIO_NUM, FUNC_SEL, I/O,
 *			       PULL UP/DOWN, DRV_STR, GPIO_FUNC]
 * gsbi_pin_conf[0][x][y] -- GSBI5
 * gsbi_pin_conf[1][x][y] -- GSBI6
 * gsbi_pin_conf[2][x][y] -- GSBI7
*/
static unsigned int gsbi_pin_conf[NUM_PORTS][NUM_GSBI_PINS][TLMM_ARGS] = {
	{
		/* GSBI5 CLK */
		{
			GSBI5_SPI_CLK,  FUNC_SEL_1, GPIO_INPUT,
			GPIO_PULL_DOWN, GPIO_16MA, GPIO_FUNC_DISABLE
		},
		/* GSBI5 MISO */
		{
			GSBI5_SPI_MISO, FUNC_SEL_1, GPIO_INPUT,
			GPIO_PULL_DOWN, GPIO_10MA, GPIO_FUNC_DISABLE
		},
		/* GSBI5 MOSI */
		{
			GSBI5_SPI_MOSI, FUNC_SEL_1, GPIO_INPUT,
			GPIO_PULL_DOWN, GPIO_10MA, GPIO_FUNC_DISABLE
		}
	},
	{
		/* GSBI6 CLK */
		{
			GSBI6_SPI_CLK,  FUNC_SEL_3, GPIO_INPUT,
			GPIO_PULL_DOWN, GPIO_16MA, GPIO_FUNC_DISABLE
		},
		/* GSBI6 MISO */
		{
			GSBI6_SPI_MISO, FUNC_SEL_3, GPIO_INPUT,
			GPIO_PULL_DOWN, GPIO_10MA, GPIO_FUNC_DISABLE
		},
		/* GSBI6 MOSI */
		{
			GSBI6_SPI_MOSI, FUNC_SEL_3, GPIO_INPUT,
			GPIO_PULL_DOWN, GPIO_10MA, GPIO_FUNC_DISABLE
		}
	},
	{
		/* GSBI7 CLK */
		{
			GSBI7_SPI_CLK,  FUNC_SEL_1, GPIO_INPUT,
			GPIO_PULL_DOWN, GPIO_16MA, GPIO_FUNC_DISABLE
		},
		/* GSBI7 MISO */
		{
			GSBI7_SPI_MISO, FUNC_SEL_1, GPIO_INPUT,
			GPIO_PULL_DOWN, GPIO_10MA, GPIO_FUNC_DISABLE
		},
		/* GSBI7 MOSI */
		{
			GSBI7_SPI_MOSI, FUNC_SEL_1, GPIO_INPUT,
			GPIO_PULL_DOWN, GPIO_10MA, GPIO_FUNC_DISABLE
		}
	}
};

/*
 * CS GPIO number array cs_gpio_array[port_num][cs_num]
 * cs_gpio_array[0][x] -- GSBI5
 * cs_gpio_array[1][x] -- GSBI6
 * cs_gpio_array[2][x] -- GSBI7
 */
static unsigned int cs_gpio_array[NUM_PORTS][NUM_CS] = {
	{
		GSBI5_SPI_CS_0, GSBI5_SPI_CS_1, GSBI5_SPI_CS_2, GSBI5_SPI_CS_3
	},
	{
		GSBI6_SPI_CS_0,              0,              0,              0
	},
	{
		GSBI7_SPI_CS_0,              0,              0,              0
	}
};

/*
 * GSBI HCLK state register bit
 * hclk_state[0] -- GSBI5
 * hclk_state[1] -- GSBI6
 * hclk_state[2] -- GSBI7
*/
static unsigned int hclk_state[NUM_PORTS] = {
	GSBI5_HCLK,
	GSBI6_HCLK,
	GSBI7_HCLK
};

/*
 * GSBI QUP_APPS_CLK state register bit
 * qup_apps_clk_state[0] -- GSBI5
 * qup_apps_clk_state[1] -- GSBI6
 * qup_apps_clk_state[2] -- GSBI7
*/
static unsigned int qup_apps_clk_state[NUM_PORTS] = {
	GSBI5_QUP_APPS_CLK,
	GSBI6_QUP_APPS_CLK,
	GSBI7_QUP_APPS_CLK
};


static int check_bit_state(void *reg_addr, int bit_num, int val, int us_delay)
{
	unsigned int count = TIMEOUT_CNT;
	unsigned int bit_val = ((readl(reg_addr) >> bit_num) & 0x01);

	while (bit_val != val) {
		count--;
		if (count == 0)
			return -ETIMEDOUT;
		udelay(us_delay);
		bit_val = ((readl(reg_addr) >> bit_num) & 0x01);
	}

	return SUCCESS;
}

/*
 * Check whether GSBIn_QUP State is valid
 */
static int check_qup_state_valid(IpqSpiSlave *ds)
{

	return check_bit_state(ds->regs->qup_state, QUP_STATE_VALID_BIT,
				QUP_STATE_VALID, 1);

}

/*
 * Configure GSBIn Core state
 */
static int config_spi_state(IpqSpiSlave *ds, unsigned int state)
{
	uint32_t val;
	int ret;
	uint32_t new_state;

	ret = check_qup_state_valid(ds);
	if (ret != SUCCESS)
		return ret;

	switch (state) {
	case SPI_RUN_STATE:
		new_state = QUP_STATE_RUN_STATE;
		break;

	case SPI_RESET_STATE:
		new_state = QUP_STATE_RESET_STATE;
		break;

	case SPI_PAUSE_STATE:
		new_state = QUP_STATE_PAUSE_STATE;
		break;

	default:
		printf("err: unsupported GSBI SPI state : %d\n", state);
		return -EINVAL;
	}

	/* Set the state as requested */
	val = (readl(ds->regs->qup_state) & ~QUP_STATE_MASK)
		| new_state;
	writel(val, ds->regs->qup_state);
	return check_qup_state_valid(ds);
}

/*
 * Set GSBIn SPI Mode
 */
static void spi_set_mode(IpqSpiSlave *ds, unsigned int mode)
{
	unsigned int clk_idle_state;
	unsigned int input_first_mode;
	uint32_t val;

	switch (mode) {
	case GSBI_SPI_MODE_0:
		clk_idle_state = 0;
		input_first_mode = SPI_INPUT_FIRST_MODE;
		break;
	case GSBI_SPI_MODE_1:
		clk_idle_state = 0;
		input_first_mode = 0;
		break;
	case GSBI_SPI_MODE_2:
		clk_idle_state = 1;
		input_first_mode = SPI_INPUT_FIRST_MODE;
		break;
	case GSBI_SPI_MODE_3:
		clk_idle_state = 1;
		input_first_mode = 0;
		break;
	default:
		printf("err : unsupported spi mode : %d\n", mode);
		return;
	}

	val = readl(ds->regs->spi_config);
	val |= input_first_mode;
	writel(val, ds->regs->spi_config);

	val = readl(ds->regs->io_control);
	if (clk_idle_state)
		val |= SPI_IO_CONTROL_CLOCK_IDLE_HIGH;
	else
		val &= ~SPI_IO_CONTROL_CLOCK_IDLE_HIGH;

	writel(val, ds->regs->io_control);
}

/*
 * Check for HCLK state
 */
static int check_hclk_state(unsigned int core_num, int enable)
{
	return check_bit_state(CLK_HALT_CFPB_STATEB_REG,
		hclk_state[core_num], enable, 5);
}

/*
 * Check for QUP APPS CLK state
 */
static int check_qup_clk_state(unsigned int core_num, int enable)
{
	return check_bit_state(CLK_HALT_CFPB_STATEB_REG,
		qup_apps_clk_state[core_num], enable, 5);
}

/*
 * Function to assert and De-assert chip select
 */
static void CS_change(int port_num, int cs_num, int enable)
{
	unsigned int cs_gpio = cs_gpio_array[port_num][cs_num];
	void *addr = GPIO_IN_OUT_ADDR(cs_gpio);
	uint32_t val = readl(addr);

	val &= (~(1 << GPIO_OUTPUT));
	if (!enable)
		val |= (1 << GPIO_OUTPUT);
	writel(val, addr);
}

/*
 * GSBIn TLMM configuration
 */
static void gsbi_pin_config(unsigned int port_num, int cs_num)
{
	unsigned int gpio;
	unsigned int i;
	/* Hold the GSBIn (core_num) core in reset */
	clrsetbits_le32(GSBIn_RESET_REG(GSBI_IDX_TO_GSBI(port_num)),
			GSBI1_RESET_MSK, GSBI1_RESET);

	/*
	 * Configure SPI_CLK, SPI_MISO and SPI_MOSI
	 */
	for (i = 0; i < NUM_GSBI_PINS; i++) {
		unsigned int func_sel;
		unsigned int io_config;
		unsigned int pull_config;
		unsigned int drv_strength;
		unsigned int gpio_en;
		unsigned int *ptr;

		ptr = gsbi_pin_conf[port_num][i];
		gpio		= *(ptr + GSBI_PIN_IDX);
		func_sel	= *(ptr + FUNC_SEL_IDX);
		io_config	= *(ptr + GPIO_DIR_IDX);
		pull_config	= *(ptr + PULL_CONF_IDX);
		drv_strength	= *(ptr + DRV_STR_IDX);
		gpio_en	= *(ptr + GPIO_EN_IDX);

		gpio_tlmm_config(gpio, func_sel, io_config,
				 pull_config, drv_strength, gpio_en);
	}

	gpio = cs_gpio_array[port_num][cs_num];
	/* configure CS */
	gpio_tlmm_config(gpio, FUNC_SEL_GPIO, GPIO_OUTPUT, GPIO_PULL_UP,
				GPIO_10MA, GPIO_FUNC_ENABLE);
	CS_change(port_num, cs_num, CS_DEASSERT);
}

/*
 * Clock configuration for GSBIn Core
 */
static int gsbi_clock_init(IpqSpiSlave *ds)
{
	int ret;

	/* Hold the GSBIn (core_num) core in reset */
	clrsetbits_le32(GSBIn_RESET_REG(GSBI_IDX_TO_GSBI(ds->slave.bus)),
			GSBI1_RESET_MSK, GSBI1_RESET);

	/* Disable GSBIn (core_num) QUP core clock branch */
	clrsetbits_le32(ds->regs->qup_ns_reg, QUP_CLK_BRANCH_ENA_MSK,
					QUP_CLK_BRANCH_DIS);

	ret = check_qup_clk_state(ds->slave.bus, 1);
	if (ret) {
		printf("QUP Clock Halt For GSBI%d failed!\n", ds->slave.bus);
		return ret;
	}

	/* Disable M/N:D counter and hold M/N:D counter in reset */
	clrsetbits_le32(ds->regs->qup_ns_reg, (MNCNTR_MSK | MNCNTR_RST_MSK),
					(MNCNTR_RST_ENA | MNCNTR_DIS));

	/* Disable GSBIn (core_num) QUP core clock root */
	clrsetbits_le32(ds->regs->qup_ns_reg, CLK_ROOT_ENA_MSK, CLK_ROOT_DIS);

	clrsetbits_le32(ds->regs->qup_ns_reg, GSBIn_PLL_SRC_MSK,
					GSBIn_PLL_SRC_PLL8);
	clrsetbits_le32(ds->regs->qup_ns_reg, GSBIn_PRE_DIV_SEL_MSK,
						(0 << GSBI_PRE_DIV_SEL_SHFT));

	/* Program M/N:D values for GSBIn_QUP_APPS_CLK @50MHz */
	clrsetbits_le32(ds->regs->qup_md_reg, GSBIn_M_VAL_MSK,
						(0x01 << GSBI_M_VAL_SHFT));
	clrsetbits_le32(ds->regs->qup_md_reg, GSBIn_D_VAL_MSK,
						(0xF7 << GSBI_D_VAL_SHFT));
	clrsetbits_le32(ds->regs->qup_ns_reg, GSBIn_N_VAL_MSK,
						(0xF8 << GSBI_N_VAL_SHFT));

	/* Set MNCNTR_MODE = 0: Bypass mode */
	clrsetbits_le32(ds->regs->qup_ns_reg, MNCNTR_MODE_MSK,
					MNCNTR_MODE_DUAL_EDGE);

	/* De-assert the M/N:D counter reset */
	clrsetbits_le32(ds->regs->qup_ns_reg, MNCNTR_RST_MSK, MNCNTR_RST_DIS);
	clrsetbits_le32(ds->regs->qup_ns_reg, MNCNTR_MSK, MNCNTR_EN);

	/*
	 * Enable the GSBIn (core_num) QUP core clock root.
	 * Keep MND counter disabled
	 */
	clrsetbits_le32(ds->regs->qup_ns_reg, CLK_ROOT_ENA_MSK, CLK_ROOT_ENA);

	/* Enable GSBIn (core_num) QUP core clock branch */
	clrsetbits_le32(ds->regs->qup_ns_reg, QUP_CLK_BRANCH_ENA_MSK,
						QUP_CLK_BRANCH_ENA);

	ret = check_qup_clk_state(ds->slave.bus, 0);
	if (ret) {
		printf("QUP Clock Enable For GSBI%d"
				" failed!\n", ds->slave.bus);
		return ret;
	}

	/* Enable GSBIn (core_num) core clock branch */
	clrsetbits_le32(GSBIn_HCLK_CTL_REG(GSBI_IDX_TO_GSBI(ds->slave.bus)),
			GSBI_CLK_BRANCH_ENA_MSK, GSBI_CLK_BRANCH_ENA);

	ret = check_hclk_state(ds->slave.bus, 0);
	if (ret) {
		printf("HCLK Enable For GSBI%d failed!\n", ds->slave.bus);
		return ret;
	}

	/* Release GSBIn (core_num) core from reset */
	clrsetbits_le32(GSBIn_RESET_REG(GSBI_IDX_TO_GSBI(ds->slave.bus)),
						GSBI1_RESET_MSK, 0);
	udelay(50);

	return SUCCESS;
}

/*
 * Reset entire QUP and all mini cores
 */
static void spi_reset(IpqSpiSlave *ds)
{
	writel(0x1, ds->regs->qup_sw_reset);
	udelay(5);
}

static const struct gsbi_spi spi_reg[] = {
	/* GSBI5 registers for SPI interface */
	{
		GSBI5_SPI_CONFIG_REG,
		GSBI5_SPI_IO_CONTROL_REG,
		GSBI5_SPI_ERROR_FLAGS_REG,
		GSBI5_SPI_ERROR_FLAGS_EN_REG,
		GSBI5_GSBI_CTRL_REG_REG,
		GSBI5_QUP_CONFIG_REG,
		GSBI5_QUP_ERROR_FLAGS_REG,
		GSBI5_QUP_ERROR_FLAGS_EN_REG,
		GSBI5_QUP_OPERATIONAL_REG,
		GSBI5_QUP_IO_MODES_REG,
		GSBI5_QUP_STATE_REG,
		GSBI5_QUP_INPUT_FIFOc_REG(0),
		GSBI5_QUP_OUTPUT_FIFOc_REG(0),
		GSBI5_QUP_MX_INPUT_COUNT_REG,
		GSBI5_QUP_MX_OUTPUT_COUNT_REG,
		GSBI5_QUP_SW_RESET_REG,
		GSBIn_QUP_APPS_NS_REG(5),
		GSBIn_QUP_APPS_MD_REG(5),
	},
	/* GSBI6 registers for SPI interface */
	{
		GSBI6_SPI_CONFIG_REG,
		GSBI6_SPI_IO_CONTROL_REG,
		GSBI6_SPI_ERROR_FLAGS_REG,
		GSBI6_SPI_ERROR_FLAGS_EN_REG,
		GSBI6_GSBI_CTRL_REG_REG,
		GSBI6_QUP_CONFIG_REG,
		GSBI6_QUP_ERROR_FLAGS_REG,
		GSBI6_QUP_ERROR_FLAGS_EN_REG,
		GSBI6_QUP_OPERATIONAL_REG,
		GSBI6_QUP_IO_MODES_REG,
		GSBI6_QUP_STATE_REG,
		GSBI6_QUP_INPUT_FIFOc_REG(0),
		GSBI6_QUP_OUTPUT_FIFOc_REG(0),
		GSBI6_QUP_MX_INPUT_COUNT_REG,
		GSBI6_QUP_MX_OUTPUT_COUNT_REG,
		GSBI6_QUP_SW_RESET_REG,
		GSBIn_QUP_APPS_NS_REG(6),
		GSBIn_QUP_APPS_MD_REG(6),
	},
	/* GSBI7 registers for SPI interface */
	{
		GSBI7_SPI_CONFIG_REG,
		GSBI7_SPI_IO_CONTROL_REG,
		GSBI7_SPI_ERROR_FLAGS_REG,
		GSBI7_SPI_ERROR_FLAGS_EN_REG,
		GSBI7_GSBI_CTRL_REG_REG,
		GSBI7_QUP_CONFIG_REG,
		GSBI7_QUP_ERROR_FLAGS_REG,
		GSBI7_QUP_ERROR_FLAGS_EN_REG,
		GSBI7_QUP_OPERATIONAL_REG,
		GSBI7_QUP_IO_MODES_REG,
		GSBI7_QUP_STATE_REG,
		GSBI7_QUP_INPUT_FIFOc_REG(0),
		GSBI7_QUP_OUTPUT_FIFOc_REG(0),
		GSBI7_QUP_MX_INPUT_COUNT_REG,
		GSBI7_QUP_MX_OUTPUT_COUNT_REG,
		GSBI7_QUP_SW_RESET_REG,
		GSBIn_QUP_APPS_NS_REG(7),
		GSBIn_QUP_APPS_MD_REG(7),
	}
};

static void spi_setup_slave(unsigned int bus,
			    unsigned int cs,
			    IpqSpiSlave *ds)
{
	/*
	 * IPQ GSBI (Generic Serial Bus Interface) supports SPI Flash on
	 * different ports (GSBI5, GSBI6 and GSBI7) with different number of
	 * chip selects (CS aka channels):
	*/
	if ((bus < GSBI5_SPI) || (bus > GSBI7_SPI)
	    || ((bus == GSBI5_SPI) && (cs > 3))
	    || ((bus != GSBI5_SPI) && (cs != 0))) {
		printf("SPI error: unsupported bus %d "
		       "(Supported busses 0,1 and 2) or chipselect\n", bus);
	}

	ds->slave.bus	= bus;
	ds->slave.cs	= cs;
	ds->regs	= &spi_reg[bus];

	/*
	 * TODO(vbendeb):
	 * hardcoded frequency and mode - we might need to find a way
	 * to configure this
	 */
	ds->freq = 10000000;
	ds->mode = GSBI_SPI_MODE_0;
}

/*
 * GSBIn SPI Hardware Initialisation
 */
static int spi_hw_init(IpqSpiSlave *ds)
{
	int ret;

	if (ds->initialized)
		return 0;

	/* GSBI module configuration */
	spi_reset(ds);

	/* Set the GSBIn QUP state */
	ret = config_spi_state(ds, SPI_RESET_STATE);
	if (ret)
		return ret;

	/* Configure GSBI_CTRL register to set protocol_mode to SPI:011 */
	clrsetbits_le32(ds->regs->gsbi_ctrl, PROTOCOL_CODE_MSK,
					PROTOCOL_CODE_SPI);

	/*
	 * Configure Mini core to SPI core with Input Output enabled,
	 * SPI master, N = 8 bits
	 */
	clrsetbits_le32(ds->regs->qup_config, (QUP_CONFIG_MINI_CORE_MSK |
					       SPI_QUP_CONF_INPUT_MSK |
					       SPI_QUP_CONF_OUTPUT_MSK |
					       SPI_BIT_WORD_MSK),
					      (QUP_CONFIG_MINI_CORE_SPI |
					       SPI_QUP_CONF_NO_INPUT |
					       SPI_QUP_CONF_NO_OUTPUT |
					       SPI_8_BIT_WORD));

	/*
	 * Configure Input first SPI protocol,
	 * SPI master mode and no loopback
	 */
	clrsetbits_le32(ds->regs->spi_config, (LOOP_BACK_MSK |
					       SLAVE_OPERATION_MSK),
					      (NO_LOOP_BACK |
					       SLAVE_OPERATION));

	/*
	 * Configure SPI IO Control Register
	 * CLK_ALWAYS_ON = 0
	 * MX_CS_MODE = 0
	 * NO_TRI_STATE = 1
	 */
	writel((CLK_ALWAYS_ON | MX_CS_MODE | NO_TRI_STATE),
				ds->regs->io_control);

	/*
	 * Configure SPI IO Modes.
	 * OUTPUT_BIT_SHIFT_EN = 1
	 * INPUT_MODE = Block Mode
	 * OUTPUT MODE = Block Mode
	 */
	clrsetbits_le32(ds->regs->qup_io_modes, (OUTPUT_BIT_SHIFT_MSK |
						 INPUT_BLOCK_MODE_MSK |
						 OUTPUT_BLOCK_MODE_MSK),
						(OUTPUT_BIT_SHIFT_EN |
						 INPUT_BLOCK_MODE |
						 OUTPUT_BLOCK_MODE));

	spi_set_mode(ds, ds->mode);

	/* Disable Error mask */
	writel(0, ds->regs->error_flags_en);
	writel(0, ds->regs->qup_error_flags_en);

	ds->initialized = 1;

	return SUCCESS;
}

static int spi_claim_bus(SpiOps *spi_ops)
{
	IpqSpiSlave *ds = to_ipq_spi(spi_ops);
	unsigned int ret;

	if (ds->initialized)
		return SUCCESS;

	/* GPIO Configuration for SPI port */
	gsbi_pin_config(ds->slave.bus, ds->slave.cs);

	/* Clock configuration */
	ret = gsbi_clock_init(ds);
	if (ret)
		return ret;

	ret = spi_hw_init(ds);
	if (ret)
		return -EIO;

	/* Assert the chip select */
	CS_change(ds->slave.bus, ds->slave.cs, CS_ASSERT);

	return SUCCESS;
}

static int spi_release_bus(SpiOps *ops)
{
	IpqSpiSlave *ds = to_ipq_spi(ops);

	/* Deassert CS */
	CS_change(ds->slave.bus, ds->slave.cs, CS_DEASSERT);

	/* Reset the SPI hardware */
	spi_reset(ds);
	ds->initialized = 0;
	return 0;
}

static int spi_xfer(SpiOps *ops, void *din, const void *dout, uint32_t bytes)
{
	int ret;
	uint8_t* dbuf;
	const uint8_t* dobuf;
	IpqSpiSlave *ds = to_ipq_spi(ops);
	unsigned in_bytes, out_bytes;

	in_bytes = din ? bytes : 0;
	out_bytes = dout ? bytes : 0;

	ret = config_spi_state(ds, SPI_RESET_STATE);
	if (ret)
		goto out;

	if (!out_bytes)
		goto spi_receive;

	/*
	 * Let's do the write side of the transaction first. Enable output
	 * FIFO.
	 */
	clrsetbits_le32(ds->regs->qup_config, SPI_QUP_CONF_OUTPUT_MSK,
			  SPI_QUP_CONF_OUTPUT_ENA);

	writel(out_bytes, ds->regs->qup_mx_output_count);

	ret = config_spi_state(ds, SPI_RUN_STATE);
	if (ret)
		goto out;

	dobuf = dout; /* Alias to make it possible to use pointer autoinc. */

	while (out_bytes) {
		if (readl(ds->regs->qup_operational) & QUP_OUTPUT_FIFO_FULL)
			continue;

		writel(*dobuf++, ds->regs->qup_output_fifo);
		out_bytes--;

		/* Wait for output FIFO to drain. */
		if (!out_bytes)
			while (readl(ds->regs->qup_operational) &
			       QUP_OUTPUT_FIFO_NOT_EMPTY)
				;
	}

	ret = config_spi_state(ds, SPI_RESET_STATE);
	if (ret)
		goto out;

 spi_receive:
	if (!in_bytes) /* Nothing to read. */
		goto out;

	/* Enable input FIFO */
	clrsetbits_le32(ds->regs->qup_config, SPI_QUP_CONF_INPUT_MSK,
			  SPI_QUP_CONF_INPUT_ENA);

	writel(in_bytes, ds->regs->qup_mx_input_count);
	writel(in_bytes, ds->regs->qup_mx_output_count);

	ret = config_spi_state(ds, SPI_RUN_STATE);
	if (ret)
		goto out;

	/* Seed clocking */
	writel(0xff, ds->regs->qup_output_fifo);
	dbuf = din;  /* Alias for pointer autoincrement again. */
	while (in_bytes) {
		if (!(readl(ds->regs->qup_operational) &
		      QUP_INPUT_FIFO_NOT_EMPTY))
			continue;
		/* Keep it clocking */
		writel(0xff, ds->regs->qup_output_fifo);

		*dbuf++ = readl(ds->regs->qup_input_fifo) & 0xff;
		in_bytes--;
	}

 out:
	/*
	 * Put the SPI Core back in the Reset State
	 * to end the transfer.
	 */
	(void)config_spi_state(ds, SPI_RESET_STATE);

	return ret;
}

SpiController *new_spi(unsigned bus_num, unsigned cs)
{
	SpiController *bus = xzalloc(sizeof(*bus));

	if (bus) {
		bus->ops.start = spi_claim_bus;
		bus->ops.transfer = spi_xfer;
		bus->ops.stop = spi_release_bus;
		spi_setup_slave(bus_num, cs, &bus->ipq_spi_slave);
	}
	return bus;
}
