/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DRIVERS_BUS_I2S_MT8189_H__
#define __DRIVERS_BUS_I2S_MT8189_H__

#define SRAM_BASE	0x11060000
#define SRAM_SIZE	0x14000

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
	uint32_t _rsv4[1177];
	struct {
		uint32_t con[10];
		uint32_t mon;
		uint32_t _rsv;
	} etdm_in[6];
	uint32_t _rsv5[24];
	struct {
		uint32_t con[10];
		uint32_t mon;
		uint32_t _rsv[5];
	} etdm_out[6];
	uint32_t _rsv6[32];
	uint32_t etdm_0_3_cowork_con[4];
	uint32_t _rsv7[1501];
	uint32_t afe_conn108_1;
	uint32_t _rsv7_1[7];
	uint32_t afe_conn109_1;
	uint32_t _rsv7_2[7];
	uint32_t afe_conn110_1;
	uint32_t _rsv7_3[7];
	uint32_t afe_conn111_1;
	uint32_t _rsv8[1398];
	uint32_t afe_dl0_base_msb;
	uint32_t afe_dl0_base;
	uint32_t afe_dl0_cur_msb;
	uint32_t afe_dl0_cur;
	uint32_t afe_dl0_end_msb;
	uint32_t afe_dl0_end;
	uint32_t _rsv8_1[2];
	uint32_t afe_dl0_con0;
	uint32_t afe_dl0_mon0;
	uint32_t _rsv9[2];
	uint32_t afe_dl1_base_msb;
	uint32_t afe_dl1_base;
	uint32_t afe_dl1_cur_msb;
	uint32_t afe_dl1_cur;
	uint32_t afe_dl1_end_msb;
	uint32_t afe_dl1_end;
	uint32_t _rsv9_1[2];
	uint32_t afe_dl1_con0;
	uint32_t afe_dl1_mon0;
} __attribute__((packed)) MtkI2sRegs;

check_member(afe_reg, afe_apll1_tuner_cfg, 0x30);
check_member(afe_reg, aud_top_cfg_vlp_rg, 0x98);
check_member(afe_reg, etdm_in[0].con[0], 0x1300);
check_member(afe_reg, etdm_in[1].con[0], 0x1330);
check_member(afe_reg, etdm_out[0].con[0], 0x1480);
check_member(afe_reg, etdm_out[1].con[0], 0x14c0);
check_member(afe_reg, etdm_0_3_cowork_con[0], 0x1680);
check_member(afe_reg, afe_conn108_1, 0x2e04);
check_member(afe_reg, afe_conn111_1, 0x2e64);
check_member(afe_reg, afe_dl0_base_msb, 0x4440);
check_member(afe_reg, afe_dl0_base, 0x4444);
check_member(afe_reg, afe_dl0_cur_msb, 0x4448);
check_member(afe_reg, afe_dl0_cur, 0x444c);
check_member(afe_reg, afe_dl0_end_msb, 0x4450);
check_member(afe_reg, afe_dl0_end, 0x4454);
check_member(afe_reg, afe_dl0_con0, 0x4460);
check_member(afe_reg, afe_dl0_mon0, 0x4464);
check_member(afe_reg, afe_dl1_base_msb, 0x4470);
check_member(afe_reg, afe_dl1_base, 0x4474);
check_member(afe_reg, afe_dl1_cur_msb, 0x4478);
check_member(afe_reg, afe_dl1_cur, 0x447c);
check_member(afe_reg, afe_dl1_end_msb, 0x4480);
check_member(afe_reg, afe_dl1_end, 0x4484);
check_member(afe_reg, afe_dl1_con0, 0x4490);
check_member(afe_reg, afe_dl1_mon0, 0x4494);

/* ETDM_0_3_COWORK_CON */
#define ETDM_OUT_SLAVE_SEL_SFT			8
#define ETDM_OUT_SLAVE_SEL_MASK			0xf
#define ETDM_OUT_SLAVE_SEL_MASK_SFT		(0xf << 8)
#define ETDM_OUT_SLAVE_SEL_VALUE		0x0

#define ETDM_IN_SLAVE_SEL_SFT			24
#define ETDM_IN_SLAVE_SEL_MASK			0xf
#define ETDM_IN_SLAVE_SEL_MASK_SFT		(0xf << 24)
#define ETDM_IN_SLAVE_SEL_VALUE			0x8

/* ETDM_IN_CON0 */
#define REG_ETDM_IN_EN_SFT			0
#define REG_ETDM_IN_EN_MASK			0x1
#define REG_ETDM_IN_EN_MASK_SFT			(0x1 << 0)
#define REG_ETDM_IN_EN_VALUE			0x1

