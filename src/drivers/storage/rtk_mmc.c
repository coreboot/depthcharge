/*
 * Copyright 2022 Realtek Inc.
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

#include <assert.h>
#include <libpayload.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/bus/pci/pci.h"
#include "drivers/storage/rtk_mmc.h"

int rtsx_write_register(RtkMmcHost *host, u16 addr, u8 mask, u8 data)
{
	u32 val = HAIMR_WRITE_START;

	val |= (u32)(addr & HAIMR_VALID_ADDR_MASK) << 16;
	val |= (u32)mask << 8;
	val |= (u32)data;

	rtsx_writel(host, RTSX_HAIMR, val);

	for (int i = 0; i < MAX_RW_REG_CNT; i++) {
		val = rtsx_readl(host, RTSX_HAIMR);
		if ((val & HAIMR_TRANS_END) == 0) {
			if (data != (u8)val)
				return STATUS_FAIL;
			return STATUS_SUCCESS;
		}
	}
	return STATUS_TIMEOUT;
}

int rtsx_read_register(RtkMmcHost *host, u16 addr, u8 *data)
{
	int i;
	u32 val = HAIMR_READ_START;

	if (data)
		*data = 0;

	val |= (u32)(addr & HAIMR_VALID_ADDR_MASK) << 16;

	rtsx_writel(host, RTSX_HAIMR, val);

	for (i = 0; i < MAX_RW_REG_CNT; i++) {
		val = rtsx_readl(host, RTSX_HAIMR);
		if ((val & HAIMR_TRANS_END) == 0)
			break;
	}

	if (i >= MAX_RW_REG_CNT)
		return STATUS_TIMEOUT;

	if (data)
		*data = (u8)(val & 0xFF);

	return STATUS_SUCCESS;
}

int rtsx_write_cfg_dw(RtkMmcHost *host, u8 func_no, u16 addr, u32 mask, u32 val)
{
	u8 mode = 0, tmp;

	for (int i = 0; i < 4; i++) {
		if (mask & 0xFF) {
			RTSX_WRITE_REG(host, (u16)(CFGDATA0 + i), 0xFF,
					(u8)(val & mask & 0xFF));
			mode |= (1 << i);
		}
		mask >>= 8;
		val >>= 8;
	}

	if (mode) {
		RTSX_WRITE_REG(host, CFGADDR0, 0xFF, (u8)addr);
		RTSX_WRITE_REG(host, CFGADDR1, 0xFF, (u8)(addr >> 8));
		RTSX_WRITE_REG(host, CFGRWCTL, 0xFF, 0x80 | mode |
				((func_no & 0x03) << 4));

		/* Wait transfer end */
		for (int i = 0; i < MAX_RW_REG_CNT; i++) {
			RTSX_READ_REG(host, CFGRWCTL, &tmp);
			if ((tmp & PHY_CFG_RW_OK) == 0)
				break;
		}
	}

	return STATUS_SUCCESS;
}

int rtsx_write_phy_register(RtkMmcHost *host, u8 addr, u16 val)
{
	int finished = 0;
	u8 tmp;

	RTSX_WRITE_REG(host, PHYDATA0, 0xFF, (u8)val);
	RTSX_WRITE_REG(host, PHYDATA1, 0xFF, (u8)(val >> 8));
	RTSX_WRITE_REG(host, PHYADDR, 0xFF, addr);

	RTSX_WRITE_REG(host, PHYRWCTL, 0xFF, 0x81);

	for (int i = 0; i < MAX_RW_PHY_CNT; i++) {
		RTSX_READ_REG(host, PHYRWCTL, &tmp);
		if (!(tmp & PHY_CFG_RW_OK)) {
			finished = 1;
			break;
		}
	}

	if (!finished)
		return STATUS_FAIL;
	return STATUS_SUCCESS;
}

typedef struct rtk_pci_host {
	RtkMmcHost host;
	pcidev_t dev;
	int (*update)(BlockDevCtrlrOps *me);
} RtkPciHost;

static bool is_rtk_ctrlr(pcidev_t dev, u16 *pid)
{
	bool found;
	const u16 vendorid = pci_read_config16(dev, REG_VENDOR_ID);
	const u16 deviceid = pci_read_config16(dev, REG_DEVICE_ID);

	if (vendorid != RTK_VID)
		return false;

	switch (deviceid) {
	case RTK_MMC_PID_522A:
	case RTK_MMC_PID_525A:
		found = true;
		break;
	default:
		found = false;
		break;
	}

	*pid = deviceid;

	return found;
}

static int rtk_get_pci_bar(pcidev_t dev, uintptr_t *bar)
{
	uint32_t addr;

	if (pci_read_config16(dev, REG_DEVICE_ID) == RTK_MMC_PID_525A)
		addr = pci_read_config32(dev, PCI_BASE_ADDRESS_1);
	else
		addr = pci_read_config32(dev, PCI_BASE_ADDRESS_0);

	if (addr == ((uint32_t)~0))
		return -1;

	*bar = (uintptr_t)(addr & PCI_BASE_ADDRESS_MEM_MASK);

	return 0;
}

static int rtkhost_pci_init(BlockDevCtrlrOps *me)
{
	RtkPciHost *pci_host =
		container_of(me, RtkPciHost, host.mmc_ctrlr.ctrlr.ops);
	RtkMmcHost *host = &pci_host->host;
	BlockDevCtrlr *block_ctrlr = &host->mmc_ctrlr.ctrlr;
	pcidev_t dev = pci_host->dev;
	uintptr_t bar;
	uint16_t pid;

	if (is_pci_bridge(dev)) {
		uint32_t bus = pci_read_config32(dev, REG_PRIMARY_BUS);

		bus = (bus >> 8) & 0xff;
		dev = PCI_DEV(bus, 0, 0);
	}

	if (!is_rtk_ctrlr(dev, &pid)) {
		mmc_error("No known Realtek reader found at %d:%d.%d",
			PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));
		block_ctrlr->ops.update = NULL;
		block_ctrlr->need_update = 0;
		return -1;
	}

	mmc_debug("Found Realtek reader at %d:%d:%d\n", PCI_BUS(dev),
		PCI_SLOT(dev), PCI_FUNC(dev));

	if (rtk_get_pci_bar(dev, &bar)) {
		mmc_error("Failed to get BAR for PCI Realtek device\n");
		block_ctrlr->ops.update = NULL;
		block_ctrlr->need_update = 0;
		return -1;
	}

	mmc_debug("Bar Address = %ld\n", bar);
	host->pid = pid;
	host->ioaddr = (void *)bar;
	host->name = xzalloc(RTK_NAME_LENGTH);
	host->dev = dev;

	switch (host->pid) {
	case RTK_MMC_PID_522A:
		host->sd_800mA_ocp_thd = 0x06;
		break;
	case RTK_MMC_PID_525A:
		host->sd_800mA_ocp_thd = 0x05;
		break;
	default:
		break;
	}

	block_ctrlr->ops.update = pci_host->update;
	pci_host->update = NULL;

	return block_ctrlr->ops.update(me);
}

void rtsx_stop_cmd(RtkMmcHost *host)
{
	rtsx_writel(host, RTSX_HCBCTLR, STOP_CMD);
	rtsx_writel(host, RTSX_HDBCTLR, STOP_DMA);

	rtsx_write_register(host, DMACTL, 0x80, 0x80);
	rtsx_write_register(host, RBCTL, 0x80, 0x80);
}

