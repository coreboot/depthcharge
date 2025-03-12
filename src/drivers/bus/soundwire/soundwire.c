// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, Intel Corporation.
 * Copyright 2020 Google LLC.
 */

#include <arch/io.h>
#include <libpayload.h>

#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/timer/timer.h"

/* Soundwire message to read the endpoint device ID */
static const sndw_cmd mipi_sndw_read_endpointid_cmds[] = {
	{
		.cmdtype = cmd_read,
		.regaddr = SCP_ENDPOINT_ID_0,
	},
	{
		.cmdtype = cmd_read,
		.regaddr = SCP_ENDPOINT_ID_1,
	},
	{
		.cmdtype = cmd_read,
		.regaddr = SCP_ENDPOINT_ID_2,
	},
	{
		.cmdtype = cmd_read,
		.regaddr = SCP_ENDPOINT_ID_3,
	},
	{
		.cmdtype = cmd_read,
		.regaddr = SCP_ENDPOINT_ID_4,
	},
	{
		.cmdtype = cmd_read,
		.regaddr = SCP_ENDPOINT_ID_5,
	}
};

/*
 * poll_status - Function for polling the Status bit.
 * reg - The register address to read the status.
 * pollingmask - The bit mapping for polling.
 * pollingdata - The Data for polling.
 */
static int poll_status(void *reg, uint32_t pollingmask, uint32_t pollingdata)
{
	struct stopwatch sw;
	uint32_t data;

	stopwatch_init_usecs_expire(&sw, SNDW_POLL_TIME_US);
	do {
		data = read32(reg);
		if ((data & pollingmask) == pollingdata)
			return 0;
		udelay(SNDW_WAIT_PERIOD);
	} while (!stopwatch_expired(&sw));

	printf("Polling status bit failed.\n");
	return -1;
}

/*
 * printtxcmd - Print message for Sndw codec.
 * txcmd - Tx Sndw message.
 */
#if DEBUG_SNDW
static void printtxcmd(uint32_t txcmd)
{
	printf("Sndw Tx commands\n");
	printf("Tx:Ssp tag = %x\n",SNDW_EXTRACT_CMD_FIELD(TX, SSPTAG, txcmd));
	printf("Tx:Command type = %x\n",SNDW_EXTRACT_CMD_FIELD(TX, TYPE, txcmd));
	printf("Tx:Device address = %x\n",SNDW_EXTRACT_CMD_FIELD(TX, DEVADDR, txcmd));
	printf("Tx:Register address = %x\n",SNDW_EXTRACT_CMD_FIELD(TX, REGADDR, txcmd));
	printf("Tx:Register data = %x\n",SNDW_EXTRACT_CMD_FIELD(TX, REGDATA, txcmd));
}
#else
#define printtxcmd(...)
#endif

/*
 * printrxcmd - Print response message from Sndw codec.
 * rxcmd - Rx Sndw message.
 */
#if DEBUG_SNDW
static void printrxcmd(uint32_t rxcmd)
{
	printf("Sndw rx commands\n");
	printf("Rx:Register data = %x\n",SNDW_EXTRACT_CMD_FIELD(RX, REGDATA, rxcmd));
	printf("Rx:Command type = %x\n",SNDW_EXTRACT_CMD_FIELD(RX, TYPE, rxcmd));
	printf("Rx:nak = %x\n",SNDW_EXTRACT_CMD_FIELD(RX, NAK, rxcmd));
	printf("Rx:ack = %x\n",SNDW_EXTRACT_CMD_FIELD(RX, ACK, rxcmd));
}
#else
#define printrxcmd(...)
#endif

/*
 * send - Function operating on Sndw Fifo for sending message to codecs.
 * sndwlinkaddr - Soundwire controller link address.
 * txcmds - Pointer to send messages.
 * numofmsgs - Size of messages to send.
 */
