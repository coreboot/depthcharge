/*
 * Copyright 2015 Mediatek Inc.
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

#include "base/container_of.h"
#include "drivers/bus/i2c/mtk_i2c.h"

/*
 * Define TPM I2C number for I2C workaround.
 * TODO: Once DMA is supported, we will remove this part.
 */
#define TPM_I2C_BUS_NUM 2

/* ---------------------- Register Settings ----------------------------------- */
static inline void I2C_START_TRANSAC(uint32_t i2c_base)
{
	writew(0x1, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_START));
}

static inline void I2C_FIFO_CLR_ADDR(uint32_t i2c_base)
{
	writew(0x1, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_FIFO_ADDR_CLR));
}

static inline uint16_t I2C_FIFO_OFFSET(uint32_t i2c_base)
{
	return readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_FIFO_STAT)) >> 4 & 0xf;
}

static inline uint16_t I2C_FIFO_IS_EMPTY(uint32_t i2c_base)
{
	return readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_FIFO_STAT)) >> 0 & 0x1;
}

static inline void I2C_SOFTRESET(uint32_t i2c_base)
{
	writew(0x1, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_SOFTRESET));
}

static inline void I2C_PATH_PMIC(uint32_t i2c_base)
{
	writew(0x1, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_PATH_DIR));
}

static inline void I2C_PATH_NORMAL(uint32_t i2c_base)
{
	writew(0x0, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_PATH_DIR));
}

static inline uint16_t I2C_INTR_STATUS(uint32_t i2c_base)
{
	return readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_INTR_STAT));
}

static inline void I2C_SET_FIFO_THRESH(uint32_t i2c_base, uint8_t tx, uint8_t rx)
{
	uint16_t tmp = (((tx) & 0x7) << I2C_TX_THR_OFFSET) |
			(((rx) & 0x7) << I2C_RX_THR_OFFSET);
	writew(tmp, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_FIFO_THRESH));
}

static inline void I2C_SET_INTR_MASK(uint32_t i2c_base, uint16_t mask)
{
	writew(mask, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_INTR_MASK));
}

static inline void I2C_CLR_INTR_MASK(uint32_t i2c_base, uint16_t mask)
{
	uint16_t tmp = readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_INTR_MASK));

	tmp &= ~(mask);
	writew(tmp, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_INTR_MASK));
}

static inline void I2C_SET_SLAVE_ADDR(uint32_t i2c_base, uint16_t addr)
{
	writew(addr, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_SLAVE_ADDR));
}

static inline void I2C_SET_TRANS_LEN(uint32_t i2c_base, uint16_t len)
{
	writew(len, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_TRANSFER_LEN));
}

static inline void I2C_SET_TRANS_AUX_LEN(uint32_t i2c_base, uint16_t len)
{
	writew(len, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_TRANSFER_AUX_LEN));
}

static inline void I2C_SET_TRANSAC_LEN(uint32_t i2c_base, uint16_t len)
{
	writew(len, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_TRANSAC_LEN));
}

static inline void I2C_SET_TRANS_DELAY(uint32_t i2c_base, uint16_t delay)
{
	writew(delay, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_DELAY_LEN));
}

static inline void I2C_SET_TRANS_CTRL(uint32_t i2c_base, uint16_t ctrl)
{
	uint16_t tmp = readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_CONTROL))
			     & ~I2C_CONTROL_MASK;

	tmp |= ((ctrl) & I2C_CONTROL_MASK);
	writew(tmp, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_CONTROL));
}

static inline void I2C_SET_HS_MODE(uint32_t i2c_base, uint16_t on_off)
{
	uint16_t tmp = readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_HS)) & ~0x1;

	tmp |= (on_off & 0x1);
	writew(tmp, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_HS));
}

static inline void I2C_READ_BYTE(uint32_t i2c_base, uint8_t *byte)
{
	*byte = readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_DATA_PORT));
}

static inline void I2C_WRITE_BYTE(uint32_t i2c_base, uint8_t byte)
{
	writew(byte, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_DATA_PORT));
}

static inline void I2C_CLR_INTR_STATUS(uint32_t i2c_base, uint16_t status)
{
	writew(status, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_INTR_STAT));
}