void rtsx_send_cmd_no_wait(RtkMmcHost *host)
{
	u32 val = (u32)(1 << 31);

	rtsx_writel(host, RTSX_HCBAR, (uintptr_t)host->host_cmds_ptr);

	val |= (u32)(host->ci * 4) & 0x00FFFFFF;
	/* Hardware Auto Response */
	val |= 0x40000000;
	rtsx_writel(host, RTSX_HCBCTLR, val);
	host->ci = 0;
}

int rtsx_send_cmd(RtkMmcHost *host, int timeout)
{
	int err = 0;
	u32 result32, loop = 0;
	u32 val = (u32)(1 << 31);

	rtsx_writel(host, RTSX_HCBAR, (uintptr_t)host->host_cmds_ptr);

	val |= (u32)(host->ci * 4) & 0x00FFFFFF;
	/* Hardware Auto Response */
	val |= 0x40000000;
	rtsx_writel(host, RTSX_HCBCTLR, val);

	do {
		result32 = rtsx_readl(host, RTSX_BIPR);
		loop++;
		if (loop > timeout) {
			err = -1;
			break;
		}
		mdelay(1);
		mmc_debug("BIPR = 0x%x\n", result32);
	} while (!(result32 & TRANS_OK_INT));

	host->int_reg = result32 & 0xFFFFFFFF;
	rtsx_writel(host, RTSX_BIPR, host->int_reg | 0xFF);

	if (err < 0)
		rtsx_stop_cmd(host);

	host->ci = 0;
	mmc_debug("%s, err = %d\n", __func__, err);

	return err;
}

void rtsx_add_cmd(RtkMmcHost *host, u8 cmd_type, u16 reg_addr, u8 mask, u8 data)
{
	u32 *cb = (u32 *)(host->host_cmds_ptr);
	u32 val = 0;

	val |= (u32)(cmd_type & 0x03) << 30;
	val |= (u32)(reg_addr & HAIMR_VALID_ADDR_MASK) << 16;
	val |= (u32)mask << 8;
	val |= (u32)data;

	if (host->ci < (HOST_CMDS_BUF_LEN / 4))
		cb[(host->ci)++] = htole32(val);
}

static int sd_response_type(MmcCommand *cmd)
{
	switch (cmd->resp_type) {
	case MMC_RSP_NONE:
		return SD_RSP_TYPE_R0;
	case MMC_RSP_R1:
		return SD_RSP_TYPE_R1;
	case MMC_RSP_R1b:
		return SD_RSP_TYPE_R1b;
	case MMC_RSP_R2:
		return SD_RSP_TYPE_R2;
	case MMC_RSP_R3:
		return SD_RSP_TYPE_R3;
	default:
		return -1;
	}
}

static int sd_send_cmd_get_rsp(RtkMmcHost *host, MmcCommand *cmd)
{
	unsigned int timeout = 0;
	int err, reg_addr, rsp_type;
	int stat_idx = 0;
	u8 *ptr;

	rsp_type = sd_response_type(cmd);
	if (rsp_type < 0)
		return STATUS_FAIL;
	mmc_debug("%s, rsp_type = %d\n", __func__, rsp_type);

	/* Wait max 1 sec */
	timeout = 1000;

	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD0, 0xFF,
		0x40 | cmd->cmdidx);
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD1, 0xFF,
		(u8)(cmd->cmdarg >> 24));
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD2, 0xFF,
		(u8)(cmd->cmdarg >> 16));
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD3, 0xFF,
		(u8)(cmd->cmdarg >> 8));
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD4, 0xFF,
		(u8)cmd->cmdarg);

	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CFG2, 0xFF, rsp_type);
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_DATA_SOURCE,
		0x01, PINGPONG_BUFFER);
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_TRANSFER,
		0xFF, SD_TM_CMD_RSP | SD_TRANSFER_START);
	rtsx_add_cmd(host, CHECK_REG_CMD, SD_TRANSFER,
		SD_TRANSFER_END | SD_STAT_IDLE, SD_TRANSFER_END | SD_STAT_IDLE);

	if (rsp_type == SD_RSP_TYPE_R2) {
		/* Read data from ping-pong buffer */
		for (reg_addr = PPBUF_BASE2; reg_addr < PPBUF_BASE2 + 16; reg_addr++)
			rtsx_add_cmd(host, READ_REG_CMD, reg_addr, 0, 0);
		stat_idx = 16;
	} else if (rsp_type != SD_RSP_TYPE_R0) {
		/* Read data from SD_CMDx registers */
		for (reg_addr = SD_CMD0; reg_addr <= SD_CMD4; reg_addr++)
			rtsx_add_cmd(host, READ_REG_CMD, reg_addr, 0, 0);
		stat_idx = 5;
	}

	rtsx_add_cmd(host, READ_REG_CMD, SD_STAT1, 0, 0);
	err = rtsx_send_cmd(host, timeout);
	if (err != 0) {
		mmc_error("%s, rtsx_send_cmd failed\n", __func__);
		rtsx_write_register(host, CARD_STOP, SD_STOP | SD_CLR_ERR,
			SD_STOP | SD_CLR_ERR);
		return STATUS_FAIL;
	}

	if (rsp_type == SD_RSP_TYPE_R0)
		return STATUS_SUCCESS;

	ptr = (u8 *)host->host_cmds_ptr + 1;
	if ((ptr[0] & 0xC0) != 0) {
		mmc_error("Invalid response bit\n");
		return STATUS_FAIL;
	}

	/* Check CRC7 */
	if (!(rsp_type & SD_NO_CHECK_CRC7)) {
		if (ptr[stat_idx] & SD_CRC7_ERR) {
			mmc_error("Check CRC7 failed\n");
			return STATUS_FAIL;
		}
	}

	/* Add Get Response */
	if (rsp_type == SD_RSP_TYPE_R2) {
		/*
		 * The controller offloads the last byte {CRC-7, end bit 1'b1}
		 * of response type R2. Assign dummy CRC, 0, and end bit to the
		 * byte(ptr[16], goes into the LSB of resp[3] later).
		 */
		ptr[16] = 1;

		cmd->response[0] = (ptr[1] << 24) | (ptr[2] << 16) |
			(ptr[3] << 8) | ptr[4];
		cmd->response[1] = (ptr[5] << 24) | (ptr[6] << 16) |
			(ptr[7] << 8) | ptr[8];
		cmd->response[2] = (ptr[9] << 24) | (ptr[10] << 16) |
			 (ptr[11] << 8) | ptr[12];
		cmd->response[3] = (ptr[13] << 24) | (ptr[14] << 16) |
			(ptr[15] << 8) | ptr[16];
	} else {
		cmd->response[0] = (ptr[1] << 24) | (ptr[2] << 16) |
			(ptr[3] << 8) | ptr[4];
	}

	return STATUS_SUCCESS;
}