static void send(void *sndwlinkaddr, sndw_cmd *txcmds, uint32_t devaddr, uint32_t numofmsgs)
{
	uint32_t fifofree, txmsg, txindex;

	fifofree = (read32(sndwlinkaddr + SNDW_MEM_FIFOSTAT) & SNDW_MEM_FIFOSTAT_FREE_MASK)
				>> SNDW_MEM_FIFOSTAT_FREE;

	for (txindex = 0; txindex < fifofree && txindex < numofmsgs; txindex++) {
		txmsg = SNDW_MAKE_CMD_FIELD(TX, REGDATA, txcmds[txindex].regdata) |
			SNDW_MAKE_CMD_FIELD(TX, REGADDR, txcmds[txindex].regaddr) |
			SNDW_MAKE_CMD_FIELD(TX, DEVADDR, devaddr) |
			SNDW_MAKE_CMD_FIELD(TX, TYPE,    txcmds[txindex].cmdtype) |
			SNDW_MAKE_CMD_FIELD(TX, SSPTAG,  txcmds[txindex].ssptag);
#if DEBUG_SNDW
		printtxcmd(txmsg);
#endif
		write32(sndwlinkaddr + SNDW_MEM_COMMAND, txmsg);
	}
}

/*
 * get_fifo_avail - This function returns number of available responses in the Response FIFO.
 */
static unsigned int get_fifo_avail(void *sndwlinkaddr)
{
	return (read32(sndwlinkaddr + SNDW_MEM_FIFOSTAT) & SNDW_MEM_FIFOSTAT_AVAIL_MASK)
			>> SNDW_MEM_FIFOSTAT_AVAIL;
}

/*
 * receive - Function operating on Sndw Fifo for receiving messages from codecs.
 * rxcmds - Pointer to pointer to received messages.
 * rxsize - Size of received messages.
 */
static int receive(void *sndwlinkaddr, uint32_t **rxcmds, uint32_t *rxsize)
{
	uint32_t fifoavailable, i, rxmsg, rxindex;
	rxindex = 0;

	fifoavailable = get_fifo_avail(sndwlinkaddr);

	if (fifoavailable == 0)
		return -1;

	*rxcmds = xzalloc(sizeof(uint32_t) * fifoavailable);

	for (i = 0; i < fifoavailable; i++) {
		rxmsg = read32(sndwlinkaddr + SNDW_MEM_COMMAND);
		mdelay(SNDW_WAIT_PERIOD);
#if DEBUG_SNDW
		printrxcmd(rxmsg);
#endif
		if (SNDW_EXTRACT_CMD_FIELD(RX, ACK, rxmsg) == 0) {
			printf(" Failed to recieve the ACK.\n");
			return -1;
		}
		(*rxcmds)[rxindex++] = rxmsg;
	}

	*rxsize = rxindex;

	/* Clear the RXNE interrupt. */
	write32(sndwlinkaddr + SNDW_MEM_INTMASK, SNDW_MEM_INTMASK_RXNE);

	return 0;
}

/*
 * read_endpointid - Function to get the identification information of connected codec.
 * sndwlinkaddr - Soundwire controller link address.
 * codecinfo - Pointer to the codec information.
 */
static int read_endpointid(void *sndwlinkaddr, uint32_t deviceindex,
							sndw_codec_info *codecinfo)
{
	uint32_t i, fifostatavail, count, rxsize;
	uint32_t *rxcmds;
	struct stopwatch sw;

	count = sizeof(mipi_sndw_read_endpointid_cmds) / sizeof(sndw_cmd);

	send(sndwlinkaddr, (sndw_cmd *)&mipi_sndw_read_endpointid_cmds, deviceindex, count);

	stopwatch_init_usecs_expire(&sw, SNDW_POLL_TIME_US);
	do {
		fifostatavail = get_fifo_avail(sndwlinkaddr);
		if (count == fifostatavail)
			break;
		else
			udelay(SNDW_WAIT_PERIOD);
	} while (!stopwatch_expired(&sw));

	if (count != fifostatavail) {
		printf("Response is not available for the txcmds\n");
		return -1;
	}

	if (receive(sndwlinkaddr, &rxcmds, &rxsize)) {
		printf("Failed to recieve the rx messages\n ");
		return -1;
	}

	for (i = 0; i < count; i++) {
		if (SNDW_EXTRACT_CMD_FIELD(RX, ACK, rxcmds[i]) == 1) {
			codecinfo->codecid.id[i] = SNDW_EXTRACT_CMD_FIELD(RX,
									REGDATA, rxcmds[i]);
		}
		else {
			printf("Failed to recieve the codec ID.\n");
			return -1;
		}
	}

	free(rxcmds);

	return 0;
}