uint32_t mtk_i2c_set_speed(struct mtk_i2c_t *i2c, uint32_t clock)
{
	uint32_t ret_code = I2C_OK;
	uint32_t i2c_base = i2c->base;
	I2C_SPD_MODE mode = i2c->mode;
	uint32_t khz = i2c->speed;

	uint16_t sample_cnt_div;
	uint16_t step_cnt_div;
	uint16_t max_step_cnt_div = (mode == HS_MODE) ?
				    MAX_HS_STEP_CNT_DIV : MAX_STEP_CNT_DIV;
	uint32_t reg_value;
	uint32_t sclk;

	uint32_t diff;
	uint32_t min_diff = clock;
	uint16_t sample_div = MAX_SAMPLE_CNT_DIV;
	uint16_t step_div = max_step_cnt_div;

	for (sample_cnt_div = 1; sample_cnt_div <= MAX_SAMPLE_CNT_DIV;
	     sample_cnt_div++) {
		for (step_cnt_div = 1; step_cnt_div <= max_step_cnt_div;
		     step_cnt_div++) {
			sclk = (clock >> 1) /
			       (sample_cnt_div * step_cnt_div);
			if (sclk > khz)
				continue;
			diff = khz - sclk;

			if (diff < min_diff) {
				min_diff = diff;
				sample_div = sample_cnt_div;
				step_div = step_cnt_div;
			}
		}
	}

	sample_cnt_div = sample_div;
	step_cnt_div = step_div;

	sclk = clock / (2 * sample_cnt_div * step_cnt_div);
	if (sclk > khz) {
		ret_code = I2C_SET_SPEED_FAIL_OVER_SPEED;
		return ret_code;
	}

	step_cnt_div--;
	sample_cnt_div--;

	if (mode == HS_MODE) {
		reg_value = readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_HS)) &
			    ((0x7 << 12) | (0x7 << 8));
		reg_value |= (sample_cnt_div & 0x7) << 12 | (step_cnt_div & 0x7) << 8;
		writew(reg_value, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_HS));
		I2C_SET_HS_MODE(i2c_base, 1);
	} else {
		reg_value = readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_TIMING)) &
			    ~((0x7 << 8) | (0x3f << 0));
		reg_value |= (sample_cnt_div & 0x7) << 8 | (step_cnt_div & 0x3f) << 0;
		writew(reg_value, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_TIMING));
		I2C_SET_HS_MODE(i2c_base, 0);
	}

	return ret_code;
}

static inline void mtk_i2c_dump_info(struct mtk_i2c_t *i2c)
{
	uint32_t i2c_base = i2c->base;

	printf("I2C register:\nSLAVE_ADDR %x\nINTR_MASK %x\nINTR_STAT %x\n"
	       "CONTROL %x\nTRANSFER_LEN %x\nTRANSAC_LEN %x\nDELAY_LEN %x\n"
	       "TIMING %x\nSTART %x\nFIFO_STAT %x\nIO_CONFIG %x\nHS %x\n"
	       "DEBUGSTAT %x\nEXT_CONF %x\nPATH_DIR %x\n",
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_SLAVE_ADDR))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_INTR_MASK))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_INTR_STAT))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_CONTROL))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_TRANSFER_LEN))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_TRANSAC_LEN))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_DELAY_LEN))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_TIMING))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_START))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_FIFO_STAT))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_IO_CONFIG))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_HS))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_DEBUGSTAT))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_EXT_CONF))),
		(readw((volatile void *)(uintptr_t)(i2c_base + MTK_I2C_PATH_DIR))));

	printf("base address %x\n", (uint32_t)i2c_base);
}