int rtsx_read_ppbuf(RtkMmcHost *host, void *buf, int buf_len)
{
	int retval, i, j, timeout;
	u16 reg_addr;
	u8 *ptr;

	timeout = 250;
	ptr = buf;
	reg_addr = PPBUF_BASE2;

	for (i = 0; i < buf_len / 256; i++) {
		for (j = 0; j < 256; j++)
			rtsx_add_cmd(host, READ_REG_CMD, reg_addr++, 0, 0);

		retval = rtsx_send_cmd(host, timeout);

		if (retval < 0)
			return STATUS_FAIL;

		memcpy(ptr, host->host_cmds_ptr, 256);
		ptr += 256;
	}

	if (buf_len % 256) {
		for (i = 0; i < buf_len % 256; i++)
			rtsx_add_cmd(host, READ_REG_CMD, reg_addr++, 0, 0);

		retval = rtsx_send_cmd(host, timeout);

		if (retval < 0)
			return STATUS_FAIL;
	}

	memcpy(ptr, host->host_cmds_ptr, buf_len % 256);

	return STATUS_SUCCESS;
}

int rtsx_write_ppbuf(RtkMmcHost *host, void *buf, int buf_len)
{
	int retval;
	int i, j, timeout;
	u16 reg_addr;
	u8 *ptr;

	timeout = 250;
	ptr = buf;
	reg_addr = PPBUF_BASE2;

	for (i = 0; i < buf_len / 256; i++) {
		for (j = 0; j < 256; j++) {
			rtsx_add_cmd(host, WRITE_REG_CMD, reg_addr++,
				0xFF, *ptr);
			ptr++;
		}

		retval = rtsx_send_cmd(host, timeout);

		if (retval < 0)
			return STATUS_FAIL;
	}

	if (buf_len % 256) {
		for (i = 0; i < buf_len % 256; i++) {
			rtsx_add_cmd(host, WRITE_REG_CMD, reg_addr++,
				0xFF, *ptr);
			ptr++;
		}

		retval = rtsx_send_cmd(host, timeout);

		if (retval < 0)
			return STATUS_FAIL;
	}

	return STATUS_SUCCESS;
}

static int sd_read_data(RtkMmcHost *host, MmcCommand *cmd, MmcData *data)
{
	int retval, timeout;

	timeout = 200;
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD0, 0xFF,
		0x40 | cmd->cmdidx);
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD1, 0xFF,
		(u8)(cmd->cmdarg >> 24));
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD2, 0xFF,
		(u8)(cmd->cmdarg >> 16));
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD3, 0xFF,
		(u8)(cmd->cmdarg >> 8));
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD4, 0xFF,
		(u8)cmd->cmdarg);

	rtsx_add_cmd(host, WRITE_REG_CMD, SD_BYTE_CNT_L, 0xFF,
		(u8)data->blocksize);
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_BYTE_CNT_H, 0xFF,
		(u8)(data->blocksize >> 8));
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_BLOCK_CNT_L, 0xFF,
		(u8)data->blocks);
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_BLOCK_CNT_H, 0xFF,
		(u8)(data->blocks >> 8));

	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CFG2, 0xFF,
		SD_CALCULATE_CRC7 | SD_CHECK_CRC16 | SD_NO_WAIT_BUSY_END |
		SD_CHECK_CRC7 | SD_RSP_LEN_6);
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_DATA_SOURCE,
		0x01, PINGPONG_BUFFER);
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_TRANSFER, 0xFF,
	SD_TM_NORMAL_READ | SD_TRANSFER_START);
	rtsx_add_cmd(host, CHECK_REG_CMD, SD_TRANSFER,
		SD_TRANSFER_END, SD_TRANSFER_END);

	retval = rtsx_send_cmd(host, timeout);
	if (retval < 0) {
		mmc_error("%s, rtsx_send_cmd failed\n", __func__);
		MmcCommand temp_cmd = {
			.cmdidx = MMC_CMD_SEND_STATUS,
			.resp_type = MMC_RSP_R1,
			.cmdarg = host->sd_addr,
		};
		sd_send_cmd_get_rsp(host, &temp_cmd);

		return STATUS_FAIL;
	}

	retval = rtsx_read_ppbuf(host, (void *)data->dest,
		data->blocksize * data->blocks);
	if (retval != STATUS_SUCCESS) {
		mmc_error("rtsx_read_ppbuf failed\n");
		return STATUS_FAIL;
	}

	return STATUS_SUCCESS;
}

static int sd_write_data(RtkMmcHost *host, MmcCommand *cmd, MmcData *data)
{
	int retval, timeout;

	timeout = 200;
	retval = sd_send_cmd_get_rsp(host, cmd);
	if (retval < 0) {
		mmc_error("%s, sd_send_cmd_get_rsp failed\n", __func__);
		return STATUS_FAIL;
	}

	retval = rtsx_write_ppbuf(host, (void *)data->src,
		data->blocksize * data->blocks);
	if (retval != STATUS_SUCCESS) {
		mmc_error("rtsx_write_ppbuf failed\n");
		return STATUS_FAIL;
	}

	rtsx_add_cmd(host, WRITE_REG_CMD, SD_BYTE_CNT_L, 0xFF,
		(u8)data->blocksize);
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_BYTE_CNT_H, 0xFF,
		(u8)(data->blocksize >> 8));
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_BLOCK_CNT_L, 0xFF,
		(u8)data->blocks);
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_BLOCK_CNT_H, 0xFF,
		(u8)(data->blocks >> 8));

	rtsx_add_cmd(host, WRITE_REG_CMD, SD_CFG2, 0xFF,
		SD_CALCULATE_CRC7 | SD_CHECK_CRC16 | SD_NO_WAIT_BUSY_END |
		SD_CHECK_CRC7 | SD_RSP_LEN_0);
	rtsx_add_cmd(host, WRITE_REG_CMD, SD_TRANSFER, 0xFF,
		SD_TM_AUTO_WRITE_3 | SD_TRANSFER_START);
	rtsx_add_cmd(host, CHECK_REG_CMD, SD_TRANSFER,
		SD_TRANSFER_END, SD_TRANSFER_END);

	retval = rtsx_send_cmd(host, timeout);
	if (retval < 0) {
		mmc_error("%s, rtsx_send_cmd failed\n", __func__);
		MmcCommand temp_cmd = {
			.cmdidx = MMC_CMD_SEND_STATUS,
			.resp_type = MMC_RSP_R1,
			.cmdarg = host->sd_addr,
		};
		sd_send_cmd_get_rsp(host, &temp_cmd);

		return STATUS_FAIL;
	}

	return STATUS_SUCCESS;
}