/*
 * getsndwlinkaddress - Function to get the Soundwire link controller address.
 * bus - Pointer to the Soudnwire structure.
 */
static uint32_t getsndwlinkaddress(Soundwire *bus)
{
	uint32_t linkaddress;
	linkaddress = (read32(bus->dspbar + DSP_MEM_SNDW_SNDWIPPTR) & DSP_MEM_SNDW_SNDWIPPTR_PRT)
			+ (bus->sndwlinkindex * DSP_MEM_SNDW_OFFSETS);

	if (linkaddress == 0) {
		printf("Failed to get the Soundwire link controller address.\n");
		return -1;
	}

	return linkaddress;
}

/*
 * getnumofsndwlinks - Get the number of supported Soundwire links.
 * bus - Pointer to the Soudnwire structure.
 */
static uint32_t getnumofsndwlinks(Soundwire *bus)
{
	/* Get the number of the supported Soundwire links */
	uint32_t numofsndwlinks = ((read32(bus->dspbar + DSP_MEM_SNDW_SNDWLCAP)
				& DSP_MEM_SNDW_SNDWLCAP_SC) >> DSP_MEM_SNDW_SNDWLCAP_SC_BIT);
	return numofsndwlinks;
}

/*
 * sndwcodecstatus - Function to check the endpoint device status.
 * bus - Pointer to the Soundwire structure.
 * endpointstatus - Pointer to the endpoint device status.
 */
static uint32_t sndwcodecstatus(Soundwire *bus, uint32_t *endpointstatus)
{
	uint32_t status;
	struct stopwatch sw;
	uint32_t sndwlinkaddress = getsndwlinkaddress(bus);

	stopwatch_init_usecs_expire(&sw, SNDW_POLL_TIME_US);
	do {
		status = read32(bus->dspbar + sndwlinkaddress + SNDW_MEM_ENDPOINTSTAT);
		if (status != 0) {
			*endpointstatus = status;
			return 0;
		} else {
			udelay(SNDW_WAIT_PERIOD);
		}
	} while (!stopwatch_expired(&sw));

	printf("Sndw controller did not detect any codecs.\n");
	return -1;
}

/*
 * enable_sndwcodec - Function for codec enumeration process and read the codec info.
 * bus - Pointer to the Soundwire structure.
 * codecinfo - Pointer to the codec information.
 */
static int enable_sndwcodec(Soundwire *bus, sndw_codec_info *codecinfo)
{
	/* Soundwire codec initialization */
	uint32_t deviceindex, endpointstatus;

	/* Check the endpoint status */
	for (deviceindex = 0; deviceindex < SNDW_MAX_ENDPOINT_NUMBER; deviceindex++) {
		if (sndwcodecstatus(bus, &endpointstatus))
			return -1;

		if ((endpointstatus & SNDW_MEM_ENDPOINTSTAT_STATUS(deviceindex))
							== status_attached_ok) {
			udelay(SNDW_POLL_TIME_US);
			if (read_endpointid(bus->sndwlinkaddr,deviceindex,codecinfo)) {
				printf("Couldn't read the endpoint ID\n");
				return -1;
			}
			codecinfo->sndwlinkaddr = bus->sndwlinkaddr;
			codecinfo->deviceindex = deviceindex;
		}
	}

	return 0;
}

/*
 * sndwmasterinit - Function initializes Sndw master for enumerating connected codecs.
 * sndwlinkaddr - Soundwire link controller address.
 */
