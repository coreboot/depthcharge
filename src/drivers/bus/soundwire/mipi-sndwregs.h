// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, Intel Corporation.
 * Copyright 2020 Google LLC.
 */

#ifndef __DRIVERS_BUS_SOUNDWIRE_MIPI_SNDWREGS_H__
#define __DRIVERS_BUS_SOUNDWIRE_MIPI_SNDWREGS_H__

/* MCP_Config - MCP configuration registers. This register contains various control bits for
   reset, clock, and keeper control. */
#define SNDW_MEM_CONFIG				0x00
#define SNDW_MEM_CONFIG_MODE_AHB		0x8
#define SNDW_MEM_CONFIG_OM_NORMAL		0x7
#define SNDW_MEM_CTRL				0x04
#define SNDW_MEM_CTRL_BLOCKWAKEUP		0x1
#define SNDW_MEM_CTRL_CLOCKSTOPCLEAR		0x4

/* MCP_CmdCtrl - MCP command control register is the ping frame configuration register. */
#define SNDW_MEM_CMDCTRL			0x08

/* MCP_ConfigUpdate - This register is used to apply new configuration to the Master. */
#define SNDW_MEM_CONFIGUPDATE			0x18
#define SNDW_MEM_CONFIGUPDATE_UPDATE_DONE	0x1

/* MCP_ClockCtrl -  This register contains fields that define clock source and clock divider */
#define SNDW_MEM_CLK_CTRL0			0x30
#define SNDW_MEM_CLK_CTRL1			0x38
#define SNDW_MEM_CLK_MCLKD_MASK			0xFF

/* MCP_Stat - Master Status contains flags from all sources which can generate interrupts. */
#define SNDW_MEM_STAT				0x40
#define SNDW_MEM_STAT_TXE			0x2
#define SNDW_MEM_STAT_TXE_FIFO_EMPTY		0x2
#define SNDW_MEM_STAT_RXNE			0x8
#define SNDW_MEM_STAT_RXNE_FIFO_EMPTY		0x8

/* MCP_IntStat - Write one to specific bits of this register will clear interrupt. */
#define SNDW_MEM_INTSTAT			0x44
#define SNDW_MEM_INTMASK			0x48
#define SNDW_MEM_INTMASK_RXNE			0x8
#define SNDW_MEM_INTMASK_RXNE_FIFO_EMPTY	0x8

/* MCP_EndpointStat - This register contains endpoint device status */
#define SNDW_MEM_ENDPOINTSTAT			0x50
#define SNDW_MEM_ENDPOINTSTAT_STATUS(x)		(0x3 << ((x) * 2))

/* MCP_FIFOStat - This register contains levels of the Command FIFO and Response FIFO */
#define SNDW_MEM_FIFOSTAT			0x7C
#define SNDW_MEM_FIFOSTAT_AVAIL_MASK		0x3F
#define SNDW_MEM_FIFOSTAT_AVAIL			0
#define SNDW_MEM_FIFOSTAT_FREE_MASK		0x3F00
#define SNDW_MEM_FIFOSTAT_FREE			8

/* MCP_Command - This register is used to access TX_FIFO and RX_FIFO */
#define SNDW_MEM_COMMAND			0x80

#define SNDW_MAX_ENDPOINT_NUMBER		12
#define CLK_DIVIDER				0x3
#define SNDW_DEV_ID_NUM				6

#define SNDW_TXCMD_REGDATA_SHIFT		0
#define SNDW_TXCMD_REGDATA_MASK			0xFF
#define SNDW_TXCMD_REGADDR_SHIFT		8
#define SNDW_TXCMD_REGADDR_MASK			0xFFFF
#define SNDW_TXCMD_DEVADDR_SHIFT		24
#define SNDW_TXCMD_DEVADDR_MASK			0xF
#define SNDW_TXCMD_TYPE_SHIFT			28
#define SNDW_TXCMD_TYPE_MASK			0x7
#define SNDW_TXCMD_SSPTAG_SHIFT			31
#define SNDW_TXCMD_SSPTAG_MASK			0x1
#define SNDW_RXCMD_ACK_SHIFT			0
#define SNDW_RXCMD_ACK_MASK			0x1
#define SNDW_RXCMD_NAK_SHIFT			1
#define SNDW_RXCMD_NAK_MASK			0x1
#define SNDW_RXCMD_TYPE_SHIFT			4
#define SNDW_RXCMD_TYPE_MASK			0x7
#define SNDW_RXCMD_REGDATA_SHIFT		8
#define SNDW_RXCMD_REGDATA_MASK			0xFF

#define SNDW_MAKE_CMD_FIELD(type, name, val) \
	(((val) & SNDW_##type##CMD_##name##_MASK) << SNDW_##type##CMD_##name##_SHIFT)
#define SNDW_EXTRACT_CMD_FIELD(type, name, val) \
	(((val) >> SNDW_##type##CMD_##name##_SHIFT) & SNDW_##type##CMD_##name##_MASK)

/* Soundwire endpoint device status */
enum SNDW_ENDPOINT_STATUS {
	status_not_present	= 0,
	status_attached_ok	= 1,
	status_alert		= 2,
	status_reserved		= 3
};

/* Soundwire command types */
enum SNDW_CMD_TYPE {
	cmd_ping	= 0,
	cmd_reserved	= 1,
	cmd_read	= 2,
	cmd_write	= 3,
	cmd_invalid	= 4
};

/*
 * Soundwire endpoint device ID
 * SCP_ENDPOINT_ID_0 - Soundwire version and unique ID.
 * SCP_ENDPOINT_ID_1 - ManufacturerID [15:8].
 * SCP_ENDPOINT_ID_2 - ManufacturerID [7:0].
 * SCP_ENDPOINT_ID_3 - PartID[15:8].
 * SCP_ENDPOINT_ID_4 - PartID[7:0].
 * SCP_ENDPOINT_ID_5 - Class.
 */
enum SNDW_ENDPOINT_ID {
	SCP_ENDPOINT_ID_0 = 0x50,
	SCP_ENDPOINT_ID_1 = 0x51,
	SCP_ENDPOINT_ID_2 = 0x52,
	SCP_ENDPOINT_ID_3 = 0x53,
	SCP_ENDPOINT_ID_4 = 0x54,
	SCP_ENDPOINT_ID_5 = 0x55,
};

/*
 * Host can access the SoundWire devices by sending commands in the Soundwire frames.
 * sndw_cmd - Tx command format.
 */
typedef struct {
	uint8_t  regdata;
	uint16_t regaddr;
	uint8_t  devaddr;
	uint8_t  cmdtype;
	uint8_t  ssptag;
} sndw_cmd;

/* Soundwire codec ID */
typedef union {
	uint8_t id[SNDW_DEV_ID_NUM];
	struct {
		uint8_t version;
		uint8_t mfgid1;
		uint8_t mfgid2;
		uint8_t partid1;
		uint8_t partid2;
		uint8_t sndwclass;
	} codec;
} sndw_codec_id;

/* Soundwire codec information */
typedef struct {
	sndw_codec_id codecid;
	void *sndwlinkaddr;
	uint32_t deviceindex;
} sndw_codec_info;

#endif /* __DRIVERS_BUS_SOUNDWIRE_MIPI_SNDWREGS_H__ */