static int sd_rw(RtkMmcHost *host, MmcCommand *cmd, MmcData *data)
{
	int retval, rsp_type;
	size_t datasize;
	u32 result32, loop = 0;
	u32 val = (u32)(1 << 31);
	u8 cfg2;

	datasize = data->blocksize * data->blocks;
	rsp_type = sd_response_type(cmd);
	if (rsp_type < 0)
		return STATUS_FAIL;

	if (data->flags & MMC_DATA_READ) {
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD0, 0xFF,
			0x40 | cmd->cmdidx);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD1, 0xFF,
			(u8)(cmd->cmdarg >> 24));
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD2, 0xFF,
			(u8)(cmd->cmdarg >> 16));
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD3, 0xFF,
			(u8)(cmd->cmdarg >> 8));
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CMD4, 0xFF,
			(u8)cmd->cmdarg);

		rtsx_add_cmd(host, WRITE_REG_CMD, SD_BYTE_CNT_L, 0xFF,
			(u8)data->blocksize);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_BYTE_CNT_H, 0xFF,
			(u8)(data->blocksize >> 8));
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_BLOCK_CNT_L, 0xFF,
			(u8)data->blocks);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_BLOCK_CNT_H, 0xFF,
			(u8)(data->blocks >> 8));

		rtsx_add_cmd(host, WRITE_REG_CMD, IRQSTAT0, DMA_DONE_INT,
			DMA_DONE_INT);
		rtsx_add_cmd(host, WRITE_REG_CMD, DMATC3, 0xFF,
			(u8)(datasize >> 24));
		rtsx_add_cmd(host, WRITE_REG_CMD, DMATC2, 0xFF,
			(u8)(datasize >> 16));
		rtsx_add_cmd(host, WRITE_REG_CMD, DMATC1, 0xFF,
			(u8)(datasize >> 8));
		rtsx_add_cmd(host, WRITE_REG_CMD, DMATC0, 0xFF,
			(u8)datasize);
		rtsx_add_cmd(host, WRITE_REG_CMD, DMACTL,
			0x03 | DMA_PACK_SIZE_MASK,
			DMA_DIR_FROM_CARD | DMA_EN | DMA_512);
		rtsx_add_cmd(host, WRITE_REG_CMD, CARD_DATA_SOURCE,
			0x01, RING_BUFFER);
		cfg2 = SD_CALCULATE_CRC7 | SD_CHECK_CRC16 |
			SD_NO_WAIT_BUSY_END | SD_CHECK_CRC7 | SD_RSP_LEN_6;
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CFG2, 0xFF, cfg2);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_TRANSFER, 0xFF,
			SD_TM_AUTO_READ_2 | SD_TRANSFER_START);
		rtsx_add_cmd(host, CHECK_REG_CMD, SD_TRANSFER,
			SD_TRANSFER_END, SD_TRANSFER_END);
		rtsx_send_cmd_no_wait(host);
	} else {
		retval = sd_send_cmd_get_rsp(host, cmd);
		if (retval != STATUS_SUCCESS)
			return STATUS_FAIL;

		rtsx_add_cmd(host, WRITE_REG_CMD, SD_BYTE_CNT_L, 0xFF,
			(u8)data->blocksize);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_BYTE_CNT_H, 0xFF,
			(u8)(data->blocksize >> 8));
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_BLOCK_CNT_L, 0xFF,
			(u8)data->blocks);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_BLOCK_CNT_H, 0xFF,
			(u8)(data->blocks >> 8));

		rtsx_add_cmd(host, WRITE_REG_CMD, IRQSTAT0, DMA_DONE_INT,
			DMA_DONE_INT);
		rtsx_add_cmd(host, WRITE_REG_CMD, DMATC3, 0xFF,
			(u8)(datasize >> 24));
		rtsx_add_cmd(host, WRITE_REG_CMD, DMATC2, 0xFF,
			(u8)(datasize >> 16));
		rtsx_add_cmd(host, WRITE_REG_CMD, DMATC1, 0xFF,
			(u8)(datasize >> 8));
		rtsx_add_cmd(host, WRITE_REG_CMD, DMATC0, 0xFF,
			(u8)datasize);
		rtsx_add_cmd(host, WRITE_REG_CMD, DMACTL,
			0x03 | DMA_PACK_SIZE_MASK,
			DMA_DIR_TO_CARD | DMA_EN | DMA_512);
		rtsx_add_cmd(host, WRITE_REG_CMD, CARD_DATA_SOURCE,
			0x01, RING_BUFFER);

		cfg2 = SD_NO_CALCULATE_CRC7 | SD_CHECK_CRC16 |
			SD_NO_WAIT_BUSY_END | SD_NO_CHECK_CRC7 | SD_RSP_LEN_0;
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CFG2, 0xFF, cfg2);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_TRANSFER, 0xFF,
			SD_TM_AUTO_WRITE_3 | SD_TRANSFER_START);
		rtsx_add_cmd(host, CHECK_REG_CMD, SD_TRANSFER,
			SD_TRANSFER_END, SD_TRANSFER_END);
		rtsx_send_cmd_no_wait(host);
	}

	mmc_debug("%s, data transfer\n", __func__);
	if (data->flags & MMC_DATA_READ) {
		val |= (u32)(DEVICE_TO_HOST & 0x01) << 29;
		val |= (u32)((data->blocks << 9) & 0x00FFFFFF);
		rtsx_writel(host, RTSX_HDBAR, (uintptr_t)data->dest);
	} else {
		val |= (u32)(HOST_TO_DEVICE & 0x01) << 29;
		val |= (u32)((data->blocks << 9) & 0x00FFFFFF);
		rtsx_writel(host, RTSX_HDBAR, (uintptr_t)data->src);
	}
	rtsx_writel(host, RTSX_HDBCTLR, val);

	do {
		result32 = rtsx_readl(host, RTSX_BIPR);
		loop++;
		if (loop > 1000) {
			retval = -1;
			break;
		}
		mdelay(1);
		mmc_debug("BIPR = 0x%x\n", result32);
	} while (!(result32 & TRANS_OK_INT));

	host->int_reg = result32 & 0xFFFFFFFF;
	rtsx_writel(host, RTSX_BIPR, host->int_reg | 0xFF);

	if (retval < 0) {
		mmc_error("data transfer failed\n");
		rtsx_stop_cmd(host);
		rtsx_write_register(host, CARD_STOP, SD_STOP | SD_CLR_ERR,
			SD_STOP | SD_CLR_ERR);
	}

	return retval;
}

static int rtk_send_command_bounced(MmcCtrlr *mmc_ctrl, MmcCommand *cmd,
				      MmcData *data,
				      struct bounce_buffer *bbstate)
{
	RtkMmcHost *host = container_of(mmc_ctrl, RtkMmcHost, mmc_ctrlr);
	size_t datasize = 0;
	int err;

	mmc_debug("%s called\n", __func__);
	mmc_debug("CMD %d, arg = 0x%x\n", cmd->cmdidx, cmd->cmdarg);

	if (data)
		datasize = data->blocksize*data->blocks;

	if (!datasize) {
		err = sd_send_cmd_get_rsp(host, cmd);
		if (cmd->cmdidx == MMC_CMD_SET_RELATIVE_ADDR)
			host->sd_addr = cmd->response[0] & 0xFFFF0000;
	} else {
		if ((cmd->cmdidx == MMC_CMD_READ_MULTIPLE_BLOCK) ||
			(cmd->cmdidx == MMC_CMD_WRITE_MULTIPLE_BLOCK)) {
			err = sd_rw(host, cmd, data);
		} else {
			if (data->flags & MMC_DATA_READ)
				err = sd_read_data(host, cmd, data);
			else
				err = sd_write_data(host, cmd, data);
		}
	}

	return err;
}

