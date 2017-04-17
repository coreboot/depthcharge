#ifndef __DRIVERS_MT_GPIO_H__
#define __DRIVERS_MT_GPIO_H__

#include "drivers/gpio/gpio.h"

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
	PAD_IDDIG = 16,
	PAD_WATCHDOG = 17,
	PAD_CEC = 18,
	PAD_HDMISCK = 19,
	PAD_HDMISD = 20,
	PAD_HTPLG = 21,
	PAD_MSDC3_DAT0 = 22,
	PAD_MSDC3_DAT1 = 23,
	PAD_MSDC3_DAT2 = 24,
	PAD_MSDC3_DAT3 = 25,
	PAD_MSDC3_CLK = 26,
	PAD_MSDC3_CMD = 27,
	PAD_MSDC3_DSL = 28,
	PAD_UCTS2 = 29,
	PAD_URTS2 = 30,
	PAD_URXD2 = 31,
	PAD_UTXD2 = 32,
	PAD_DAICLK = 33,
	PAD_DAIPCMIN = 34,
	PAD_DAIPCMOUT = 35,
	PAD_DAISYNC = 36,
	PAD_EINT16 = 37,
	PAD_CONN_RST = 38,
	PAD_CM2MCLK = 39,
	PAD_CMPCLK = 40,
	PAD_CMMCLK = 41,
	PAD_DSI_TE = 42,
	PAD_SDA2 = 43,
	PAD_SCL2 = 44,
	PAD_SDA0 = 45,
	PAD_SCL0 = 46,
	PAD_RDN0_A = 47,
	PAD_RDP0_A = 48,
	PAD_RDN1_A = 49,
	PAD_RDP1_A = 50,
	PAD_RCN_A = 51,
	PAD_RCP_A = 52,
	PAD_RDN2_A = 53,
	PAD_RDP2_A = 54,
	PAD_RDN3_A = 55,
	PAD_RDP3_A = 56,
	PAD_MSDC0_DAT0 = 57,
	PAD_MSDC0_DAT1 = 58,
	PAD_MSDC0_DAT2 = 59,
	PAD_MSDC0_DAT3 = 60,
	PAD_MSDC0_DAT4 = 61,
	PAD_MSDC0_DAT5 = 62,
	PAD_MSDC0_DAT6 = 63,
	PAD_MSDC0_DAT7 = 64,
	PAD_MSDC0_CLK = 65,
	PAD_MSDC0_CMD = 66,
	PAD_MSDC0_DSL = 67,
	PAD_MSDC0_RST = 68,
	PAD_SPI_CK = 69,
	PAD_SPI_MI = 70,
	PAD_SPI_MO = 71,
	PAD_SPI_CS = 72,
	PAD_MSDC1_DAT0 = 73,
	PAD_MSDC1_DAT1 = 74,
	PAD_MSDC1_DAT2 = 75,
	PAD_MSDC1_DAT3 = 76,
	PAD_MSDC1_CLK = 77,
	PAD_MSDC1_CMD = 78,
	PAD_PWRAP_SPI0_MI = 79,
	PAD_PWRAP_SPI0_MO = 80,
	PAD_PWRAP_SPI0_CK = 81,
	PAD_PWRAP_SPI0_CSN = 82,
	PAD_AUD_CLK_MOSI = 83,
	PAD_AUD_DAT_MISO = 84,
	PAD_AUD_DAT_MOSI = 85,
	PAD_RTC32K_CK = 86,
	PAD_DISP_PWM0 = 87,
	PAD_SRCLKENAI = 88,
	PAD_SRCLKENAI2 = 89,
	PAD_SRCLKENA0 = 90,
	PAD_SRCLKENA1 = 91,
	PAD_PCM_CLK = 92,
	PAD_PCM_SYNC = 93,
	PAD_PCM_RX = 94,
	PAD_PCM_TX = 95,
	PAD_URXD1 = 96,
	PAD_UTXD1 = 97,
	PAD_URTS1 = 98,
	PAD_UCTS1 = 99,
	PAD_MSDC2_DAT0 = 100,
	PAD_MSDC2_DAT1 = 101,
	PAD_MSDC2_DAT2 = 102,
	PAD_MSDC2_DAT3 = 103,
	PAD_MSDC2_CLK = 104,
	PAD_MSDC2_CMD = 105,
	PAD_SDA3 = 106,
	PAD_SCL3 = 107,
	PAD_JTMS = 108,
	PAD_JTCK = 109,
	PAD_JTDI = 110,
	PAD_JTDO = 111,
	PAD_JTRST_B = 112,
	PAD_URXD0 = 113,
	PAD_UTXD0 = 114,
	PAD_URTS0 = 115,
	PAD_UCTS0 = 116,
	PAD_URXD3 = 117,
	PAD_UTXD3 = 118,
	PAD_KPROW0 = 119,
	PAD_KPROW1 = 120,
	PAD_KPROW2 = 121,
	PAD_KPCOL0 = 122,
	PAD_KPCOL1 = 123,
	PAD_KPCOL2 = 124,
	PAD_SDA1 = 125,
	PAD_SCL1 = 126,
	PAD_LCM_RST = 127,
	PAD_I2S0_LRCK = 128,
	PAD_I2S0_BCK = 129,
	PAD_I2S0_MCK = 130,
	PAD_I2S0_DATA0 = 131,
	PAD_I2S0_DATA1 = 132,
	PAD_SDA4 = 133,
	PAD_SCL4 = 134,
	GPIO_MAX,
};

