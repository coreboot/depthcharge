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

#include <arch/io.h>
#include <libpayload.h>
#include "drivers/bus/spi/ipq40xx.h"
#include "ipq40xx_qup.h"
#include "ipq40xx_blsp.h"

#define TIMEOUT_CNT	100

#define QUP_ADDR(id, reg)	(blsp_qup_base(id) + (reg))

#define QUP_DEBUG	0

#define QUPDBG		"\t-> "

#if QUP_DEBUG
#define qup_write32(a, v) do {				\
	write32(a, v);					\
	printf(QUPDBG "%s(%d): write32(0x%p, 0x%x)\n",	\
			__func__, __LINE__, a, v);	\
} while (0)
#else
#define qup_write32	write32
#endif

static qup_return_t qup_i2c_master_status(blsp_qup_id_t id)
{
	uint32_t reg_val = read32(QUP_ADDR(id, QUP_I2C_MASTER_STATUS));

	if (read32(QUP_ADDR(id, QUP_ERROR_FLAGS)))
		return QUP_ERR_XFER_FAIL;

#if QUP_DEBUG
	printf(QUPDBG "%s: 0x%x\n", __func__, reg_val);
#endif

	if (reg_val & QUP_I2C_INVALID_READ_ADDR)
		return QUP_ERR_I2C_INVALID_SLAVE_ADDR;
	if (reg_val & QUP_I2C_FAILED_MASK)
		return QUP_ERR_I2C_FAILED;
	if (reg_val & QUP_I2C_ARB_LOST)
		return QUP_ERR_I2C_ARB_LOST;
	if (reg_val & QUP_I2C_BUS_ERROR)
		return QUP_ERR_I2C_BUS_ERROR;
	if (reg_val & QUP_I2C_INVALID_WRITE)
		return QUP_ERR_I2C_INVALID_WRITE;
	if (reg_val & QUP_I2C_PACKET_NACK)
		return QUP_ERR_I2C_NACK;
	if (reg_val & QUP_I2C_INVALID_TAG)
		return QUP_ERR_I2C_INVALID_TAG;

	return QUP_SUCCESS;
}

static int check_bit_state(uint32_t *reg, int wait_for)
{
	unsigned int count = TIMEOUT_CNT;

	while ((read32(reg) &
		(BLSP_QUP_STATE_VALID_MASK | BLSP_QUP_STATE_MASK)) !=
			(BLSP_QUP_STATE_VALID | wait_for)) {
		if (count == 0)
			return QUP_ERR_TIMEOUT;
		count--;
	}

	return QUP_SUCCESS;
}

/*
 * Check whether GSBIn_QUP State is valid
 */
static qup_return_t qup_wait_for_state(blsp_qup_id_t id, unsigned wait_for)
{
	return check_bit_state(QUP_ADDR(id, QUP_STATE), wait_for);
}

qup_return_t qup_reset_i2c_master_status(blsp_qup_id_t id)
{
	/*
	 * The I2C_STATUS is a status register.
	 * Writing any value clears the status bits.
	 */
	qup_write32(QUP_ADDR(id, QUP_I2C_MASTER_STATUS), 0);
	return QUP_SUCCESS;
}

static qup_return_t qup_reset_master_status(blsp_qup_id_t id)
{
	qup_write32(QUP_ADDR(id, QUP_ERROR_FLAGS), 0x3C);
	qup_write32(QUP_ADDR(id, QUP_ERROR_FLAGS_EN), 0x3C);
	qup_reset_i2c_master_status(id);
	return QUP_SUCCESS;
}

static qup_return_t qup_fifo_wait_for(blsp_qup_id_t id, uint32_t status)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;
	unsigned int count = TIMEOUT_CNT;

	while (!(read32(QUP_ADDR(id, QUP_OPERATIONAL)) & status)) {
		ret = qup_i2c_master_status(id);
		if (ret)
			return ret;
		if (count == 0)
			return QUP_ERR_TIMEOUT;
		count--;
	}

	return QUP_SUCCESS;
}