static int rtk_send_command(MmcCtrlr *mmc_ctrl, MmcCommand *cmd,
					MmcData *data)
{
	void *buf;
	unsigned int bbflags;
	size_t len;
	struct bounce_buffer *bbstate = NULL;
	struct bounce_buffer bbstate_val;
	int err;

	if (data) {
		if (data->flags & MMC_DATA_READ) {
			buf = data->dest;
			bbflags = GEN_BB_WRITE;
		} else {
			buf = (void *)data->src;
			bbflags = GEN_BB_READ;
		}

		len = data->blocks * data->blocksize;
		/*
		 * on some platform(like rk3399 etc) need to worry about
		 * cache coherency, so check the buffer, if not dma
		 * coherent, use bounce_buffer to do DMA management.
		 */
		if (!dma_coherent(buf)) {
			bbstate = &bbstate_val;
			if (bounce_buffer_start(bbstate, buf, len, bbflags)) {
				mmc_error("ERROR: get bounce buffer fail.\n");
				return -1;
			}
		}
	}

	err = rtk_send_command_bounced(mmc_ctrl, cmd, data, bbstate);

	if (bbstate)
		bounce_buffer_stop(bbstate);

	return err;
}

static int sd_set_timing(RtkMmcHost *host)
{
	int err = 0;
	int timeout = 100;

	mmc_debug("host->timing = %d\n", host->timing);

	switch (host->timing) {
	case MMC_TIMING_UHS_SDR104:
	case MMC_TIMING_UHS_SDR50:
		mmc_debug("SDR104/SDR50 mode\n");
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CFG1,
				0x0C | SD_ASYNC_FIFO_NOT_RST,
				SD_30_MODE | SD_ASYNC_FIFO_NOT_RST);
		rtsx_add_cmd(host, WRITE_REG_CMD, CLK_CTL,
				CLK_LOW_FREQ, CLK_LOW_FREQ);
		rtsx_add_cmd(host, WRITE_REG_CMD, CARD_CLK_SOURCE, 0xFF,
				CRC_VAR_CLK0 | SD30_FIX_CLK | SAMPLE_VAR_CLK1);
		rtsx_add_cmd(host, WRITE_REG_CMD, CLK_CTL, CLK_LOW_FREQ, 0);
		break;

	case MMC_TIMING_MMC_DDR52:
	case MMC_TIMING_UHS_DDR50:
		mmc_debug("DDR52/DDR50 mode\n");
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CFG1,
				0x0C | SD_ASYNC_FIFO_NOT_RST,
				SD_DDR_MODE | SD_ASYNC_FIFO_NOT_RST);
		rtsx_add_cmd(host, WRITE_REG_CMD, CLK_CTL,
				CLK_LOW_FREQ, CLK_LOW_FREQ);
		rtsx_add_cmd(host, WRITE_REG_CMD, CARD_CLK_SOURCE, 0xFF,
				CRC_VAR_CLK0 | SD30_FIX_CLK | SAMPLE_VAR_CLK1);
		rtsx_add_cmd(host, WRITE_REG_CMD, CLK_CTL, CLK_LOW_FREQ, 0);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_PUSH_POINT_CTL,
				DDR_VAR_TX_CMD_DAT, DDR_VAR_TX_CMD_DAT);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_SAMPLE_POINT_CTL,
				DDR_VAR_RX_DAT | DDR_VAR_RX_CMD,
				DDR_VAR_RX_DAT | DDR_VAR_RX_CMD);
		break;

	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
		mmc_debug("HS mode\n");
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_CFG1,
				0x0C, SD_20_MODE);
		rtsx_add_cmd(host, WRITE_REG_CMD, CLK_CTL,
				CLK_LOW_FREQ, CLK_LOW_FREQ);
		rtsx_add_cmd(host, WRITE_REG_CMD, CARD_CLK_SOURCE, 0xFF,
				CRC_FIX_CLK | SD30_VAR_CLK0 | SAMPLE_VAR_CLK1);
		rtsx_add_cmd(host, WRITE_REG_CMD, CLK_CTL, CLK_LOW_FREQ, 0);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_PUSH_POINT_CTL,
				SD20_TX_SEL_MASK, SD20_TX_14_AHEAD);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_SAMPLE_POINT_CTL,
				SD20_RX_SEL_MASK, SD20_RX_14_DELAY);
		break;

	default:
		mmc_debug("default mode\n");
		rtsx_add_cmd(host, WRITE_REG_CMD,
				SD_CFG1, 0x0C, SD_20_MODE);
		rtsx_add_cmd(host, WRITE_REG_CMD, CLK_CTL,
				CLK_LOW_FREQ, CLK_LOW_FREQ);
		rtsx_add_cmd(host, WRITE_REG_CMD, CARD_CLK_SOURCE, 0xFF,
				CRC_FIX_CLK | SD30_VAR_CLK0 | SAMPLE_VAR_CLK1);
		rtsx_add_cmd(host, WRITE_REG_CMD, CLK_CTL, CLK_LOW_FREQ, 0);
		rtsx_add_cmd(host, WRITE_REG_CMD,
				SD_PUSH_POINT_CTL, 0xFF, 0);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_SAMPLE_POINT_CTL,
				SD20_RX_SEL_MASK, SD20_RX_POS_EDGE);
		break;
	}

	err = rtsx_send_cmd(host, timeout);

	return err;
}

static inline u8 double_ssc_depth(u8 depth)
{
	return ((depth > 1) ? (depth - 1) : depth);
}

static u8 revise_ssc_depth(u8 ssc_depth, u8 div)
{
	if (div > CLK_DIV_1) {
		if (ssc_depth > (div - 1))
			ssc_depth -= (div - 1);
		else
			ssc_depth = SSC_DEPTH_4M;
	}

	return ssc_depth;
}

static int rtsx_switch_clock(RtkMmcHost *host, unsigned int card_clock,
		u8 ssc_depth, bool initial_mode, bool double_clk, bool vpclk)
{
	int err, clk, timeout;
	u8 n, clk_divider, mcu_cnt, div;

	timeout = 2000;
	if (initial_mode) {
		/* We use 250k(around) here, in initial stage */
		clk_divider = SD_CLK_DIVIDE_128;
		card_clock = 30000000;
	} else {
		clk_divider = SD_CLK_DIVIDE_0;
	}
	err = rtsx_write_register(host, SD_CFG1,
			SD_CLK_DIVIDE_MASK, clk_divider);
	if (err < 0)
		return err;

	card_clock /= 1000000;
	mmc_debug("Switch card clock to %dMHz\n", card_clock);

	clk = card_clock;
	if (!initial_mode && double_clk)
		clk = card_clock * 2;
	mmc_debug("Internal SSC clock: %dMHz (cur_clock = %d)\n",
		clk, host->cur_clock);

	if (clk == host->cur_clock)
		return 0;

	n = (u8)(clk - 2);

	mcu_cnt = (u8)(125 / clk + 3);
	if (mcu_cnt > 15)
		mcu_cnt = 15;

	/* Make sure that the SSC clock div_n is not less than MIN_DIV_N_PCR */
	div = CLK_DIV_1;
	while ((n < 80) && (div < CLK_DIV_8)) {
		n = (n + 2) * 2 - 2;
		div++;
	}
	mmc_debug("n = %d, div = %d\n", n, div);

	if (double_clk)
		ssc_depth = double_ssc_depth(ssc_depth);

	ssc_depth = revise_ssc_depth(ssc_depth, div);
	mmc_debug("ssc_depth = %d\n", ssc_depth);

	rtsx_add_cmd(host, WRITE_REG_CMD, CLK_CTL,
			CLK_LOW_FREQ, CLK_LOW_FREQ);
	rtsx_add_cmd(host, WRITE_REG_CMD, CLK_DIV,
			0xFF, (div << 4) | mcu_cnt);
	rtsx_add_cmd(host, WRITE_REG_CMD, SSC_CTL1, SSC_RSTB, 0);
	rtsx_add_cmd(host, WRITE_REG_CMD, SSC_CTL2,
			SSC_DEPTH_MASK, ssc_depth);
	rtsx_add_cmd(host, WRITE_REG_CMD, SSC_DIV_N_0, 0xFF, n);
	rtsx_add_cmd(host, WRITE_REG_CMD, SSC_CTL1, SSC_RSTB, SSC_RSTB);
	if (vpclk) {
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_VPCLK0_CTL,
				PHASE_NOT_RESET, 0);
		rtsx_add_cmd(host, WRITE_REG_CMD, SD_VPCLK0_CTL,
				PHASE_NOT_RESET, PHASE_NOT_RESET);
	}

	err = rtsx_send_cmd(host, timeout);
	if (err < 0) {
		mmc_error("%s, rtsx_send_cmd failed\n", __func__);
		return err;
	}

	/* Wait SSC clock stable */
	udelay(150);
	err = rtsx_write_register(host, CLK_CTL, CLK_LOW_FREQ, 0);
	if (err < 0)
		return err;

	host->cur_clock = clk;

	return 0;
}