typedef struct {
	uint16_t val;
	uint16_t rsv00;
	uint16_t set;
	uint16_t rsv01;
	uint16_t rst;
	uint16_t rsv02[3];
} GpioValRegs;

typedef struct {
	GpioValRegs dir[9];		/* 0x0000 ~ 0x008F: 144 bytes */
	uint8_t rsv00[112];		/* 0x0090 ~ 0x00FF: 112 bytes */
	GpioValRegs pullen[9];		/* 0x0100 ~ 0x018F: 144 bytes */
	uint8_t rsv01[112];		/* 0x0190 ~ 0x01FF: 112 bytes */
	GpioValRegs pullsel[9];		/* 0x0200 ~ 0x028F: 144 bytes */
	uint8_t rsv02[112];		/* 0x0290 ~ 0x02FF: 112 bytes */
	uint8_t rsv03[256];		/* 0x0300 ~ 0x03FF: 256 bytes */
	GpioValRegs dout[9];		/* 0x0400 ~ 0x048F: 144 bytes */
	uint8_t rsv04[112];		/* 0x0490 ~ 0x04FF: 112 bytes */
	GpioValRegs din[9];		/* 0x0500 ~ 0x058F: 114 bytes */
	uint8_t rsv05[112];		/* 0x0590 ~ 0x05FF: 112 bytes */
	GpioValRegs mode[27];		/* 0x0600 ~ 0x07AF: 432 bytes */
	uint8_t rsv06[336];		/* 0x07B0 ~ 0x08FF: 336 bytes */
	GpioValRegs ies[3];		/* 0x0900 ~ 0x092F:  48 bytes */
	GpioValRegs smt[3];		/* 0x0930 ~ 0x095F:  48 bytes */
	uint8_t rsv07[160];		/* 0x0960 ~ 0x09FF: 160 bytes */
	GpioValRegs tdsel[8];		/* 0x0A00 ~ 0x0A7F: 128 bytes */
	GpioValRegs rdsel[6];	        /* 0x0A80 ~ 0x0ADF:  96 bytes */
	uint8_t rsv08[32];		/* 0x0AE0 ~ 0x0AFF:  32 bytes */
	GpioValRegs drv_mode[10]; 	/* 0x0B00 ~ 0x0B9F: 160 bytes */
	uint8_t rsv09[96];		/* 0x0BA0 ~ 0x0BFF:  96 bytes */
	GpioValRegs msdc0_ctrl0;	/* 0x0C00 ~ 0x0C0F:  16 bytes */
	GpioValRegs msdc0_ctrl1;	/* 0x0C10 ~ 0x0C1F:  16 bytes */
	GpioValRegs msdc0_ctrl2;	/* 0x0C20 ~ 0x0C2F:  16 bytes */
	GpioValRegs msdc0_ctrl5;	/* 0x0C30 ~ 0x0C3F:  16 bytes */
	GpioValRegs msdc1_ctrl0;	/* 0x0C40 ~ 0x0C4F:  16 bytes */
	GpioValRegs msdc1_ctrl1;	/* 0x0C50 ~ 0x0C5F:  16 bytes */
	GpioValRegs msdc1_ctrl2;	/* 0x0C60 ~ 0x0C6F:  16 bytes */
	GpioValRegs msdc1_ctrl5;	/* 0x0C70 ~ 0x0C7F:  16 bytes */
	GpioValRegs msdc2_ctrl0;	/* 0x0C80 ~ 0x0C8F:  16 bytes */
	GpioValRegs msdc2_ctrl1;	/* 0x0C90 ~ 0x0C9F:  16 bytes */
	GpioValRegs msdc2_ctrl2;	/* 0x0CA0 ~ 0x0CAF:  16 bytes */
	GpioValRegs msdc2_ctrl5;	/* 0x0CB0 ~ 0x0CBF:  16 bytes */
	GpioValRegs msdc3_ctrl0;	/* 0x0CC0 ~ 0x0CCF:  16 bytes */
	GpioValRegs msdc3_ctrl1;	/* 0x0CD0 ~ 0x0CDF:  16 bytes */
	GpioValRegs msdc3_ctrl2;	/* 0x0CE0 ~ 0x0CEF:  16 bytes */
	GpioValRegs msdc3_ctrl5;	/* 0x0CF0 ~ 0x0CFF:  16 bytes */
	GpioValRegs msdc0_ctrl3;	/* 0x0D00 ~ 0x0D0F:  16 bytes */
	GpioValRegs msdc0_ctrl4;	/* 0x0D10 ~ 0x0D1F:  16 bytes */
	GpioValRegs msdc1_ctrl3;	/* 0x0D20 ~ 0x0D2F:  16 bytes */
	GpioValRegs msdc1_ctrl4;	/* 0x0D30 ~ 0x0D3F:  16 bytes */
	GpioValRegs msdc2_ctrl3;	/* 0x0D40 ~ 0x0D4F:  16 bytes */
	GpioValRegs msdc2_ctrl4;	/* 0x0D50 ~ 0x0D5F:  16 bytes */
	GpioValRegs msdc3_ctrl3;	/* 0x0D60 ~ 0x0D6F:  16 bytes */
	GpioValRegs msdc3_ctrl4;	/* 0x0D70 ~ 0x0D7F:  16 bytes */
	uint8_t rsv10[64];		/* 0x0D80 ~ 0x0DBF:  64 bytes */
	GpioValRegs exmd_ctrl[1];	/* 0x0DC0 ~ 0x0DCF:  16 bytes */
	uint8_t rsv11[48];		/* 0x0DD0 ~ 0x0DFF:  48 bytes */
	GpioValRegs kpad_ctrl[2];	/* 0x0E00 ~ 0x0E1F:  32 bytes */
	GpioValRegs hsic_ctrl[4];	/* 0x0E20 ~ 0x0E5F:  64 bytes */
} GpioRegs;