static qup_return_t qup_fifo_wait_while(blsp_qup_id_t id, uint32_t status)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;
	unsigned int count = TIMEOUT_CNT;

	while (read32(QUP_ADDR(id, QUP_OPERATIONAL)) & status) {
		ret = qup_i2c_master_status(id);
		if (ret)
			return ret;
		if (count == 0)
			return QUP_ERR_TIMEOUT;
		count--;
	}

	return QUP_SUCCESS;
}

static inline uint32_t qup_i2c_create_tag(int stop, u8 data)
{
	uint32_t tag;

	if (stop)
		tag = QUP_I2C_STOP_SEQ | QUP_I2C_DATA(data);
	else
		tag = QUP_I2C_DATA_SEQ | QUP_I2C_DATA(data);

	return tag;
}

static inline qup_return_t qup_i2c_write_fifo_flush(blsp_qup_id_t id)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;

	qup_write32(QUP_ADDR(id, QUP_OPERATIONAL), OUTPUT_SERVICE_FLAG);

	mdelay(3);	/* TPM seems to need this */

	ret = qup_fifo_wait_while(id, OUTPUT_FIFO_NOT_EMPTY);
	if (ret)
		return ret;

	ret = qup_i2c_master_status(id);

	if (ret)
		printf(QUPDBG "%s: error 0x%x\n", __func__, ret);

	return ret;
}

static qup_return_t qup_i2c_write_fifo(blsp_qup_id_t id, qup_data_t *p_tx_obj,
				       uint8_t stop_seq)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;
		uint8_t addr = p_tx_obj->p.iic.addr;
		uint8_t *data_ptr = p_tx_obj->p.iic.data;
		unsigned data_len = p_tx_obj->p.iic.data_len;
		unsigned idx = 0;
	uint32_t tag, *fifo = QUP_ADDR(id, QUP_OUTPUT_FIFO);

	qup_reset_master_status(id);

	qup_write32(QUP_ADDR(id, QUP_MX_OUTPUT_COUNT), data_len + 1);

	qup_set_state(id, BLSP_QUP_STATE_RUN);

	/*
	 * Since UNPACK enable is set in io mode register, populate 2 tags
	 * for each fifo register.
	 *
	 * Create the first tag as follows, with the start tag and first byte
	 * of the data to be written
	 *	+--------+--------+--------+--------+
	 *	| STOP / |  data  | START  | ADDR   |
	 *	|DATA tag|  byte  |  tag   | << 1   |
	 *	+--------+--------+--------+--------+
	 * rest will be created in the following while loop.
	 */
	tag = qup_i2c_create_tag(data_len == 1 && stop_seq, data_ptr[idx]);
	tag = ((tag << 16) & 0xffff0000) |
			(QUP_I2C_START_SEQ | QUP_I2C_ADDR(addr));
	data_len--;
	idx++;

	qup_write32(fifo, tag);

	while (data_len) {

		tag = qup_i2c_create_tag(data_len == 1 && stop_seq,
				data_ptr[idx]);
		data_len--;
		idx++;

		if (data_len) {
			tag |= qup_i2c_create_tag(data_len == 1 && stop_seq,
					data_ptr[idx]) << 16;
			data_len--;
			idx++;
		}

		qup_write32(fifo, tag);

		ret = qup_i2c_write_fifo_flush(id);

		if (ret) {
			printf(QUPDBG "%s: error\n", __func__);
			return ret;
		}
	}

	ret = qup_i2c_write_fifo_flush(id);

	qup_set_state(id, BLSP_QUP_STATE_RESET);

	return ret;
}

static qup_return_t qup_i2c_write(blsp_qup_id_t id, uint8_t mode,
				  qup_data_t *p_tx_obj, uint8_t stop_seq)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;

	switch (mode) {
	case QUP_MODE_FIFO:
	case QUP_MODE_BLOCK:
		ret = qup_i2c_write_fifo(id, p_tx_obj, stop_seq);
		break;
	default:
		ret = QUP_ERR_UNSUPPORTED;
	}

	if (ret) {
		qup_set_state(id, BLSP_QUP_STATE_RESET);
		printf(QUPDBG "%s() failed (%d)\n", __func__, ret);
	}

	return ret;
}

