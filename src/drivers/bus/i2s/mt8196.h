/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DRIVERS_BUS_I2S_MT8196_H__
#define __DRIVERS_BUS_I2S_MT8196_H__

#define SRAM_BASE	0x1A119000
#define SRAM_SIZE	0x10000

/* AFE */
typedef struct afe_reg {
	uint32_t audio_top_con0;
	uint32_t audio_top_con1;
	uint32_t audio_top_con2;
	uint32_t audio_top_con3;
	uint32_t audio_top_con4;
	uint32_t audio_engen_con0;
	uint32_t _rsv0[6];
	uint32_t afe_apll1_tuner_cfg;
	uint32_t _rsv1;
	uint32_t afe_apll2_tuner_cfg;
	uint32_t _rsv2[9];
	uint32_t afe_spm_control_req;
	uint32_t _rsv3[13];
	uint32_t aud_top_cfg_vlp_rg;
	uint32_t _rsv4[1225];
	struct {
		uint32_t con[10];
		uint32_t mon;
	} etdm_in[1];
	uint32_t _rsv5[101];
	struct {
		uint32_t con[10];
		uint32_t mon;
	} etdm_out[1];
	uint32_t _rsv6[57];
	uint32_t etdm_4_7_cowork_con0;
	uint32_t _rsv7[1564];
	uint32_t afe_conn116_1;
	uint32_t _rsv8[7];
	uint32_t afe_conn117_1;
	uint32_t _rsv9[1470];
	uint32_t afe_dl_24ch_base_msb;
	uint32_t afe_dl_24ch_base;
	uint32_t afe_dl_24ch_cur_msb;
	uint32_t afe_dl_24ch_cur;
	uint32_t afe_dl_24ch_end_msb;
	uint32_t afe_dl_24ch_end;
	uint32_t _rsv10[2];
	uint32_t afe_dl_24ch_con0;
	uint32_t _rsv11;
	uint32_t afe_dl_24ch_mon0;
} __attribute__((packed)) MtkI2sRegs;

check_member(afe_reg, etdm_in[0].con[0], 0x13c0);
check_member(afe_reg, etdm_out[0].con[0], 0x1580);
check_member(afe_reg, etdm_4_7_cowork_con0, 0x1690);
check_member(afe_reg, afe_conn116_1, 0x2f04);
check_member(afe_reg, afe_conn117_1, 0x2f24);
check_member(afe_reg, afe_dl_24ch_base_msb, 0x4620);
check_member(afe_reg, afe_dl_24ch_base, 0x4624);
check_member(afe_reg, afe_dl_24ch_cur_msb, 0x4628);
check_member(afe_reg, afe_dl_24ch_cur, 0x462c);
check_member(afe_reg, afe_dl_24ch_end_msb, 0x4630);
check_member(afe_reg, afe_dl_24ch_end, 0x4634);
check_member(afe_reg, afe_dl_24ch_con0, 0x4640);
check_member(afe_reg, afe_dl_24ch_mon0, 0x4648);

/* ETDM_4_7_COWORK_CON0 */
#define ETDM_OUT4_SLAVE_SEL_SFT			8
#define ETDM_OUT4_SLAVE_SEL_MASK		0xf
#define ETDM_OUT4_SLAVE_SEL_MASK_SFT		(0xf << 8)
#define ETDM_OUT4_SLAVE_SEL_VALUE		0x0

#define ETDM_IN4_SLAVE_SEL_SFT			24
#define ETDM_IN4_SLAVE_SEL_MASK			0xf
#define ETDM_IN4_SLAVE_SEL_MASK_SFT		(0xf << 24)
#define ETDM_IN4_SLAVE_SEL_VALUE		0x8

/* ETDM_IN0_CON0 */
#define REG_ETDM_IN_EN_SFT			0
#define REG_ETDM_IN_EN_MASK			0x1
#define REG_ETDM_IN_EN_MASK_SFT			(0x1 << 0)
#define REG_ETDM_IN_EN_VALUE			BIT(0)

#define REG_BIT_LENGTH_SFT			11
#define REG_BIT_LENGTH_MASK			0x1f
#define REG_BIT_LENGTH_MASK_SFT			(0x1f << 11)
#define REG_BIT_LENGTH_VALUE			0xf