static void rtk_set_ios(MmcCtrlr *mmc_ctrlr)
{
	RtkMmcHost *host = container_of(mmc_ctrlr,
						RtkMmcHost, mmc_ctrlr);

	mmc_debug("%s called\n", __func__);
	mmc_debug("Buswidth = %d, clock: %d\n", mmc_ctrlr->bus_width,
		mmc_ctrlr->bus_hz);

	switch (mmc_ctrlr->bus_width) {
	case 8:
		host->bus_width = SD_BUS_WIDTH_8;
		rtsx_write_register(host, SD_CFG1, 0x03, 0x2);
		break;
	case 4:
		host->bus_width = SD_BUS_WIDTH_4;
		rtsx_write_register(host, SD_CFG1, 0x03, 0x1);
		break;
	default:
		host->bus_width = SD_BUS_WIDTH_1;
		rtsx_write_register(host, SD_CFG1, 0x03, 0x0);
		break;
	}

	host->timing = mmc_ctrlr->timing;
	sd_set_timing(host);

	host->vpclk = false;
	host->double_clk = true;

	switch (mmc_ctrlr->timing) {
	case MMC_TIMING_UHS_SDR104:
	case MMC_TIMING_UHS_SDR50:
		host->ssc_depth = 0x02;
		host->vpclk = true;
		host->double_clk = false;
		break;
	case MMC_TIMING_MMC_DDR52:
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_UHS_SDR25:
		host->ssc_depth = 0x03;
		break;
	default:
		host->ssc_depth = 0x04;
		break;
	}

	host->initial_mode = (mmc_ctrlr->bus_hz <= 1000000) ? true : false;
	host->clock = mmc_ctrlr->bus_hz;
	rtsx_switch_clock(host, host->clock, host->ssc_depth,
			host->initial_mode, host->double_clk, host->vpclk);
}

static int sd_pull_ctl_disable(RtkMmcHost *host)
{
	int err, timeout;

	timeout = 100;
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PULL_CTL1, 0xFF, 0x66);
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PULL_CTL2, 0xFF, 0x55);
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PULL_CTL3, 0xFF, 0xD5);
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PULL_CTL4, 0xFF, 0x55);

	err = rtsx_send_cmd(host, timeout);
	if (err != 0)
		return STATUS_FAIL;

	return STATUS_SUCCESS;
}

static int sd_pull_ctl_enable(RtkMmcHost *host)
{
	int err, timeout;

	timeout = 100;
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PULL_CTL1, 0xFF, 0x66);
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PULL_CTL2, 0xFF, 0xAA);
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PULL_CTL3, 0xFF, 0xE9);
	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PULL_CTL4, 0xFF, 0xAA);

	err = rtsx_send_cmd(host, timeout);
	if (err != 0)
		return STATUS_FAIL;

	return STATUS_SUCCESS;
}

static int sd_power_on_card3v3(RtkMmcHost *host)
{
	int err, timeout;

	timeout = 100;
	if (CHK_PCI_PID(host, RTK_MMC_PID_525A))
		RTSX_WRITE_REG(host, LDO_VCC_CFG1, LDO_VCC_TUNE_MASK,
			LDO_VCC_3V3);

	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PWR_CTL,
		SD_POWER_MASK, SD_PARTIAL_POWER_ON);
	rtsx_add_cmd(host, WRITE_REG_CMD, PWR_GATE_CTRL,
		LDO3318_PWR_MASK, LDO_SUSPEND);
	err = rtsx_send_cmd(host, timeout);
	if (err != 0)
		return STATUS_FAIL;

	mdelay(RTK_WAIT_SD_POWER_ON_STABLE_MS);

	rtsx_add_cmd(host, WRITE_REG_CMD, CARD_PWR_CTL,
		SD_POWER_MASK, SD_POWER_ON);
	rtsx_add_cmd(host, WRITE_REG_CMD, PWR_GATE_CTRL,
		LDO3318_PWR_MASK, LDO_ON);
	err = rtsx_send_cmd(host, timeout);
	if (err != 0)
		return STATUS_FAIL;

	return STATUS_SUCCESS;
}

static int sd_power_off_card3v3(RtkMmcHost *host)
{
	int err;
	u8 clk_en = 0;

	clk_en |= SD_CLK_EN;
	RTSX_WRITE_REG(host, CARD_CLK_EN, clk_en, 0);

	RTSX_WRITE_REG(host, CARD_OE, SD_OUTPUT_EN, 0);

	RTSX_WRITE_REG(host, CARD_PWR_CTL, SD_POWER_MASK | PMOS_STRG_MASK,
					SD_POWER_OFF | PMOS_STRG_400mA);
	RTSX_WRITE_REG(host, PWR_GATE_CTRL, LDO3318_PWR_MASK, LDO_OFF);

	mdelay(RTK_WAIT_SD_POWER_DOWN_STABLE_MS);

	err = sd_pull_ctl_disable(host);
	if (err != 0)
		return STATUS_FAIL;

	return STATUS_SUCCESS;
}