typedef struct MtGpio {
	GpioOps ops;
	u32 pin_num;
} MtGpio;

typedef struct {
	uint32_t sta[16];		/* 0xB000 ~ 0xB03F: 64 bytes */
	uint32_t ack[16];		/* 0xB040 ~ 0xB07F: 64 bytes */
	uint32_t mask[16];		/* 0xB080 ~ 0xB0BF: 64 bytes */
	uint32_t mask_set[16];		/* 0xB0C0 ~ 0xB0FF: 64 bytes */
	uint32_t mask_clr[16];		/* 0xB100 ~ 0xB13F: 64 bytes */
	uint32_t sens[16];		/* 0xB140 ~ 0xB17F: 64 bytes */
	uint32_t sens_set[16];		/* 0xB180 ~ 0xB1BF: 64 bytes */
	uint32_t sens_clr[16];		/* 0xB1C0 ~ 0xB1FF: 64 bytes */
	uint32_t soft[16];		/* 0xB200 ~ 0xB23F: 64 bytes */
	uint32_t soft_set[16];		/* 0xB240 ~ 0xB27F: 64 bytes */
	uint32_t soft_clr[16];		/* 0xB280 ~ 0xB2BF: 64 bytes */
	uint32_t rsv00[16];		/* 0xB2C0 ~ 0xB2FF: 64 bytes */
	uint32_t pol[16];		/* 0xB300 ~ 0xB33F: 64 bytes */
	uint32_t pol_set[16];		/* 0xB340 ~ 0xB35F: 64 bytes */
	uint32_t pol_clr[16];		/* 0xB380 ~ 0xB39F: 64 bytes */
	uint32_t rsv01[16];		/* 0xB3C0 ~ 0xB3FF: 64 bytes */
	uint32_t d0en[8];		/* 0xB400 ~ 0xB41F: 32 bytes */
	uint32_t d1en[8];		/* 0xB420 ~ 0xB43F: 32 bytes */
	uint32_t resv02[48];		/* 0xB440 ~ 0xB4FF: 192 bytes */
	uint32_t dbnc_3_0[8];		/* 0xB500 ~ 0xB51F: 32 bytes */
	uint32_t resv03[56];		/* 0xB520 ~ 0xB5FF: 224 bytes */
	uint32_t dbnc_set[8];		/* 0xB600 ~ 0xB61F: 32 bytes */
	uint32_t resv04[56];		/* 0xB620 ~ 0xB6FF: 224 bytes */
	uint32_t dbnc_clr[8];		/* 0xB700 ~ 0xB71F: 32 bytes */
	uint32_t resv05[56];		/* 0xB720 ~ 0xB7FF: 224 bytes */
	uint32_t deint_con;		/* 0xB800 ~ 0xB803: 4 bytes */
	uint32_t resv06[15];		/* 0xB804 ~ 0xB83F: 60 bytes */
	uint32_t deint_sel_3_0;		/* 0xB840 ~ 0xB843: 4 bytes */
	uint32_t deint_sel_7_4;		/* 0xB844 ~ 0xB847: 4 bytes */
	uint32_t resv07[14];		/* 0xB848 ~ 0xB87F: 56 bytes */
	uint32_t deint_sel_3_0_set;	/* 0xB880 ~ 0xB883: 4 bytes */
	uint32_t deint_sel_7_4_set;	/* 0xB884 ~ 0xB887: 4 bytes */
	uint32_t resv08[14];		/* 0xB888 ~ 0xB8BF: 56 bytes */
	uint32_t deint_sel_3_0_clr;	/* 0xB8C0 ~ 0xB8C3: 4 bytes */
	uint32_t deint_sel_7_4_clr;	/* 0xB8C4 ~ 0xB8C7: 4 bytes */
	uint32_t resv09[14];		/* 0xB8C8 ~ 0xB8FF: 56 bytes */
	uint32_t eevt;			/* 0xB900 ~ 0xB903: 4 bytes */
	uint32_t resv10[63];		/* 0xB8C8 ~ 0xB8FF: 252 bytes */
	uint32_t raw_sta[16];		/* 0xBA00 ~ 0xBA3F: 64 bytes */
} EintRegs;

GpioOps *new_mtk_gpio_input(u32 pin);
GpioOps *new_mtk_gpio_output(u32 pin);
GpioOps *new_mtk_eint(u32 pin);

#endif /* __DRIVERS_MT_GPIO_H__ */