#define REG_WORD_LENGTH_SFT			16
#define REG_WORD_LENGTH_MASK			0x1f
#define REG_WORD_LENGTH_MASK_SFT		(0x1f << 16)
#define REG_WORD_LENGTH_VALUE			0xf

/* ETDM_IN0_CON1 */
#define REG_INITIAL_POINT_SFT			5
#define REG_INITIAL_POINT_MASK			0x1f
#define REG_INITIAL_POINT_MASK_SFT		(0x1f << 5)
#define REG_INITIAL_POINT_VAL			(0x3)

/* ETDM_IN0_CON2 */
#define REG_CLOCK_SOURCE_SEL_SFT		10
#define REG_CLOCK_SOURCE_SEL_MASK		0x7
#define REG_CLOCK_SOURCE_SEL_MASK_SFT		(0x7 << 10)
#define REG_CLOCK_SOURCE_SEL_VALUE		(0x1)

/* ETDM_IN0_CON3 */
#define REG_FS_TIMING_SEL_SFT			26
#define REG_FS_TIMING_SEL_MASK			0x1f
#define REG_FS_TIMING_SEL_MASK_SFT		(0x1f << 26)
#define REG_FS_TIMING_SEL_VALUE			(0x5)

/* ETDM_IN0_CON4 */
#define REG_RELATCH_1X_EN_SEL_SFT		20
#define REG_RELATCH_1X_EN_SEL_MASK		0x1f
#define REG_RELATCH_1X_EN_SEL_MASK_SFT		(0x1f << 20)
#define REG_RELATCH_1X_EN_SEL_VALUE		0xa

/* ETDM_IN0_CON9 */
#define REG_OUT2LATCH_TIME_SFT			10
#define REG_OUT2LATCH_TIME_MASK			0x1f
#define REG_OUT2LATCH_TIME_MASK_SFT		(0x1f << 10)
#define REG_OUT2LATCH_TIME_VALUE		0x6

/* ETDM_OUT4_CON0*/
#define OUT_REG_ETDM_OUT_EN_SFT			0
#define OUT_REG_ETDM_OUT_EN_MASK		0x1
#define OUT_REG_ETDM_OUT_EN_MASK_SFT		(0x1 << 0)
#define OUT_REG_ETDM_OUT_EN_VALUE		(0x1)
#define OUT_REG_FMT_SFT				6
#define OUT_REG_FMT_MASK			0x7
#define OUT_REG_FMT_MASK_SFT			(0x7 << 6)
#define OUT_REG_FMT_VALUE			0x0

#define OUT_REG_BIT_LENGTH_SFT			11
#define OUT_REG_BIT_LENGTH_MASK			0x1f
#define OUT_REG_BIT_LENGTH_MASK_SFT		(0x1f << 11)
#define OUT_REG_BIT_LENGTH_VALUE		0xf

#define OUT_REG_WORD_LENGTH_SFT			16
#define OUT_REG_WORD_LENGTH_MASK		0x1f
#define OUT_REG_WORD_LENGTH_MASK_SFT		(0x1f << 16)
#define OUT_REG_WORD_LENGTH_VAL			0xf

#define OUT_REG_CH_NUM_SFT			23
#define OUT_REG_CH_NUM_MASK			0x1f
#define OUT_REG_CH_NUM_MASK_SFT			(0x1f << 23)
#define OUT_REG_CH_NUM_VALUE			0x1

#define OUT_REG_RELATCH_DOMAIN_SEL_SFT		28
#define OUT_REG_RELATCH_DOMAIN_SEL_MASK		0x7
#define OUT_REG_RELATCH_DOMAIN_SEL_MASK_SFT	(0x7 << 28)
#define OUT_REG_RELATCH_DOMAIN_SEL_VALUE	0x1

/* ETDM_OUT4_CON4*/
#define OUT_REG_FS_TIMING_SEL_SFT		0
#define OUT_REG_FS_TIMING_SEL_MASK		0x1f
#define OUT_REG_FS_TIMING_SEL_MASK_SFT		(0x1f << 0)
#define OUT_REG_FS_TIMING_SEL_VALUE		0x5