static int sd_init_power(RtkMmcHost *host)
{
	int err;

	mmc_debug("%s called\n", __func__);

	err = sd_power_off_card3v3(host);
	if (err != 0)
		return STATUS_FAIL;

	/* Switch SD bus to 3V3 signal */
	switch (host->pid) {
	case RTK_MMC_PID_522A:
		rtsx_write_phy_register(host, 0x08, 0x57E4);
		break;
	case RTK_MMC_PID_525A:
		RTSX_WRITE_REG(host, 0xFF71, 0x07, 0x07);
		break;
	default:
		break;
	}

	u8 sd30_clk_drive_sel = 0x96;
	u8 sd30_cmd_drive_sel = 0x96;
	u8 sd30_dat_drive_sel = 0x96;

	RTSX_WRITE_REG(host, SD30_CLK_DRIVE_SEL, 0xFF, sd30_clk_drive_sel);
	RTSX_WRITE_REG(host, SD30_CMD_DRIVE_SEL, 0xFF, sd30_cmd_drive_sel);
	RTSX_WRITE_REG(host, SD30_DAT_DRIVE_SEL, 0xFF, sd30_dat_drive_sel);

	mdelay(RTK_WAIT_IC_STABLE_MS);

	RTSX_WRITE_REG(host, CARD_SELECT, 0x07, 0x02);
	RTSX_WRITE_REG(host, CARD_SHARE_MODE, CARD_SHARE_SD, CARD_SHARE_SD);
	RTSX_WRITE_REG(host, CARD_CLK_EN, SD_CLK_EN, SD_CLK_EN);

	err = sd_pull_ctl_enable(host);
	if (err != 0)
		return STATUS_FAIL;

	err = sd_power_on_card3v3(host);
	if (err != 0)
		return STATUS_FAIL;

	mdelay(RTK_WAIT_IC_STABLE_MS);

	RTSX_WRITE_REG(host, CARD_OE, SD_OUTPUT_EN, SD_OUTPUT_EN);

	/* For SD init sequence at least 74 clks before CMD0 */
	RTSX_WRITE_REG(host, SD_BUS_STAT, SD_CLK_TOGGLE_EN, SD_CLK_TOGGLE_EN);
	mdelay(RTK_WAIT_SD_POWER_ON_STABLE_MS);
	RTSX_WRITE_REG(host, SD_BUS_STAT, SD_CLK_TOGGLE_EN, 0);

	return STATUS_SUCCESS;
}

static int rts522a_sd_extra_init_hw(RtkMmcHost *host)
{
	/* Configure GPIO as output */
	RTSX_WRITE_REG(host, GPIO_CTL, 0x02, 0x02);
	/* Switch LDO3318 source from DV33 to card_3v3 */
	RTSX_WRITE_REG(host, LDO_PWR_SEL, LDO_SRC_SEL_MASK, LDO_SRC_NONE);
	RTSX_WRITE_REG(host, LDO_PWR_SEL, LDO_SRC_SEL_MASK, LDO_SRC_PMOS);
	/* LED shine disabled, set initial shine cycle period */
	RTSX_WRITE_REG(host, OLT_LED_CTL, 0x0F, 0x02);

	RTSX_WRITE_REG(host, PETXCFG, 0x30, 0x00);

	RTSX_WRITE_REG(host, PETXCFG, FORCE_CLKREQ_DELINK_MASK,
		FORCE_CLKREQ_HIGH);

	RTSX_WRITE_REG(host, PM_CTRL3, 0x10, 0x00);

	RTSX_WRITE_REG(host, FUNC_FORCE_CTL, 0x02, 0x02);
	RTSX_WRITE_REG(host, PCLK_CTL, 0x04, 0x04);
	RTSX_WRITE_REG(host, PM_EVENT_DEBUG, PME_DEBUG_0, PME_DEBUG_0);
	RTSX_WRITE_REG(host, PM_CLK_FORCE_CTL, 0xFF, 0x11);

	return 0;
}

static int rts525a_sd_extra_init_hw(RtkMmcHost *host)
{
	/* Rest L1SUB Config */
	RTSX_WRITE_REG(host, L1SUB_CONFIG3, 0xFF, 0x00);
	/* Configure GPIO as output */
	RTSX_WRITE_REG(host, GPIO_CTL, 0x02, 0x02);
	/* Switch LDO3318 source from DV33 to card_3v3 */
	RTSX_WRITE_REG(host, LDO_PWR_SEL, LDO_SRC_SEL_MASK, LDO_SRC_NONE);
	RTSX_WRITE_REG(host, LDO_PWR_SEL, LDO_SRC_SEL_MASK, LDO_SRC_PMOS);
	/* LED shine disabled, set initial shine cycle period */
	RTSX_WRITE_REG(host, OLT_LED_CTL, 0x0F, 0x02);

	RTSX_WRITE_REG(host, PETXCFG, 0xB0, 0x80);

	RTSX_WRITE_REG(host, REG_VREF, PWD_SUSPND_EN, PWD_SUSPND_EN);


	RTSX_WRITE_REG(host, PM_CTRL3, 0x01, 0x00);
	RTSX_WRITE_REG(host, 0xFF78, 0x30, 0x20);

	RTSX_WRITE_REG(host, PETXCFG, FORCE_CLKREQ_DELINK_MASK,
		FORCE_CLKREQ_HIGH);

	RTSX_WRITE_REG(host, PM_CTRL3, 0x10, 0x00);

	RTSX_WRITE_REG(host, RTS5250_CLK_CFG3, RTS525A_CFG_MEM_PD,
		RTS525A_CFG_MEM_PD);
	RTSX_WRITE_REG(host, PCLK_CTL, PCLK_MODE_SEL, PCLK_MODE_SEL);

	return 0;
}

static int rtk_init(BlockDevCtrlrOps *me)
{
	RtkMmcHost *host = container_of(me, RtkMmcHost, mmc_ctrlr.ctrlr.ops);
	pcidev_t dev = host->dev;
	int ret;
	uint8_t val;

	mmc_debug("%s called\n", __func__);

	host->host_cmds_ptr = dma_malloc(RTSX_RESV_BUF_LEN);
	host->ci = 0;

	RTSX_READ_REG(host, 0xFE90, &val);
	host->ic_version = val & 0x0F;
	mmc_debug("IC version 0x%x\n", host->ic_version);

	/* ack every tlp */
	if (CHK_PCI_PID(host, RTK_MMC_PID_525A))
		pci_write_config8(dev, 0x70C, 1);

	/* Power on SSC */
	RTSX_WRITE_REG(host, FPDCTL, SSC_POWER_DOWN, 0);
	udelay(200);

	/* disable ASPM */
	RTSX_WRITE_REG(host, ASPM_FORCE_CTL, 0x30, 0x30);

	RTSX_WRITE_REG(host, CLK_DIV, 0x07, 0x07);
	RTSX_WRITE_REG(host, HOST_SLEEP_STATE, 0x03, 0x00);
	RTSX_WRITE_REG(host, CARD_CLK_EN, 0x1E, 0);
	/* Reset delink mode */
	RTSX_WRITE_REG(host, CHANGE_LINK_STATE, 0x0A, 0);
	/*  Card driving select */
	RTSX_WRITE_REG(host, CARD_DRIVE_SEL, 0xFF, RTSX_CARD_DRIVE_DEFAULT);
	/* Enable SSC Clock */
	RTSX_WRITE_REG(host, SSC_CTL1, 0xFF, SSC_8X_EN | SSC_SEL_4M);
	RTSX_WRITE_REG(host, SSC_CTL2, 0xFF, 0x12);
	/* Disable cd_pwr_save */
	RTSX_WRITE_REG(host, CHANGE_LINK_STATE, 0x16, 0x10);
	/* Clear Link Ready Interrupt */
	RTSX_WRITE_REG(host, IRQSTAT0, LINK_RDY_INT, LINK_RDY_INT);
	/* Enlarge the estimation window of PERST# glitch
	 * to reduce the chance of invalid card interrupt
	 */
	RTSX_WRITE_REG(host, PERST_GLITCH_WIDTH, 0xFF, 0x80);
	/* Update RC oscillator to 400k
	 * bit[0] F_HIGH: for RC oscillator, Rst_value is 1'b1
	 *                1: 2M  0: 400k
	 */
	RTSX_WRITE_REG(host, RCCTL, 0x01, 0x00);
	/* Set interrupt write clear
	 * bit 1: U_elbi_if_rd_clr_en
	 *	1: Enable ELBI interrupt[31:22] & [7:0] flag read clear
	 *	0: ELBI interrupt flag[31:22] & [7:0] only can be write clear
	 */
	RTSX_WRITE_REG(host, NFTS_TX_CTRL, 0x02, 0x00);
	RTSX_WRITE_REG(host, PM_CLK_FORCE_CTL, 0x01, 0x01);

	if (CHK_PCI_PID(host, RTK_MMC_PID_525A))
		RTSX_WRITE_REG(host, SSC_DIV_N_0, 0xff, 0x5d);

	/* ocp setting */
	RTSX_WRITE_REG(host, FPDCTL, OC_POWER_DOWN, 0);
	RTSX_WRITE_REG(host, OCPPARA1, SD_OCP_TIME_MASK, SD_OCP_TIME_800);
	RTSX_WRITE_REG(host, OCPPARA2, SD_OCP_THD_MASK, host->sd_800mA_ocp_thd);
	RTSX_WRITE_REG(host, OCPGLITCH, SD_OCP_GLITCH_MASK, SD_OCP_GLITCH_10M);
	RTSX_WRITE_REG(host, OCPCTL, 0xFF, SD_OCP_INT_EN | SD_DETECT_EN);


	/* Enable clk_request_n to enable clock power management */
	pci_write_config8(dev, 0x81, 1);

	/* Enter L1 when host tx idle */
	ret = rtsx_write_cfg_dw(host, 0, 0x70C, 0xFF000000, 0x5B000000);
	if (ret)
		return ret;

	switch (host->pid) {
	case RTK_MMC_PID_522A:
		rts522a_sd_extra_init_hw(host);
		break;
	case RTK_MMC_PID_525A:
		rts525a_sd_extra_init_hw(host);
		break;
	default:
		break;
	}

	host->int_reg = rtsx_readl(host, RTSX_BIPR);

	if (host->int_reg & CARD_EXIST)
		RTSX_WRITE_REG(host, SSC_CTL1, SSC_RSTB, SSC_RSTB);

	/* Clear initial CARD_INT_PEND results */
	RTSX_WRITE_REG(host, CARD_INT_PEND, 0x0C, 0x0C);
	RTSX_WRITE_REG(host, PETXCFG, 0x08, 0x08);

	sd_init_power(host);

	return 0;
}