static int sndwmasterinit(void *sndwlinkaddr)
{
	/* Program MCP_ClockCtrl.Master Clock Divider to configure the Soundwire
	   clock frequency */
	write32(sndwlinkaddr + SNDW_MEM_CLK_CTRL0, CLK_DIVIDER);
	write32(sndwlinkaddr + SNDW_MEM_CLK_CTRL1, CLK_DIVIDER);

	/* Program MCP_Config.CMDMode to access the commands/responses via AHB mode */
	write32(sndwlinkaddr + SNDW_MEM_CONFIG,
		(read32(sndwlinkaddr + SNDW_MEM_CONFIG) & ~SNDW_MEM_CONFIG_MODE_AHB));

	/* Write 0 to MCP_Config.OperationMode to bring the master into normal mode */
	write32(sndwlinkaddr + SNDW_MEM_CONFIG,
		(read32(sndwlinkaddr + SNDW_MEM_CONFIG) & ~SNDW_MEM_CONFIG_OM_NORMAL));

	/* Write 0 to MCP_ConfigUpdate to update controller settings */
	write32(sndwlinkaddr + SNDW_MEM_CONFIGUPDATE,
		SNDW_MEM_CONFIGUPDATE_UPDATE_DONE);

	/* Waiting for FIFO to be ready to send MCP message */
	if (poll_status(sndwlinkaddr + SNDW_MEM_STAT, SNDW_MEM_STAT_TXE,
			SNDW_MEM_STAT_TXE_FIFO_EMPTY)) {
		printf("Soundwire controller not ready to send commands to codecs.\n");
		return -1;
	}

	return 0;
}

/*
 * sndwlink_poweron - Power-on the Soundwire link.
 * bus - Pointer to the Soudnwire structure.
 */
static int sndwlink_poweron(Soundwire *bus)
{
	uint32_t value;
	uint16_t rwval;

	/* Set SPA = 1 to power on the SoundWire link */
	value = DSP_MEM_SNDW_SNDWLCTL_SPA << bus->sndwlinkindex;
	write32(bus->dspbar + DSP_MEM_SNDW_SNDWLCTL,
		(read32(bus->dspbar + DSP_MEM_SNDW_SNDWLCTL) | value));

	value = DSP_MEM_SNDW_SNDWLCTL_CPA << bus->sndwlinkindex;
	/* Wait till current active power bit is set */
	for (int i = 0; i < RETRY_COUNT; i++) {
		if ((read32(bus->dspbar + DSP_MEM_SNDW_SNDWLCTL) & value) == value)
			break;
		mdelay(1);
	}

	if ((read32(bus->dspbar + DSP_MEM_SNDW_SNDWLCTL) & value) != value) {
		printf("Failed to poweron the Soundwire link.\n");
		return -1;
	}

	/*
	 * Enable Data AC Timing Qualifier in preparation for Master IP being
	 * put into "Normal" Operation
	 */
	rwval = read16(bus->dspbar + DSP_MEM_SNDW_SNDWxACTMCTL(bus->sndwlinkindex))
						| DSP_MEM_SNDW_SNDWxACTMCTL_DACTQE;
	write16(bus->dspbar + DSP_MEM_SNDW_SNDWxACTMCTL(bus->sndwlinkindex),
		rwval);

	/* Enable the Master IP Flowthrough */
	rwval = read16(bus->dspbar + DSP_MEM_SNDW_SNDWxIOCTL(bus->sndwlinkindex))
			| DSP_MEM_SNDW_SNDWxIOCTL_MIF;
	write16(bus->dspbar + DSP_MEM_SNDW_SNDWxIOCTL(bus->sndwlinkindex),
		rwval);

	return 0;
}

/*
 * sndwlink_init - Initialize the Soundwire link.
 * bus - Pointer to the Soudnwire structure.
 */