uint32_t mtk_i2c_read(struct mtk_i2c_t *i2c, uint8_t *buffer,
                      uint32_t len)
{
	uint32_t ret_code = I2C_OK;
	uint32_t i2c_clk;
	uint8_t *ptr = buffer;
	uint16_t status;
	uint32_t time_out_val = 0;
	uint32_t count = 0;
	uint32_t temp_len = 0;
	uint32_t total_count = 0;
	uint8_t tpm_data_fifo_v = 0x5;
	uint32_t i2c_base = i2c->base;

	if ((len == 0) || (len > 255)) {
		printf("[i2c_read] I2C doesn't support len = %d.\n", len);
		return I2C_READ_FAIL_ZERO_LENGTH;
	}

	if (i2c->dir == 0) {
		I2C_PATH_NORMAL(i2c_base);
		i2c_clk = I2C_CLK_RATE;
	} else {
		I2C_PATH_PMIC(i2c_base);
		i2c_clk = I2C_CLK_RATE_PMIC;
	}

	count = len / 8;
	if (len % 8 != 0)
		count += 1;

	if (count > 1)
		total_count = count;

	while (count) {
		/*
		 * I2C FIFO SIZE is 8 bytes, so if i2c transfer length > 8,
		 * we will separate the transfer length, and after transfer 8 bytes one time.
		 * we should write 0x5 to the tpm according to the tpm spec.
		 * TODO: DMA is supported, we will remove this part.
		 */
		if (count < total_count && count > 0 && i2c->id == TPM_I2C_BUS_NUM)
			mtk_i2c_write(i2c, &tpm_data_fifo_v, 1);

		if (count == 1) {
			temp_len = len % 8;
			if (temp_len == 0)
				temp_len = 8;
			I2C_SET_TRANS_LEN(i2c_base, temp_len);
		} else {
			I2C_SET_TRANS_LEN(i2c_base, 8);
			temp_len = 8;
		}

		I2C_CLR_INTR_STATUS(i2c_base, I2C_TRANSAC_COMP | I2C_ACKERR
				    | I2C_HS_NACKERR);
		/* setting speed */
		mtk_i2c_set_speed(i2c, i2c_clk);
		if (i2c->speed <= 100)
			/* 0x8001 is for setting the i2c start and stop bit extension time. */
			writew(0x8001, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_EXT_CONF));

		/* control registers */
		I2C_SET_SLAVE_ADDR(i2c_base, ((i2c->addr << 1) | 0x1));

		I2C_SET_TRANSAC_LEN(i2c_base, 1);
		I2C_SET_INTR_MASK(i2c_base, I2C_HS_NACKERR | I2C_ACKERR
				  | I2C_TRANSAC_COMP);
		I2C_FIFO_CLR_ADDR(i2c_base);
		I2C_SET_TRANS_CTRL(i2c_base, ACK_ERR_DET_EN
				   | (i2c->is_clk_ext_disable ?  0 : CLK_EXT)
				   | (i2c->is_rs_enable
				      ? REPEATED_START_FLAG : STOP_FLAG));

		/* start trnasfer transaction */
		I2C_START_TRANSAC(i2c_base);

		/* polling mode : see if transaction complete */
		while (1) {
			status = I2C_INTR_STATUS(i2c_base);

			if (status & I2C_TRANSAC_COMP &&
			    (!I2C_FIFO_IS_EMPTY(i2c_base))) {
				ret_code = I2C_OK;
				/*
				 * TODO: remove it after DMA is supported and verified.
				 * we don't need this delay for TPM guard time.
				 */
				udelay(60);
				break;
			} else if (status & I2C_HS_NACKERR) {
				ret_code = I2C_READ_FAIL_HS_NACKERR;
				printf("[i2c%d read] transaction NACK error (%x)\n",
				       i2c->id, status);
				mtk_i2c_dump_info(i2c);
				break;
			} else if (status & I2C_ACKERR) {
				ret_code = I2C_READ_FAIL_ACKERR;
				printf("[i2c%d read] transaction ACK error (%x)\n",
				       i2c->id, status);
				mtk_i2c_dump_info(i2c);
				break;
			} else if (time_out_val > 100000) {
				ret_code = I2C_READ_FAIL_TIMEOUT;
				I2C_SOFTRESET(i2c_base);
				printf("[i2c%d read] transaction timeout:%d\n",
				       i2c->id, time_out_val);
				mtk_i2c_dump_info(i2c);
				break;
			}
			time_out_val++;
		}

		I2C_CLR_INTR_STATUS(i2c_base, I2C_TRANSAC_COMP | I2C_ACKERR
				    | I2C_HS_NACKERR);

		if (ret_code == I2C_OK) {
			while (temp_len-- > 0) {
				I2C_READ_BYTE(i2c_base, ptr++);
			}
		}

		/* clear bit mask */
		I2C_CLR_INTR_MASK(i2c_base, I2C_HS_NACKERR | I2C_ACKERR
				  | I2C_TRANSAC_COMP);

		I2C_SOFTRESET(i2c_base);
		count--;
	}

	return ret_code;
}