static int qup_i2c_parse_tag(uint32_t data, uint8_t *data_ptr, uint32_t len)
{
	int i, idx = 0;
	int max = (len > 2) ? 2 : len;

	for (i = 0; i < max; i++) {
		switch (QUP_I2C_MI_TAG(data)) {
		case QUP_I2C_MIDATA_SEQ:
			data_ptr[idx] = QUP_I2C_DATA(data);
			idx++;
			break;
		case QUP_I2C_MISTOP_SEQ:
			data_ptr[idx] = QUP_I2C_DATA(data);
			idx++;
			return idx;
		default:
			return -1;
		}

		data = (data >> 16);
	}

	return idx;
}

static qup_return_t qup_i2c_read_fifo(blsp_qup_id_t id, qup_data_t *p_tx_obj)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;
	uint8_t addr = p_tx_obj->p.iic.addr;
	uint8_t *data_ptr = p_tx_obj->p.iic.data;
	unsigned data_len = p_tx_obj->p.iic.data_len;
	unsigned idx = 0;
	uint32_t *fifo = QUP_ADDR(id, QUP_OUTPUT_FIFO);

	qup_reset_master_status(id);

	qup_write32(QUP_ADDR(id, QUP_IO_MODES),
		QUP_UNPACK_EN | QUP_PACK_EN |
			  ((QUP_MODE_BLOCK & QUP_MODE_MASK) <<
					QUP_OUTPUT_MODE_SHFT) |
			  ((QUP_MODE_BLOCK & QUP_MODE_MASK) <<
					QUP_INPUT_MODE_SHFT));

	qup_write32(QUP_ADDR(id, QUP_MX_INPUT_COUNT), data_len);

	qup_set_state(id, BLSP_QUP_STATE_RUN);

	qup_write32(fifo, (QUP_I2C_START_SEQ |
				  (QUP_I2C_ADDR(addr) | QUP_I2C_SLAVE_READ)) |
				((QUP_I2C_RECV_SEQ | data_len) << 16));

	ret = qup_i2c_write_fifo_flush(id);
	if (ret) {
		printf(QUPDBG "%s: OUTPUT_FIFO_NOT_EMPTY\n", __func__);
		return ret;
	}

	ret = qup_fifo_wait_for(id, INPUT_SERVICE_FLAG);
	if (ret) {
		printf(QUPDBG "%s: INPUT_SERVICE_FLAG\n", __func__);
		return ret;
	}

	fifo = QUP_ADDR(id, QUP_INPUT_FIFO);

	while (data_len) {
		uint32_t data;
		int count;

		data = read32(fifo);
		mdelay(1);

		count = qup_i2c_parse_tag(data, data_ptr + idx, data_len);

		if (count < 0) {
			printf(QUPDBG "%s: Cannot parse tag 0x%x\n",
					__func__, data);
			qup_set_state(id, BLSP_QUP_STATE_PAUSE);

			return QUP_ERR_I2C_INVALID_TAG;
		}

		idx += count;
		data_len -= count;

		qup_write32(QUP_ADDR(id, QUP_OPERATIONAL), INPUT_SERVICE_FLAG);
	}

	p_tx_obj->p.iic.data_len = idx;

	qup_write32(QUP_ADDR(id, QUP_MX_READ_COUNT), 0);

	qup_set_state(id, BLSP_QUP_STATE_RESET);

	return QUP_SUCCESS;
}

static qup_return_t qup_i2c_read(blsp_qup_id_t id, uint8_t mode,
				 qup_data_t *p_tx_obj)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;

	qup_set_state(id, BLSP_QUP_STATE_RESET);

	switch (mode) {
	case QUP_MODE_FIFO:
	case QUP_MODE_BLOCK:
		ret = qup_i2c_read_fifo(id, p_tx_obj);
		break;
	default:
		ret = QUP_ERR_UNSUPPORTED;
	}

	if (ret) {
		qup_set_state(id, BLSP_QUP_STATE_RESET);
		printf(QUPDBG "%s() failed (%d)\n", __func__, ret);
	}

	return ret;
}