static int sndwlink_init(Soundwire *bus)
{
	uint32_t sndwlinkaddress;

	if (getnumofsndwlinks(bus) < bus->sndwlinkindex) {
		printf("Soundwire link %d doesn't exist.\n", bus->sndwlinkindex);
		return -1;
	}

	if (sndwlink_poweron(bus)) {
		printf("Failed to initialize the Sndw Link.\n");
		return -1;
	}

	sndwlinkaddress = getsndwlinkaddress(bus);
	bus->sndwlinkaddr = bus->dspbar + sndwlinkaddress;

	/* Function to check access to Sndw controller */
	if (read32((bus->sndwlinkaddr) + SNDW_MEM_CONFIG) == 0xffffffff) {
		printf("Failed to enable access to Sndw controller.\n");
		return -1;
	}

	if (sndwmasterinit(bus->sndwlinkaddr)) {
		printf("Failed to initialize the soundwire controller.\n");
		return -1;
	}

	return 0;
}

/*
 * ace_dsp_core_power_up - power up DSP subsystem and SDW IO domain
 * bus - Pointer to the Soudnwire structure.
 */
static int ace_dsp_core_power_up(Soundwire *bus)
{
	u32 val;

	/* Set the DSP subsystem power on */
	write32(bus->dspbar + MTL_HFDSSCS,
		(read32(bus->dspbar + MTL_HFDSSCS) | MTL_HFDSSCS_SPA_MASK));

	/* Poll: first wait for unstable CPA (1 then 0 then 1) after SPA set */
	for (int i = 0; i < RETRY_COUNT; i++) {
		mdelay(1);
		if ((read32(bus->dspbar + MTL_HFDSSCS) &
				MTL_HFDSSCS_CPA_MASK) == MTL_HFDSSCS_CPA_MASK)
			break;
	}
	if ((read32(bus->dspbar + MTL_HFDSSCS) &
			MTL_HFDSSCS_CPA_MASK) != MTL_HFDSSCS_CPA_MASK)
		return -1;

	/* Wake/Prevent gated-DSP0 & gated-IO1(SDW) from power gating */
	write32(bus->dspbar + MTL_HFPWRCTL,
		(read32(bus->dspbar + MTL_HFPWRCTL) | MTL_HFPWRCTL_WPHP0IO0_PG));

	for (int i = 0; i < RETRY_COUNT; i++) {
		if ((read32(bus->dspbar + MTL_HFPWRSTS) &
				MTL_HFPWRCTL_WPHP0IO0_PG) == MTL_HFPWRCTL_WPHP0IO0_PG)
			break;
		mdelay(1);
	}
	if ((read32(bus->dspbar + MTL_HFPWRSTS) &
			MTL_HFPWRCTL_WPHP0IO0_PG) != MTL_HFPWRCTL_WPHP0IO0_PG)
		return -1;

	/* Program Host CPU the owner of the IP & shim */
	val = read32(bus->dspbar + MTL_DSP2C0_CTL);
	write32(bus->dspbar + MTL_DSP2C0_CTL,
		(MTL_DSP2C0_OSEL_HOST(val) | MTL_DSP2C0_CTL_SPA_MASK));

	/* Poll: wait for unstable CPA (1 then 0 then 1) read first */
	for (int i = 0; i < RETRY_COUNT; i++) {
		mdelay(1);
		if ((read32(bus->dspbar + MTL_DSP2C0_CTL) &
			    MTL_DSP2C0_CTL_CPA_MASK) == MTL_DSP2C0_CTL_CPA_MASK)
			break;
	}

	if ((read32(bus->dspbar + MTL_DSP2C0_CTL) &
			MTL_DSP2C0_CTL_CPA_MASK) != MTL_DSP2C0_CTL_CPA_MASK)
		return -1;

	return 0;
}

/*
 * enable_hda_dsp - Function to reset the HDA and DSP
 * bus - Pointer to the Soudnwire structure
 */