uint32_t mtk_i2c_write(struct mtk_i2c_t *i2c, uint8_t *buffer, uint32_t len)
{
	uint32_t ret_code = I2C_OK;
	uint32_t i2c_clk;
	uint8_t *ptr = buffer;
	uint16_t status;
	uint32_t time_out_val = 0;

	uint32_t count = 0;
	uint32_t temp_len = 0;
	uint32_t total_count = 0;
	uint8_t tpm_data_fifo_v = 0x5;
	uint32_t i2c_base = i2c->base;


	/* mt65xx doesn't support len = 0. */
	if ((len == 0) || (len > 255)) {
		printf("[i2c_write] I2C doesn't support len = %d.\n", len);
		return I2C_WRITE_FAIL_ZERO_LENGTH;
	}

	/* setting path direction and clock first all */
	if (i2c->dir == 0) {
		I2C_PATH_NORMAL(i2c_base);
		i2c_clk = I2C_CLK_RATE;
	} else {
		I2C_PATH_PMIC(i2c_base);
		i2c_clk = I2C_CLK_RATE_PMIC;
	}

	count = len / 8;
	if (len % 8 != 0)
		count += 1;

	if (count > 1)
		total_count = count;

	while (count) {
		/*
		 * I2C FIFO SIZE is 8 bytes, so if i2c transfer length > 8,
		 * we will separate the transfer length, and after transfer 8 bytes one time.
		 * we should write 0x5 to the tpm according to the tpm spec.
		 * TODO: DMA is supported, we will remove this part.
		 */
		if (count < total_count && count > 0 && i2c->id == TPM_I2C_BUS_NUM)
			mtk_i2c_write(i2c, &tpm_data_fifo_v, 1);

		if (count == 1) {
			temp_len = len % 8;
			if (temp_len == 0)
				temp_len = 8;
			I2C_SET_TRANS_LEN(i2c_base, temp_len);
		} else {
			I2C_SET_TRANS_LEN(i2c_base, 8);
			temp_len = 8;
		}


		I2C_CLR_INTR_STATUS(i2c_base, I2C_TRANSAC_COMP | I2C_ACKERR
				    | I2C_HS_NACKERR);
		/* setting speed */
		mtk_i2c_set_speed(i2c, i2c_clk);
		if (i2c->speed <= 100)
			/* 0x8001 is for setting the i2c start and stop bit extension time. */
			writew(0x8001, (volatile void *)(uintptr_t)(i2c_base + MTK_I2C_EXT_CONF));

		/* control registers */
		I2C_SET_SLAVE_ADDR(i2c_base, i2c->addr << 1);

		I2C_SET_TRANSAC_LEN(i2c_base, 1);
		I2C_SET_INTR_MASK(i2c_base, I2C_HS_NACKERR | I2C_ACKERR
				  | I2C_TRANSAC_COMP);

		I2C_FIFO_CLR_ADDR(i2c_base);
		I2C_SET_TRANS_CTRL(i2c_base, ACK_ERR_DET_EN |
		                   (i2c->is_clk_ext_disable ? 0 : CLK_EXT)
				   | (i2c->is_rs_enable ? REPEATED_START_FLAG : STOP_FLAG));

		/* start to write data */
		while (temp_len-- > 0) {
			I2C_WRITE_BYTE(i2c_base, *(ptr++));
		}

		/* start trnasfer transaction */
		I2C_START_TRANSAC(i2c_base);

		/* polling mode : see if transaction complete */
		while (1) {
			status = I2C_INTR_STATUS(i2c_base);
			if (status & I2C_HS_NACKERR) {
				ret_code = I2C_WRITE_FAIL_HS_NACKERR;
				printf("[i2c%d write] transaction NACK error\n",
				       i2c->id);
				mtk_i2c_dump_info(i2c);
				break;
			} else if (status & I2C_ACKERR) {
				ret_code = I2C_WRITE_FAIL_ACKERR;
				printf("[i2c%d write] transaction ACK error\n",
				       i2c->id);
				mtk_i2c_dump_info(i2c);
				break;
			} else if (status & I2C_TRANSAC_COMP) {
				/*
				 * TODO: remove it after DMA is supported and verified.
				 * we don't need this delay for TPM guard time.
				 */
				udelay(60);
				ret_code = I2C_OK;
				break;
			} else if (time_out_val > 100000) {
				ret_code = I2C_WRITE_FAIL_TIMEOUT;
				printf("[i2c%d write] transaction timeout:%d\n",
				       i2c->id, time_out_val);
				mtk_i2c_dump_info(i2c);
			} else if (status & I2C_TRANSAC_COMP) {
				ret_code = I2C_OK;
				break;
			} else if (time_out_val > 100000) {
				ret_code = I2C_WRITE_FAIL_TIMEOUT;
				printf("[i2c%d write] transaction timeout:%d\n",
				       i2c->id, time_out_val);
				mtk_i2c_dump_info(i2c);
				break;
			}
			time_out_val++;
		}

		I2C_CLR_INTR_STATUS(i2c_base, I2C_TRANSAC_COMP | I2C_ACKERR
				    | I2C_HS_NACKERR);

		/* clear bit mask */
		I2C_CLR_INTR_MASK(i2c_base, I2C_HS_NACKERR | I2C_ACKERR
				  | I2C_TRANSAC_COMP);
		I2C_SOFTRESET(i2c_base);

		count--;
	}
	return ret_code;
}

static int i2c_transfer(I2cOps *me, I2cSeg *segments, int seg_count)
{
	I2cSeg *seg = segments;
	MTKI2c *bus = container_of(me, MTKI2c, ops);
	struct mtk_i2c_t *i2c = bus->i2c;
	int ret = 0;

	for (int i = 0; i < seg_count; i++) {
		if (seg[i].read == 1)
			ret = mtk_i2c_read(i2c, seg[i].buf,
					   seg[i].len);
		else if (seg[i].read == 0)
			ret = mtk_i2c_write(i2c, seg[i].buf,
					    seg[i].len);
		if (ret)
			break;
	}

	return ret;
}

MTKI2c *new_mtk_i2c(struct mtk_i2c_t *i2c)
{
	MTKI2c *bus = xzalloc(sizeof(*bus));

	bus->ops.transfer = &i2c_transfer;
	bus->i2c = i2c;

	return bus;
}