#define OUT_REG_CLOCK_SOURCE_SEL_SFT		6
#define OUT_REG_CLOCK_SOURCE_SEL_MASK		0x7
#define OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT	(0x7 << 6)
#define OUT_REG_CLOCK_SOURCE_SEL_VALUE		0x1

#define OUT_REG_RELATCH_EN_SEL_SFT		24
#define OUT_REG_RELATCH_EN_SEL_MASK		0x1f
#define OUT_REG_RELATCH_EN_SEL_MASK_SFT		(0x1f << 24)
#define OUT_REG_RELATCH_EN_SEL_VALUE		0xa

/* AFE_CONN116_1 */
#define I054_O116_S_SFT				22
#define I054_O116_S_MASK			0x1
#define I054_O116_S_MASK_SFT			(0x1 << 22)
#define I054_O116_S_VALUE			0x1

/* AFE_CONN117_1 */
#define I055_O117_S_SFT				23
#define I055_O117_S_MASK			0x1
#define I055_O117_S_MASK_SFT			(0x1 << 23)
#define I055_O117_S_VALUE			0x1

/* AFE_DL_24CH_BASE_MSB */
#define DL_24CH_BASE__ADDR_MSB_SFT		0
#define DL_24CH_BASE__ADDR_MSB_MASK		0x1ff
#define DL_24CH_BASE__ADDR_MSB_MASK_SFT		(0x1ff << 0)

/* AFE_DL_24CH_BASE */
#define DL_24CH_BASE_ADDR_SFT			4
#define DL_24CH_BASE_ADDR_MASK			0xfffffff
#define DL_24CH_BASE_ADDR_MASK_SFT		(0xfffffff << 4)

/* AFE_DL_24CH_CUR_MSB */
#define DL_24CH_CUR_PTR_MSB_SFT			0
#define DL_24CH_CUR_PTR_MSB_MASK		0x1ff
#define DL_24CH_CUR_PTR_MSB_MASK_SFT		(0x1ff << 0)

/* AFE_DL_24CH_CUR */
#define DL_24CH_CUR_PTR_SFT			0
#define DL_24CH_CUR_PTR_MASK			0xffffffff
#define DL_24CH_CUR_PTR_MASK_SFT		(0xffffffff << 0)

/* AFE_DL_24CH_END_MSB */
#define DL_24CH_END_ADDR_MSB_SFT		0
#define DL_24CH_END_ADDR_MSB_MASK		0x1ff
#define DL_24CH_END_ADDR_MSB_MASK_SFT		(0x1ff << 0)

/* AFE_DL_24CH_END */
#define DL_24CH_END_ADDR_SFT			4
#define DL_24CH_END_ADDR_MASK			0xfffffff
#define DL_24CH_END_ADDR_MASK_SFT		(0xfffffff << 4)

/* AFE_DL_24CH_CON0 */
#define DL_24CH_ON_SFT				31
#define DL_24CH_ON_MASK				0x1
#define DL_24CH_ON_MASK_SFT			(0x1 << 31)
#define DL_24CH_ON_VALUE			0x1

#define DL_24CH_NUM_SFT				24
#define DL_24CH_NUM_MASK			0x3f
#define DL_24CH_NUM_MASK_SFT			(0x3f << 24)
#define DL_24CH_NUM_VALUE			0x2

#define DL_24CH_MINLEN_SFT			20
#define DL_24CH_MINLEN_MASK			0x3
#define DL_24CH_MINLEN_MASK_SFT			(0x3 << 20)
#define DL_24CH_MINLEN_VALUE			0x0

#define DL_24CH_SEL_FS_SFT			8
#define DL_24CH_SEL_FS_MASK			0x1f
#define DL_24CH_SEL_FS_MASK_SFT			(0x1f << 8)
#define DL_24CH_SEL_FS_VALUE			0xa

#define DL_24CH_PBUF_SIZE_SFT			5
#define DL_24CH_PBUF_SIZE_MASK			0x3
#define DL_24CH_PBUF_SIZE_MASK_SFT		(0x3 << 5)
#define DL_24CH_PBUF_SIZE_VALUE			0x0

#endif /* __DRIVERS_BUS_I2S_MT8196_H__ */