static int enable_hda_dsp(Soundwire *bus)
{
	/* Set the CRST bit to get the HDA controller out of reset state */
	write32(bus->hdabar + HDA_MEM_GCTL, HDA_MEM_GCTL_CRST);
	for (int i = 0; i < RETRY_COUNT; i++) {
		if (read32(bus->hdabar + HDA_MEM_GCTL) == HDA_MEM_GCTL_CRST)
			break;
		mdelay(1);
	}

	if (read32(bus->hdabar + HDA_MEM_GCTL) != HDA_MEM_GCTL_CRST) {
		printf("HDA controller in reset state.\n");
		return -1;
	}

	/*  Enable Audio DSP for operation. */
	write32(bus->hdabar + HDA_MEM_PPCTL, HDA_MEM_PPCTL_DSPEN);
	for (int i = 0; i < RETRY_COUNT; i++) {
		if (read32(bus->hdabar + HDA_MEM_PPCTL) == HDA_MEM_PPCTL_DSPEN)
			break;
		mdelay(1);
	}

	if (read32(bus->hdabar + HDA_MEM_PPCTL) != HDA_MEM_PPCTL_DSPEN) {
		printf("Failed to enable ADSP for operation \n");
		return -1;
	}

	if (CONFIG(INTEL_COMMON_SOUNDWIRE_ACE_1_x)) {
		if (ace_dsp_core_power_up(bus)) {
			printf("Failed to power up ACE dsp core\n");
			return -1;
		}
	} else {
		/* Set power active to get DSP subsystem out of reset. */
		write32(bus->dspbar + DSP_MEM_ADSPCS,
			(read32(bus->dspbar + DSP_MEM_ADSPCS) | DSP_MEM_ADSPCS_SPA));
		/* Wait till current power active bit is set */
		for (int i = 0; i < RETRY_COUNT; i++) {
			if ((read32(bus->dspbar + DSP_MEM_ADSPCS) &
					DSP_MEM_ADSPCS_CPA) == DSP_MEM_ADSPCS_CPA)
				break;
			mdelay(1);
		}

		if ((read32(bus->dspbar + DSP_MEM_ADSPCS) &
				DSP_MEM_ADSPCS_CPA) != DSP_MEM_ADSPCS_CPA) {
			printf("Failed to get the ADSP out of reset \n");
			return -1;
		}
	}

	/* Enable the Host interrupt generation for SNDW interface */
	write32(bus->dspbar + DSP_MEM_ADSPIC2,
		(read32(bus->dspbar + DSP_MEM_ADSPIC2) | DSP_MEM_ADSPIC2_SNDW));

	return 0;
}

/*
 * sndw_enable - Enable HDA, DSP and Soundwire interfaces.
 * me - Pointer to the Soundwire ops.
 * codecinfo - Pointer to the codec information.
 */
static int sndw_enable(struct SndwOps *me, sndw_codec_info *codecinfo)
{
	Soundwire *sndwbus = container_of(me, Soundwire, ops);

	if (enable_hda_dsp(sndwbus)) {
		printf("Failed to reset HDA/DSP.\n");
		return -1;
	}

	if (sndwlink_init(sndwbus)) {
		printf("Failed to initialize the Soundwire link.\n");
		return -1;
	}

	if (enable_sndwcodec(sndwbus, codecinfo)) {
		printf("Failed to enable the Soundwire codec.\n");
		return -1;
	}

	return 0;
}

/*
 * sndw_sendwack -  Function sends one message through the Sndw interface and waits
 * for the corresponding ACK message.
 * sndwlinkaddr - Soundwire Link controller address.
 * txcmd - Message to send.
 * sndwindex - Index of the Soudnwire endpoint device.
 */
static int sndw_sendwack(void *sndwlinkaddr, sndw_cmd txcmd, uint32_t deviceindex)
{
	uint32_t rxsize;
	uint32_t *rxcmd;

	send(sndwlinkaddr, &txcmd, deviceindex, 1);

	if (poll_status(sndwlinkaddr + SNDW_MEM_STAT, SNDW_MEM_STAT_RXNE,
						SNDW_MEM_STAT_RXNE_FIFO_EMPTY)) {
		printf("Sending command failed. No response from codec.\n");
		return -1;
	}

	if (receive(sndwlinkaddr, &rxcmd, &rxsize)) {
		printf("Failed to recieve the rx messages\n ");
		return -1;
	}

	return 0;
}