qup_return_t qup_init(blsp_qup_id_t id, const qup_config_t *config_ptr)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;
	uint32_t reg_val, mstr_cfg = 0;

	/* Reset the QUP core.*/
	qup_write32(QUP_ADDR(id, QUP_SW_RESET), 0x1);

	/*Wait till the reset takes effect */
	ret = qup_wait_for_state(id, BLSP_QUP_STATE_RESET);
	if (ret)
		goto bailout;

	/* Reset the config */
	qup_write32(QUP_ADDR(id, QUP_CONFIG), 0);

	/*Program the config register*/
	/*Set N value*/
	reg_val = 0x0F;
	/*Set protocol*/
	switch (config_ptr->protocol) {
	case QUP_MINICORE_I2C_MASTER:
		reg_val |= ((config_ptr->protocol &
				QUP_MINI_CORE_PROTO_MASK) <<
				QUP_MINI_CORE_PROTO_SHFT);
		break;
	default:
		ret = QUP_ERR_UNSUPPORTED;
		goto bailout;
	}
	reg_val |= QUP_APP_CLK_ON_EN | QUP_CORE_CLK_ON_EN;
	qup_write32(QUP_ADDR(id, QUP_CONFIG), reg_val);

	/* Choose version 1 tag */
	qup_write32(QUP_ADDR(id, QUP_I2C_MASTER_CONFIG), mstr_cfg);

	/* Reset i2c clk cntl register */
	qup_write32(QUP_ADDR(id, QUP_I2C_MASTER_CLK_CTL), 0);

	/* Set QUP IO Mode */
	switch (config_ptr->mode) {
	case QUP_MODE_FIFO:
	case QUP_MODE_BLOCK:
		reg_val = QUP_UNPACK_EN | QUP_PACK_EN |
			  ((config_ptr->mode & QUP_MODE_MASK) <<
					QUP_OUTPUT_MODE_SHFT) |
			  ((config_ptr->mode & QUP_MODE_MASK) <<
					QUP_INPUT_MODE_SHFT);
		break;
	default:
		ret = QUP_ERR_UNSUPPORTED;
		goto bailout;
	}
	qup_write32(QUP_ADDR(id, QUP_IO_MODES), reg_val);

	/*Set i2c clk cntl*/
	if (config_ptr->ht_div && config_ptr->fs_div) {
		reg_val = ((config_ptr->ht_div & QUP_HT_DIVIDER_MASK) <<
				QUP_HT_DIVIDER_SHFT) |
				(config_ptr->fs_div & QUP_FS_DIVIDER_MASK);
	} else {
		reg_val = (QUP_DIVIDER_MIN_VAL << QUP_HS_DIVIDER_SHFT);
		reg_val |= ((((config_ptr->src_frequency /
				config_ptr->clk_frequency) / 2) -
				QUP_DIVIDER_MIN_VAL) & QUP_FS_DIVIDER_MASK);
	}

	if (config_ptr->noise_rej_sda) {
		reg_val = I2C_SDA_NOISE_REJECTION(reg_val,
						  config_ptr->noise_rej_sda);
		qup_write32(QUP_ADDR(id, QUP_I2C_MASTER_CONFIG),
				     mstr_cfg | QUP_CLK_NOISE_REJECT_ENABLE);
	}

	if (config_ptr->noise_rej_scl) {
		reg_val = I2C_SCL_NOISE_REJECTION(reg_val,
						  config_ptr->noise_rej_scl);
		qup_write32(QUP_ADDR(id, QUP_I2C_MASTER_CONFIG),
				     mstr_cfg | QUP_CLK_NOISE_REJECT_ENABLE);
	}

	qup_write32(QUP_ADDR(id, QUP_I2C_MASTER_CLK_CTL), reg_val);

	qup_set_state(id, BLSP_QUP_STATE_RESET);
 bailout:
	if (ret)
		printf(QUPDBG "failed to init qup (%d)\n", ret);

	return ret;
}