static int rtk_update(BlockDevCtrlrOps *me)
{
	RtkMmcHost *host = container_of
		(me, RtkMmcHost, mmc_ctrlr.ctrlr.ops);
	bool present;

	mmc_debug("%s called\n", __func__);

	if (!host->initialized && rtk_init(me))
		return -1;
	host->initialized = 1;

	if (host->mmc_ctrlr.slot_type == MMC_SLOT_TYPE_REMOVABLE) {
		present = (rtsx_readl(host, RTSX_BIPR) & SD_EXIST) != 0;
		if (!present) {
			if (host->mmc_ctrlr.media) {
				/*
				 * A card was present but isn't any more. Get
				 * rid of it.
				 */
				list_remove(&host->mmc_ctrlr.media->dev.list_node);
				free(host->mmc_ctrlr.media);
				host->mmc_ctrlr.media = NULL;
			}
			return 0;
		}

		if (!host->mmc_ctrlr.media) {
			/* A card is present and not set up yet.*/
			if (mmc_setup_media(&host->mmc_ctrlr))
				return -1;

			host->mmc_ctrlr.media->dev.name = "removable rtk_mmc";
			list_insert_after(&host->mmc_ctrlr.media->dev.list_node,
				&removable_block_devices);
		}
	} else {

		if (mmc_setup_media(&host->mmc_ctrlr))
			return -1;

		host->mmc_ctrlr.media->dev.name = "rtk_mmc";
		list_insert_after(&host->mmc_ctrlr.media->dev.list_node,
				&fixed_block_devices);
		host->mmc_ctrlr.ctrlr.need_update = 0;
	}

	host->mmc_ctrlr.media->dev.removable =
		host->mmc_ctrlr.slot_type == MMC_SLOT_TYPE_REMOVABLE;
	host->mmc_ctrlr.media->dev.ops.read = block_mmc_read;
	host->mmc_ctrlr.media->dev.ops.write = block_mmc_write;
	host->mmc_ctrlr.media->dev.ops.fill_write = block_mmc_fill_write;
	host->mmc_ctrlr.media->dev.ops.new_stream = new_simple_stream;
	host->mmc_ctrlr.media->dev.ops.get_health_info =
		block_mmc_get_health_info;

	return 0;
}

void add_rtkhost(RtkMmcHost *host)
{
	host->mmc_ctrlr.send_cmd = &rtk_send_command;
	host->mmc_ctrlr.set_ios = &rtk_set_ios;
	host->mmc_ctrlr.presets_enabled =
		!!(host->platform_info & RTKMMC_PLATFORM_VALID_PRESETS);

	host->mmc_ctrlr.ctrlr.ops.is_bdev_owned = block_mmc_is_bdev_owned;
	host->mmc_ctrlr.ctrlr.ops.update = &rtk_update;
	host->mmc_ctrlr.ctrlr.need_update = 1;

	host->mmc_ctrlr.f_min = 250000;
	host->mmc_ctrlr.f_max = 208000000;

	host->mmc_ctrlr.voltages = (MMC_VDD_32_33 | MMC_VDD_33_34);
	host->mmc_ctrlr.caps = MMC_CAPS_HS | MMC_CAPS_HS_52MHz |
			MMC_CAPS_4BIT | MMC_CAPS_HC;

	host->mmc_ctrlr.b_max = 65535;/* Some controllers use 16-bit regs. */

	assert(host->mmc_ctrlr.slot_type == MMC_SLOT_TYPE_EMBEDDED ||
		!(host->platform_info & RTK_PLATFORM_EMMC_HARDWIRED_VCC));
}

RtkMmcHost *probe_pci_rtk_host(pcidev_t dev, unsigned int platform_info)
{
	RtkPciHost *pci_host;
	int removable = platform_info & RTKMMC_PLATFORM_REMOVABLE;

	mmc_debug("%s called\n", __func__);

	pci_host = xzalloc(sizeof(*pci_host));

	pci_host->dev = dev;
	pci_host->host.platform_info = platform_info;

	pci_host->host.mmc_ctrlr.slot_type =
		removable ? MMC_SLOT_TYPE_REMOVABLE : MMC_SLOT_TYPE_EMBEDDED;

	if (!removable)
		pci_host->host.mmc_ctrlr.hardcoded_voltage =
			OCR_HCS | MMC_VDD_165_195 | MMC_VDD_27_28 |
			MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 |
			MMC_VDD_31_32 | MMC_VDD_32_33 | MMC_VDD_33_34 |
			MMC_VDD_34_35 | MMC_VDD_35_36;

	add_rtkhost(&pci_host->host);

	/* We temporarily replace the sdhci_update call with the
	 * rtkhost_pci_init call.
	 */
	pci_host->update = pci_host->host.mmc_ctrlr.ctrlr.ops.update;
	pci_host->host.mmc_ctrlr.ctrlr.ops.update = rtkhost_pci_init;

	/*
	 * We return RtkMmcHost because RtkPciHost is an implementation detail
	 */
	return &pci_host->host;
}