/*
 * ace_dsp_core_power_down - Function to power down DSP subsystem
 * bus - Pointer to the Soudnwire structure
 */
static int ace_dsp_core_power_down(Soundwire *bus)
{
	/* disable SPA bit */
	write32(bus->dspbar + MTL_DSP2C0_CTL,
		(read32(bus->dspbar + MTL_DSP2C0_CTL) & ~MTL_DSP2C0_CTL_SPA_MASK));

	for (int i = 0; i < RETRY_COUNT; i++) {
		/* Wait for unstable CPA read (0 then 1 then 0) */
		mdelay(1);
		if (!(read32(bus->dspbar + MTL_DSP2C0_CTL) & MTL_DSP2C0_CTL_CPA_MASK))
			break;
	}
	if ((read32(bus->dspbar + MTL_DSP2C0_CTL) & MTL_DSP2C0_CTL_SPA_MASK)) {
		printf("%s : failed to power down primary core.\n", __func__);
		return -1;
	}

	/* Set the DSP subsystem to power down */
	write32(bus->dspbar + MTL_HFDSSCS,
		(read32(bus->dspbar + MTL_HFDSSCS) & ~MTL_HFDSSCS_SPA_MASK));

	for (int i = 0; i < RETRY_COUNT; i++) {
		mdelay(1);
		if (!(read32(bus->dspbar + MTL_HFDSSCS) & MTL_HFDSSCS_CPA_MASK))
			break;
	}
	if ((read32(bus->dspbar + MTL_HFDSSCS) & MTL_HFDSSCS_CPA_MASK)) {
		printf("%s : failed to disable DSP subsystem\n", __func__);
		return -1;
	}

	return 0;
}

/*
 * sndw_disable - Disable the soundwire interface by resetting the DSP.
 * bus - Pointer to the Soundwire structure.
 */
static int sndw_disable(SndwOps *me)
{
	Soundwire *bus = container_of(me, Soundwire, ops);

	if (CONFIG(INTEL_COMMON_SOUNDWIRE_ACE_1_x)) {
		if (ace_dsp_core_power_down(bus))
			return -1;
	} else {
		/* Set SPA=0 to reset ADSP subsystem. */
		write32(bus->dspbar + DSP_MEM_ADSPCS,
			(read32(bus->dspbar + DSP_MEM_ADSPCS) & ~DSP_MEM_ADSPCS_SPA));
		/* Wait till current power active bit is set */
		for (int i = 0; i < RETRY_COUNT; i++) {
			if ((read32(bus->dspbar + DSP_MEM_ADSPCS) &
					DSP_MEM_ADSPCS_CPA) == 0)
				break;
			mdelay(1);
		}
		if ((read32(bus->dspbar + DSP_MEM_ADSPCS) & DSP_MEM_ADSPCS_CPA) != 0) {
			printf("Failed to reset the ADSP.\n");
			return -1;
		}
	}

	return 0;
}

/*
 * new_sndw_structure - Allocate new Soundwire data structures.
 * Allocate new Soundwire data structures.
 */
Soundwire *new_soundwire(int sndwlinkindex)
{
	Soundwire *bus = xzalloc(sizeof(*bus));
	pcidev_t lpe_pcidev = PCI_DEV(0, AUDIO_DEV, AUDIO_FUNC);

	bus->ops.sndw_enable = &sndw_enable;
	bus->ops.sndw_sendwack = &sndw_sendwack;
	bus->ops.sndw_disable = &sndw_disable;
	bus->hdabar = (void *)(uintptr_t)(pci_read_config32(lpe_pcidev, REG_BAR0) & (~0xf));
	bus->dspbar = (void *)(uintptr_t)(pci_read_config32(lpe_pcidev, REG_BAR4) & (~0xf));
	bus->sndwlinkindex = sndwlinkindex;

	return bus;
}
