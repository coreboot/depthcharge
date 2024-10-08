/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __DRIVERS_GPIO_MT8196_H__
#define __DRIVERS_GPIO_MT8196_H__

#include <stdint.h>

enum {
	MAX_GPIO_REG_BITS = 32,
	MAX_EINT_REG_BITS = 32,
};

enum {
	GPIO_BASE = 0x1002D000,
};

enum {
	EINT_E_BASE = 0x12080000,
	EINT_S_BASE = 0x12880000,
	EINT_W_BASE = 0x13080000,
	EINT_N_BASE = 0x13880000,
	EINT_C_BASE = 0x1C54A000,
};

enum {
	PAD_EINT0 = 0,
	PAD_EINT1 = 1,
	PAD_EINT2 = 2,
	PAD_EINT3 = 3,
	PAD_EINT4 = 4,
	PAD_EINT5 = 5,
	PAD_EINT6 = 6,
	PAD_EINT7 = 7,
	PAD_EINT8 = 8,
	PAD_EINT9 = 9,
	PAD_EINT10 = 10,
	PAD_EINT11 = 11,
	PAD_EINT12 = 12,
	PAD_EINT13 = 13,
	PAD_EINT14 = 14,
	PAD_EINT15 = 15,
	PAD_EINT16 = 16,
	PAD_EINT17 = 17,
	PAD_EINT18 = 18,
	PAD_EINT19 = 19,
	PAD_EINT20 = 20,
	PAD_EINT21 = 21,
	PAD_EINT22 = 22,
	PAD_EINT23 = 23,
	PAD_EINT24 = 24,
	PAD_EINT25 = 25,
	PAD_EINT26 = 26,
	PAD_EINT27 = 27,
	PAD_EINT28 = 28,
	PAD_EINT29 = 29,
	PAD_EINT30 = 30,
	PAD_EINT31 = 31,
	PAD_EINT32 = 32,
	PAD_EINT33 = 33,
	PAD_EINT34 = 34,
	PAD_EINT35 = 35,
	PAD_EINT36 = 36,
	PAD_EINT37 = 37,
	PAD_EINT38 = 38,
	PAD_I2SIN0_MCK = 39,
	PAD_I2SIN0_BCK = 40,
	PAD_I2SIN0_LRCK = 41,
	PAD_I2SIN0_DI = 42,
	PAD_I2SOUT0_DO = 43,
	PAD_INT_SIM1 = 44,
	PAD_INT_SIM2 = 45,
	PAD_SCP_SCL4 = 46,
	PAD_SCP_SDA4 = 47,
	PAD_SCP_SCL5 = 48,
	PAD_SCP_SDA5 = 49,
	PAD_SCP_SCL6 = 50,
	PAD_SCP_SDA6 = 51,
	PAD_AUD_SCL = 52,
	PAD_AUD_SDA = 53,
	PAD_AUD_CLK_MOSI = 54,
	PAD_AUD_CLK_MISO = 55,
	PAD_AUD_DAT_MOSI0 = 56,
	PAD_AUD_DAT_MOSI1 = 57,
	PAD_AUD_DAT_MISO0 = 58,
	PAD_AUD_DAT_MISO1 = 59,
	PAD_KPCOL0 = 60,
	PAD_CPUM_PMIC_POC = 61,
	PAD_CPUB_PMIC_POC = 62,
	PAD_GPU_PMIC_POC = 63,
	PAD_SYS_PMIC_POC = 64,
	PAD_DRAM_DVFS = 65,
	PAD_WATCHDOG = 66,
	PAD_SRCLKENA0 = 67,
	PAD_SCP_VREQ_VAO = 68,
	PAD_X32K_IN = 69,
	PAD_FL_STROBE_EN = 70,
	PAD_EINT_CHG_IRQB = 71,
	PAD_USB_RST = 72,
	PAD_LVSYS_INT = 73,
	PAD_FPM_IND = 74,
	PAD_SPMI_M_SCL = 75,
	PAD_SPMI_M_SDA = 76,
	PAD_SPMI_P_SCL = 77,
	PAD_SPMI_P_SDA = 78,
	PAD_CAM_CLK0 = 79,
	PAD_CAM_CLK1 = 80,
	PAD_SCP_SPI0_CK = 81,
	PAD_SCP_SPI0_CSB = 82,
	PAD_SCP_SPI0_MO = 83,
	PAD_SCP_SPI0_MI = 84,
	PAD_SCP_SPI1_CK = 85,
	PAD_SCP_SPI1_CSB = 86,
	PAD_SCP_SPI1_MO = 87,
	PAD_SCP_SPI1_MI = 88,
	PAD_DSI_TE = 89,
	PAD_LCM_RST = 90,
	PAD_PERIPHERAL_EN5 = 91,
	PAD_PERIPHERAL_EN6 = 92,
	PAD_PERIPHERAL_EN7 = 93,
	PAD_I2SIN1_MCK = 94,
	PAD_I2SIN1_BCK = 95,
	PAD_I2SIN1_LRCK = 96,
	PAD_I2SIN1_DI = 97,
	PAD_I2SOUT1_DO = 98,
	PAD_SCL0 = 99,
	PAD_SDA0 = 100,
	PAD_SCL10 = 101,
	PAD_SDA10 = 102,
	PAD_DISP_PWM = 103,
	PAD_SCL6 = 104,
	PAD_SDA6 = 105,
	PAD_SCP_SPI3_CK = 106,
	PAD_SCP_SPI3_CSB = 107,
	PAD_SCP_SPI3_MO = 108,
	PAD_SCP_SPI3_MI = 109,
	PAD_SPI1_CLK = 110,
	PAD_SPI1_CSB = 111,
	PAD_SPI1_MO = 112,
	PAD_SPI1_MI = 113,
	PAD_SPI_CLK_SEC = 114,
	PAD_SPI_CSB_SEC = 115,
	PAD_SPI_MO_SEC = 116,
	PAD_SPI_MI_SEC = 117,
	PAD_SPI5_CLK = 118,
	PAD_SPI5_CSB = 119,
	PAD_SPI5_MO = 120,
	PAD_SPI5_MI = 121,
	PAD_AP_GOOD = 122,
	PAD_SCL3 = 123,
	PAD_SDA3 = 124,
	PAD_MSDC1_CLK = 125,
	PAD_MSDC1_CMD = 126,
	PAD_MSDC1_DAT0 = 127,
	PAD_MSDC1_DAT1 = 128,
	PAD_MSDC1_DAT2 = 129,
	PAD_MSDC1_DAT3 = 130,
	PAD_SIM2_SCLK = 131,
	PAD_SIM2_SRST = 132,
	PAD_SIM2_SIO = 133,
	PAD_SIM1_SCLK = 134,
	PAD_SIM1_SRST = 135,
	PAD_SIM1_SIO = 136,
	PAD_MIPI0_D_SCLK = 137,
	PAD_MIPI0_D_SDATA = 138,
	PAD_MIPI1_D_SCLK = 139,
	PAD_MIPI1_D_SDATA = 140,
	PAD_MIPI2_D_SCLK = 141,
	PAD_MIPI2_D_SDATA = 142,
	PAD_MIPI3_D_SCLK = 143,
	PAD_MIPI3_D_SDATA = 144,
	PAD_BPI_D_BUS0 = 145,
	PAD_BPI_D_BUS1 = 146,
	PAD_BPI_D_BUS2 = 147,
	PAD_BPI_D_BUS3 = 148,
	PAD_BPI_D_BUS4 = 149,
	PAD_BPI_D_BUS5 = 150,
	PAD_BPI_D_BUS6 = 151,
	PAD_BPI_D_BUS7 = 152,
	PAD_MD_UCNT = 153,
	PAD_DIGRF_IRQ = 154,
	PAD_MIPI_M_SCLK = 155,
	PAD_MIPI_M_SDATA = 156,
	PAD_BPI_D_BUS8 = 157,
	PAD_BPI_D_BUS9 = 158,
	PAD_BPI_D_BUS10 = 159,
	PAD_UTXD0 = 160,
	PAD_URXD0 = 161,
	PAD_UTXD1 = 162,
	PAD_URXD1 = 163,
	PAD_SCP_SCL0 = 164,
	PAD_SCP_SDA0 = 165,
	PAD_SCP_SCL2 = 166,
	PAD_SCP_SDA2 = 167,
	PAD_SCP_SPI2_CK = 168,
	PAD_SCP_SPI2_CSB = 169,
	PAD_SCP_SPI2_MO = 170,
	PAD_SCP_SPI2_MI = 171,
	PAD_PERIPHERAL_EN2 = 172,
	PAD_PERIPHERAL_EN3 = 173,
	PAD_PERIPHERAL_EN0 = 174,
	PAD_PERIPHERAL_EN1 = 175,
	PAD_SCL5 = 176,
	PAD_SDA5 = 177,
	PAD_DMIC_CLK = 178,
	PAD_DMIC_DAT = 179,
	PAD_CAM_PDN0 = 180,
	PAD_CAM_PDN1 = 181,
	PAD_CAM_PDN2 = 182,
	PAD_CAM_PDN3 = 183,
	PAD_CAM_PDN4 = 184,
	PAD_CAM_PDN5 = 185,
	PAD_CAM_RST0 = 186,
	PAD_CAM_RST1 = 187,
	PAD_CAM_SCL2 = 188,
	PAD_CAM_SDA2 = 189,
	PAD_CAM_SCL4 = 190,
	PAD_CAM_SDA4 = 191,
	PAD_CAM_CLK2 = 192,
	PAD_CAM_RST2 = 193,
	PAD_CAM_SCL7 = 194,
	PAD_CAM_SDA7 = 195,
	PAD_CAM_CLK3 = 196,
	PAD_CAM_RST3 = 197,
	PAD_CAM_SCL8 = 198,
	PAD_CAM_SDA8 = 199,
	PAD_SCL1 = 200,
	PAD_SDA1 = 201,
	PAD_CAM_SCL9 = 202,
	PAD_CAM_SDA9 = 203,
	PAD_CAM_PDN6 = 204,
	PAD_CAM_PDN7 = 205,
	PAD_CAM_RST4 = 206,
	PAD_CAM_RST5 = 207,
	PAD_CAM_RST6 = 208,
	PAD_CAM_RST7 = 209,
	PAD_CAM_CLK4 = 210,
	PAD_CAM_CLK5 = 211,
	PAD_CAM_CLK6 = 212,
	PAD_CAM_CLK7 = 213,
	PAD_SCP_SCL3 = 214,
	PAD_SCP_SDA3 = 215,
	PAD_PERIPHERAL_EN4 = 216,
	PAD_KPROW0 = 217,
	PAD_KPROW1 = 218,
	PAD_KPCOL1 = 219,
	PAD_SPI0_CLK = 220,
	PAD_SPI0_CSB = 221,
	PAD_SPI0_MO = 222,
	PAD_SPI0_MI = 223,
	PAD_MSDC2_CLK = 224,
	PAD_MSDC2_CMD = 225,
	PAD_MSDC2_DAT0 = 226,
	PAD_MSDC2_DAT1 = 227,
	PAD_MSDC2_DAT2 = 228,
	PAD_MSDC2_DAT3 = 229,
	PAD_CONN_TOP_CLK = 230,
	PAD_CONN_TOP_DATA = 231,
	PAD_CONN_HRST_B = 232,
	PAD_BT_BCK = 233,
	PAD_BT_LRCK = 234,
	PAD_BT_DI = 235,
	PAD_BT_DO = 236,
	PAD_BT_UTXD = 237,
	PAD_BT_URXD = 238,
	PAD_SCP_WB_UTXD = 239,
	PAD_SCP_WB_URXD = 240,
	PAD_PCIE0_PERSTN = 241,
	PAD_PCIE0_WAKEN = 242,
	PAD_PCIE0_CLKREQN = 243,
	PAD_CONN_RST = 244,
	PAD_CONN_FAULTB = 245,
	PAD_COEX_UTXD = 246,
	PAD_COEX_URXD = 247,
	PAD_BT_RST = 248,
	PAD_WF_RST = 249,
	PAD_CONN_PMIC_EN = 250,
	PAD_IVI_GPIO0 = 251,
	PAD_IVI_GPIO1 = 252,
	PAD_IVI_GPIO2 = 253,
	PAD_IVI_GPIO3 = 254,
	PAD_IVI_GPIO4 = 255,
	PAD_IVI_GPIO5 = 256,
	PAD_IVI_GPIO6 = 257,
	PAD_IVI_GPIO7 = 258,
	PAD_RGMII_0 = 259,
	PAD_RGMII_1 = 260,
	PAD_RGMII_2 = 261,
	PAD_RGMII_3 = 262,
	PAD_RGMII_4 = 263,
	PAD_RGMII_5 = 264,
	PAD_RGMII_6 = 265,
	PAD_RGMII_7 = 266,
	PAD_RGMII_8 = 267,
	PAD_RGMII_9 = 268,
	PAD_RGMII_10 = 269,
	PAD_RGMII_11 = 270,
	GPIO_NUM,
};

typedef struct {
	GpioValRegs dir[9];
	uint8_t rsv00[112];
	GpioValRegs dout[9];
	uint8_t rsv01[112];
	GpioValRegs din[9];
	uint8_t rsv02[112];
	GpioValRegs mode[34];
} GpioRegs;

typedef struct {
	uint32_t sta[16];
	uint32_t ack[16];
} EintRegs;

#endif /* __DRIVERS_GPIO_MT8196_H__ */