qup_return_t qup_set_state(blsp_qup_id_t id, uint32_t state)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;
	unsigned curr_state = read32(QUP_ADDR(id, QUP_STATE));

	if ((state >= BLSP_QUP_STATE_RESET && state <= BLSP_QUP_STATE_PAUSE)
		&& (curr_state & BLSP_QUP_STATE_VALID_MASK)) {
		/*
		* For PAUSE_STATE to RESET_STATE transition,
		* two writes of  10[binary]) are required for the
		* transition to complete.
		*/
		if (BLSP_QUP_STATE_PAUSE == curr_state &&
					BLSP_QUP_STATE_RESET == state) {
			qup_write32(QUP_ADDR(id, QUP_STATE), 0x2);
			qup_write32(QUP_ADDR(id, QUP_STATE), 0x2);
		} else {
			qup_write32(QUP_ADDR(id, QUP_STATE), state);
		}
		ret = qup_wait_for_state(id, state);
	}

	return ret;
}

static qup_return_t qup_i2c_send_data(blsp_qup_id_t id, qup_data_t *p_tx_obj,
				      uint8_t stop_seq)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;
	uint8_t mode = (read32(QUP_ADDR(id, QUP_IO_MODES)) >>
			QUP_OUTPUT_MODE_SHFT) & QUP_MODE_MASK;

	ret = qup_i2c_write(id, mode, p_tx_obj, stop_seq);
	if (0) {
		int i;

		printf("i2c tx bus %d device %2.2x:", id, p_tx_obj->p.iic.addr);
		for (i = 0; i < p_tx_obj->p.iic.data_len; i++)
			printf(" %2.2x", p_tx_obj->p.iic.data[i]);
		printf("\n");
	}

	return ret;
}

qup_return_t qup_send_data(blsp_qup_id_t id, qup_data_t *p_tx_obj,
			   uint8_t stop_seq)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;

	if (p_tx_obj->protocol == ((read32(QUP_ADDR(id, QUP_CONFIG)) >>
			QUP_MINI_CORE_PROTO_SHFT) & QUP_MINI_CORE_PROTO_MASK)) {
		switch (p_tx_obj->protocol) {
		case QUP_MINICORE_I2C_MASTER:
			ret = qup_i2c_send_data(id, p_tx_obj, stop_seq);
			break;
		default:
			ret = QUP_ERR_UNSUPPORTED;
		}
	}

	return ret;
}

static qup_return_t qup_i2c_recv_data(blsp_qup_id_t id, qup_data_t *p_rx_obj)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;
	uint8_t mode = (read32(QUP_ADDR(id, QUP_IO_MODES)) >>
			QUP_INPUT_MODE_SHFT) & QUP_MODE_MASK;

	ret = qup_i2c_read(id, mode, p_rx_obj);
	if (0) {
		int i;

		printf("i2c rxed on bus %d device %2.2x:",
			id, p_rx_obj->p.iic.addr);
		for (i = 0; i < p_rx_obj->p.iic.data_len; i++)
			printf(" %2.2x", p_rx_obj->p.iic.data[i]);
		printf("\n");
	}

	return ret;
}

qup_return_t qup_recv_data(blsp_qup_id_t id, qup_data_t *p_rx_obj)
{
	qup_return_t ret = QUP_ERR_UNDEFINED;

	if (p_rx_obj->protocol == ((read32(QUP_ADDR(id, QUP_CONFIG)) >>
			QUP_MINI_CORE_PROTO_SHFT) & QUP_MINI_CORE_PROTO_MASK)) {
		switch (p_rx_obj->protocol) {
		case QUP_MINICORE_I2C_MASTER:
			ret = qup_i2c_recv_data(id, p_rx_obj);
			break;
		default:
			ret = QUP_ERR_UNSUPPORTED;
		}
	}

	return ret;
}