#define REG_BIT_LENGTH_SFT			11
#define REG_BIT_LENGTH_MASK			0x1f
#define REG_BIT_LENGTH_MASK_SFT			(0x1f << 11)
#define REG_BIT_LENGTH_VALUE			0xf

#define REG_WORD_LENGTH_SFT			16
#define REG_WORD_LENGTH_MASK			0x1f
#define REG_WORD_LENGTH_MASK_SFT		(0x1f << 16)
#define REG_WORD_LENGTH_VALUE			0xf

/* ETDM_IN_CON1 */
#define REG_INITIAL_POINT_SFT			5
#define REG_INITIAL_POINT_MASK			0x1f
#define REG_INITIAL_POINT_MASK_SFT		(0x1f << 5)
#define REG_INITIAL_POINT_VAL			0x3

/* ETDM_IN_CON2 */
#define REG_CLOCK_SOURCE_SEL_SFT		10
#define REG_CLOCK_SOURCE_SEL_MASK		0x7
#define REG_CLOCK_SOURCE_SEL_MASK_SFT		(0x7 << 10)
#define REG_CLOCK_SOURCE_SEL_VALUE		0x1

/* ETDM_IN_CON3 */
#define REG_FS_TIMING_SEL_SFT			26
#define REG_FS_TIMING_SEL_MASK			0x1f
#define REG_FS_TIMING_SEL_MASK_SFT		(0x1f << 26)
#define REG_FS_TIMING_SEL_VALUE			0x5

/* ETDM_IN_CON4 */
#define REG_RELATCH_1X_EN_SEL_SFT		20
#define REG_RELATCH_1X_EN_SEL_MASK		0x1f
#define REG_RELATCH_1X_EN_SEL_MASK_SFT		(0x1f << 20)
#define REG_RELATCH_1X_EN_SEL_VALUE		0xa

/* ETDM_IN_CON9 */
#define REG_OUT2LATCH_TIME_SFT			10
#define REG_OUT2LATCH_TIME_MASK			0x1f
#define REG_OUT2LATCH_TIME_MASK_SFT		(0x1f << 10)
#define REG_OUT2LATCH_TIME_VALUE		0x6

/* ETDM_OUT_CON0 */
#define OUT_REG_ETDM_OUT_EN_SFT			0
#define OUT_REG_ETDM_OUT_EN_MASK		0x1
#define OUT_REG_ETDM_OUT_EN_MASK_SFT		(0x1 << 0)
#define OUT_REG_ETDM_OUT_EN_VALUE		0x1
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

/* ETDM_OUT_CON4 */
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

/* AFE_CONN108_1 */
#define I034_O108_S_SFT				2
#define I034_O108_S_MASK			0x1
#define I034_O108_S_MASK_SFT			(0x1 << 2)
#define I034_O108_S_VALUE			0x1

/* AFE_CONN109_1 */
#define I035_O109_S_SFT				3
#define I035_O109_S_MASK			0x1
#define I035_O109_S_MASK_SFT			(0x1 << 3)
#define I035_O109_S_VALUE			0x1

/* AFE_CONN110_1 */
#define I032_O110_S_SFT				0
#define I032_O110_S_MASK			0x1
#define I032_O110_S_MASK_SFT			(0x1 << 0)
#define I032_O110_S_VALUE			0x1

/* AFE_CONN111_1 */
#define I033_O111_S_SFT				1
#define I033_O111_S_MASK			0x1
#define I033_O111_S_MASK_SFT			(0x1 << 1)
#define I033_O111_S_VALUE			0x1

/* AFE_DL_CON0 */
#define DL_ON_SFT				28
#define DL_ON_MASK				0x1
#define DL_ON_MASK_SFT				(0x1 << 28)
#define DL_ON_VALUE				0x1

#define DL_MINLEN_SFT				20
#define DL_MINLEN_MASK				0x3
#define DL_MINLEN_MASK_SFT			(0x3 << 20)
#define DL_MINLEN_VALUE				0x0

#define DL_SEL_FS_SFT				8
#define DL_SEL_FS_MASK				0x1f
#define DL_SEL_FS_MASK_SFT			(0x1f << 8)
#define DL_SEL_FS_VALUE				0xa

#define DL_PBUF_SIZE_SFT			5
#define DL_PBUF_SIZE_MASK			0x3
#define DL_PBUF_SIZE_MASK_SFT			(0x3 << 5)
#define DL_PBUF_SIZE_VALUE			0x0

#endif /* __DRIVERS_BUS_I2S_MT8189_H__ */
