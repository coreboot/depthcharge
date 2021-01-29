/*
 * Copyright (c) 2012 - 2014 The Linux Foundation. All rights reserved.
 */

#include <libpayload.h>
#include <arch/cache.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "drivers/storage/bouncebuf.h"

#include "ipq806x.h"
#include "drivers/gpio/ipq806x.h"
#include "drivers/net/nss/msm_ipq806x_gmac.h"
#include "drivers/net/athrs17_phy.h"
#include "board/storm/board.h"

#define PCS_QSGMII_MAC_STAT	0x74
#define MAX_FRAME_SIZE		1536
#define DESC_SIZE	(sizeof(ipq_gmac_desc_t))
#define DESC_FLUSH_SIZE	ALIGN_UP(sizeof(ipq_gmac_desc_t), get_cache_line_size())
#define ipq_info	printf
#define ipq_dbg		printf

static void *net_rx_packets[NO_OF_RX_DESC];

static int ipq_eth_start(IpqEthDev *priv);

/*
 * get_eth_mac_address:
 *
 * Retrieve MAC address from the sysinfo table.
 *
 * If the address is not set in the sysinfo table, set the first byte to 0x02
 * (a locally administered MAC address) and use the unique 64 bit value
 * provided by the SOC to calculate the remaining 5 bytes.
 *
 * Retrun 1, as nonzero return value indicates success.
 */
static int get_eth_mac_address(u8 *enetaddr)
{
	int i;
	int valid_mac = 0;

	for (i = 0; i < 6; i++) {
		enetaddr[i] = lib_sysinfo.macs[0].mac_addr[i];
		if (enetaddr[i])
			valid_mac = 1; /* Not all zeros */
	}

	if (!valid_mac) {
		uint8_t scramble[SHA1_DIGEST_LENGTH];
		uint32_t soc_unique[2];

		printf("Will use default MAC address\n");

		/* Read 8 bytes of HW provided unique value. */
		soc_unique[0] = read32(QFPROM_CORR_PTE_ROW0_LSB);
		soc_unique[1] = read32(QFPROM_CORR_PTE_ROW0_MSB);

		/* calculate its sha1. */
		sha1((const u8 *)soc_unique, sizeof(soc_unique), scramble);

		/*
		 * 0x02 is a good first byte of the locally administered MAC
		 * address (bit d1 in the first byte set to 1).
		 */
		enetaddr[0] = 0x02;

		/* the rest is derived from the SOC HW id. */
		memcpy(enetaddr + 1, scramble, 5);
	}

	return 1;
}

static void config_auto_neg(IpqEthDev *priv)
{
	ipq_mdio_write(priv->mdio_addr,
			PHY_CONTROL_REG,
			AUTO_NEG_ENABLE);
}

static int ipq_phy_check_link(NetDevice *dev, int *ready)
{
	IpqEthDev *priv = dev->dev_data;
	u16 phy_status;

	if (!priv->started) {
		ipq_eth_start(priv);
		udelay(1000);
	}

	if (ipq_mdio_read(priv->mdio_addr, PHY_SPECIFIC_STATUS_REG,
			  &phy_status))
		return 1;

	*ready = ((phy_status & Mii_phy_status_link_up) != 0);
	return 0;
}

static void get_phy_speed_duplexity(NetDevice *dev)
{
	IpqEthDev *priv = dev->dev_data;
	unsigned phy_status;
	u64 start;
	const u64 timeout = 2000 * 1000; /* in microseconds */

	start = timer_us(0);
	while (timer_us(start) < timeout) {

		phy_status = read32(QSGMII_REG_BASE + PCS_QSGMII_MAC_STAT);

		if (PCS_QSGMII_MAC_LINK(phy_status, priv->mac_unit)) {

			priv->speed =
			PCS_QSGMII_MAC_SPEED(phy_status,
			priv->mac_unit);

			priv->duplex =
			PCS_QSGMII_MAC_DUPLEX(phy_status,
			priv->mac_unit);

			if (priv->duplex)
				ipq_info("Full duplex link\n");
			else
				ipq_info("Half duplex link\n");

			ipq_info("Link %x up, Phy_status = %x\n",
			priv->mac_unit,phy_status);

			break;
		}

		udelay(10);
	}
}

static int ipq_eth_wr_macaddr(IpqEthDev *eth_dev )
{
	struct eth_mac_regs *mac_p = eth_dev->mac_regs_p;
	u32 macid_lo, macid_hi;
	u8 *mac_id = eth_dev->mac_addr.addr;

	macid_lo = mac_id[0] + (mac_id[1] << 8) +
		   (mac_id[2] << 16) + (mac_id[3] << 24);
	macid_hi = mac_id[4] + (mac_id[5] << 8);

	write32(&mac_p->macaddr0hi, macid_hi);
	write32(&mac_p->macaddr0lo, macid_lo);

	return 0;
}

static void ipq_mac_reset(IpqEthDev *eth_dev)
{
	struct eth_dma_regs *dma_reg = eth_dev->dma_regs_p;
	u32 val;

	write32(&dma_reg->busmode, DMAMAC_SRST);
	do {
		udelay(10);
		val = read32(&dma_reg->busmode);
	} while (val & DMAMAC_SRST);

}

static void ipq_eth_mac_cfg(IpqEthDev *priv)
{
	unsigned ipq_mac_cfg;
	struct eth_mac_regs *mac_reg = priv->mac_regs_p;

	if (priv->mac_unit > GMAC_UNIT1)
		ipq_mac_cfg = (priv->mac_ps | FULL_DUPLEX_ENABLE);
	else
		ipq_mac_cfg = (GMII_PORT_SELECT | FULL_DUPLEX_ENABLE);

	ipq_mac_cfg |= (FRAME_BURST_ENABLE | TX_ENABLE | RX_ENABLE);

	write32(&mac_reg->conf, ipq_mac_cfg);
}

static void ipq_eth_dma_cfg(IpqEthDev *priv)
{
	struct eth_dma_regs *dma_reg = priv->dma_regs_p;
	unsigned ipq_dma_bus_mode;
	unsigned ipq_dma_op_mode;

	ipq_dma_op_mode = DmaStoreAndForward | DmaRxThreshCtrl128 |
				DmaTxSecondFrame;
	ipq_dma_bus_mode = DmaFixedBurstEnable | DmaBurstLength16 |
				DmaDescriptorSkip0 | DmaDescriptor8Words |
				DmaArbitPr;

	write32(&dma_reg->busmode, ipq_dma_bus_mode);
	write32(&dma_reg->opmode, ipq_dma_op_mode);
}

static void ipq_eth_flw_cntl_cfg(IpqEthDev *priv)
{
	struct eth_mac_regs *mac_reg = priv->mac_regs_p;
	struct eth_dma_regs *dma_reg = priv->dma_regs_p;
	unsigned ipq_dma_flw_cntl;
	unsigned ipq_mac_flw_cntl;

	ipq_dma_flw_cntl = DmaRxFlowCtrlAct3K | DmaRxFlowCtrlDeact4K |
				DmaEnHwFlowCtrl;
	ipq_mac_flw_cntl = GmacRxFlowControl | GmacTxFlowControl | 0xFFFF0000;

	setbits_le32(&dma_reg->opmode, ipq_dma_flw_cntl);
	setbits_le32(&mac_reg->flowcontrol, ipq_mac_flw_cntl);
}

static int ipq_gmac_alloc_fifo(int ndesc, ipq_gmac_desc_t **fifo)
{
	int i;
	ipq_gmac_desc_t *p;

	p = xmemalign((get_cache_line_size()),
			(ndesc * DESC_FLUSH_SIZE));

	if (!p) {
		ipq_info("Cant allocate desc  fifos\n");
		return -1;
	}

	for (i = 0; i < ndesc; i++)
		fifo[i] = (ipq_gmac_desc_t *)((unsigned long)p +
					i * DESC_FLUSH_SIZE);
	return 0;
}

static int ipq_gmac_rx_desc_setup(IpqEthDev  *priv)
{
	struct eth_dma_regs *dma_reg = priv->dma_regs_p;
	ipq_gmac_desc_t *rxdesc;
	int i;

	for (i = 0; i < NO_OF_RX_DESC; i++) {
		void *p = xmemalign(get_cache_line_size(), MAX_FRAME_SIZE);

		if (!p)
			return -1;
		net_rx_packets[i] = p;

		rxdesc = priv->desc_rx[i];
		rxdesc->length |= ((ETH_MAX_FRAME_LEN << DescSize1Shift) &
					DescSize1Mask);
		rxdesc->buffer1 = (u32)p;
		rxdesc->data1 = (u32)priv->desc_rx[(i + 1) %
							NO_OF_RX_DESC];

		rxdesc->extstatus = 0;
		rxdesc->reserved1 = 0;
		rxdesc->timestamplow = 0;
		rxdesc->timestamphigh = 0;
		rxdesc->status = DescOwnByDma;

		dcache_clean_invalidate_by_mva((void const *)rxdesc,
			DESC_FLUSH_SIZE);
	}

	/* Assign Descriptor base address to dmadesclist addr reg */
	write32(&dma_reg->rxdesclistaddr, (unsigned)priv->desc_rx[0]);

	return 0;
}

static int ipq_gmac_tx_rx_desc_ring(IpqEthDev  *priv)
{
	int i;
	ipq_gmac_desc_t *desc;

	if (ipq_gmac_alloc_fifo(NO_OF_TX_DESC, priv->desc_tx))
		return -1;

	for (i = 0; i < NO_OF_TX_DESC; i++) {
		desc = priv->desc_tx[i];
		memset(desc, 0, sizeof(ipq_gmac_desc_t));
		desc->status =
		(i == (NO_OF_TX_DESC - 1)) ? TxDescEndOfRing : 0;

		desc->status |= TxDescChain;

		desc->data1 = (u32)priv->desc_tx[(i + 1) %
				NO_OF_TX_DESC];

		dcache_clean_invalidate_by_mva((void const *)desc,
			DESC_FLUSH_SIZE);

	}

	if (ipq_gmac_alloc_fifo(NO_OF_RX_DESC, priv->desc_rx))
		return -1;

	for (i = 0; i < NO_OF_RX_DESC; i++) {
		desc = priv->desc_rx[i];
		memset(desc, 0, sizeof(ipq_gmac_desc_t));
		desc->length =
		(i == (NO_OF_RX_DESC - 1)) ? RxDescEndOfRing : 0;

		desc->length |= RxDescChain;
		desc->data1 = (u32)priv->desc_rx[(i + 1) %
				NO_OF_RX_DESC];

		dcache_clean_invalidate_by_mva((void const *)desc,
			DESC_FLUSH_SIZE);

	}

	priv->next_tx = 0;
	priv->next_rx = 0;

	return 0;
}

static inline void ipq_gmac_give_to_dma(ipq_gmac_desc_t *fr)
{
	fr->status |= DescOwnByDma;
}

static inline u32 ipq_gmac_owned_by_dma(ipq_gmac_desc_t *fr)
{
	return fr->status & DescOwnByDma;
}

static inline u32 ipq_gmac_is_desc_empty(ipq_gmac_desc_t *fr)
{
	return ((fr->length & DescSize1Mask) == 0);
}

static int ipq_eth_start(IpqEthDev *priv)
{
	u32 data;
	struct eth_dma_regs *dma_reg = priv->dma_regs_p;

	priv->next_rx = 0;
	priv->next_tx = 0;

	ipq_mac_reset(priv);

	if ((priv->mac_unit == GMAC_UNIT2) || (priv->mac_unit == GMAC_UNIT3))
		config_auto_neg(priv);

	ipq_eth_wr_macaddr(priv);

	/* DMA, MAC configuration for Synopsys GMAC */
	ipq_eth_dma_cfg(priv);
	ipq_eth_mac_cfg(priv);
	ipq_eth_flw_cntl_cfg(priv);

	/* clear all pending interrupts if any */
	data = read32(&dma_reg->status);
	write32(&dma_reg->status, data);

	/* Setup Rx fifos and assign base address to */
	if (ipq_gmac_rx_desc_setup(priv))
		return -1;

	write32(&dma_reg->txdesclistaddr, (unsigned)priv->desc_tx[0]);
	setbits_le32(&dma_reg->opmode, (RXSTART));
	setbits_le32(&dma_reg->opmode, (TXSTART));

	priv->started = 1;

	return 0;
}

static int ipq_eth_send(NetDevice *dev, void *packet, u16 length)
{
	IpqEthDev *priv = dev->dev_data;
	struct eth_dma_regs *dma_p = priv->dma_regs_p;
	ipq_gmac_desc_t *txdesc = priv->desc_tx[priv->next_tx];
	int i = 0;

	dcache_invalidate_by_mva((void const *)txdesc, DESC_FLUSH_SIZE);

	/* Check if the dma descriptor is still owned by DMA */
	if (ipq_gmac_owned_by_dma(txdesc)) {
		ipq_info("BUG: Tx descriptor is owned by DMA %p\n", txdesc);
		return NETDEV_TX_BUSY;
	}

	txdesc->length |= ((length <<DescSize1Shift) & DescSize1Mask);
	txdesc->status |= (DescTxFirst | DescTxLast | DescTxIntEnable);
	txdesc->buffer1 = virt_to_phys(packet);
	ipq_gmac_give_to_dma(txdesc);

	dcache_clean_invalidate_by_mva((void const *)txdesc, DESC_FLUSH_SIZE);

	dcache_clean_invalidate_by_mva((void const *)(txdesc->buffer1), length);

	/* Start the transmission */
	write32(&dma_p->txpolldemand, POLL_DATA);

	for (i = 0; i < MAX_WAIT; i++) {
		udelay(10);
		dcache_invalidate_by_mva((void const *)txdesc, DESC_FLUSH_SIZE);
		if (!ipq_gmac_owned_by_dma(txdesc))
			break;
	}
	if (i == MAX_WAIT)
		ipq_info("Tx Timed out\n");

	/* reset the descriptors */
	txdesc->status = (priv->next_tx == (NO_OF_RX_DESC - 1)) ?
	TxDescEndOfRing : 0;
	txdesc->status |= TxDescChain;
	txdesc->length = 0;
	txdesc->buffer1 = 0;
	priv->next_tx = (priv->next_tx + 1) % NO_OF_TX_DESC;

	txdesc->data1 = (u32)priv->desc_tx[priv->next_tx];

	dcache_clean_invalidate_by_mva((void const *)txdesc, DESC_FLUSH_SIZE);

	return 0;
}

static int ipq_eth_recv(NetDevice *dev, void *buf, uint16_t *len, int maxlen)
{
	IpqEthDev *priv = dev->dev_data;
	struct eth_dma_regs *dma_p = priv->dma_regs_p;
	u16 length = 0;
	ipq_gmac_desc_t *rxdesc = priv->desc_rx[priv->next_rx];
	unsigned status;
	u8 *p_buf = (u8 *)buf;
	*len = 0;

	dcache_invalidate_by_mva((void const *)(priv->desc_rx[0]),
		NO_OF_RX_DESC * DESC_FLUSH_SIZE);

	for (rxdesc = priv->desc_rx[priv->next_rx];
		!ipq_gmac_owned_by_dma(rxdesc);
		rxdesc = priv->desc_rx[priv->next_rx]) {

		status = rxdesc->status;
		/*
		 TODO: Status shall be processed to determine error
		 return (non-zero) if required
		*/
		length = ((status & DescFrameLengthMask) >>
				DescFrameLengthShift);

		if ((*len + length) > maxlen)
			break;

		dcache_invalidate_by_mva(
			(void const *)(net_rx_packets[priv->next_rx]), length);

		if (((*len + length) <= maxlen) && length) {
			memcpy(p_buf,
				(void *)(net_rx_packets[priv->next_rx]),
				length);
			*len += length;
			p_buf += length;
		} else {
			if (!length)
				printf("received zero length frame.\n");
			else
				printf("\n%s:%d received frame too long (%d)\n",
					__func__, __LINE__, length);
		}

		rxdesc->length = ((ETH_MAX_FRAME_LEN << DescSize1Shift) &
					DescSize1Mask);
		rxdesc->length |= (priv->next_rx == (NO_OF_RX_DESC - 1)) ?
					RxDescEndOfRing : 0;
		rxdesc->length |= RxDescChain;
		rxdesc->buffer1 = (u32)net_rx_packets[priv->next_rx];
		priv->next_rx = (priv->next_rx + 1) % NO_OF_RX_DESC;
		rxdesc->data1 = (unsigned long)priv->desc_rx[priv->next_rx];

		rxdesc->extstatus = 0;
		rxdesc->reserved1 = 0;
		rxdesc->timestamplow = 0;
		rxdesc->timestamphigh = 0;
		rxdesc->status = DescOwnByDma;

		dcache_clean_invalidate_by_mva((void const *)rxdesc,
			DESC_FLUSH_SIZE);

		write32(&dma_p->rxpolldemand, POLL_DATA);
	}
	return 0;
}

static void gmac_sgmii_clk_init(unsigned mac_unit, unsigned clk_div,
	const ipq_gmac_board_cfg_t *gmac_cfg)
{
	unsigned gmac_ctl_val;
	unsigned nss_eth_clk_gate_val;

	gmac_ctl_val = (NSS_ETH_GMAC_PHY_INTF_SEL |
			NSS_ETH_GMAC_PHY_IFG_LIMIT |
			NSS_ETH_GMAC_PHY_IFG);

	nss_eth_clk_gate_val = (GMACn_GMII_RX_CLK(mac_unit) |
				GMACn_GMII_TX_CLK(mac_unit) |
				GMACn_PTP_CLK(mac_unit));

	write32((NSS_REG_BASE + NSS_GMACn_CTL(mac_unit)), gmac_ctl_val);

	switch (mac_unit) {
	case GMAC_UNIT1:
		setbits_le32((QSGMII_REG_BASE + PCS_ALL_CH_CTL),
				PCS_CHn_FORCE_SPEED(mac_unit));
		clrbits_le32((QSGMII_REG_BASE + PCS_ALL_CH_CTL),
				PCS_CHn_SPEED_MASK(mac_unit));
		setbits_le32((QSGMII_REG_BASE + PCS_ALL_CH_CTL),
				PCS_CHn_SPEED(mac_unit,
					PCS_CH_SPEED_1000));

		setbits_le32((NSS_REG_BASE + NSS_ETH_CLK_GATE_CTL),
			nss_eth_clk_gate_val);
		break;
	case GMAC_UNIT2:
	case GMAC_UNIT3:
		if (gmac_cfg->mac_conn_to_phy) {

			setbits_le32((QSGMII_REG_BASE + PCS_ALL_CH_CTL),
				(PCS_CHn_SPEED_FORCE_OUTSIDE(mac_unit) |
				PCS_DEBUG_SELECT));

			setbits_le32((NSS_REG_BASE + NSS_ETH_CLK_SRC_CTL),
				(1 << mac_unit));

			if (clk_div == 0) {
				clrbits_le32((NSS_REG_BASE +
					NSS_ETH_CLK_DIV0),
					(NSS_ETH_CLK_DIV(
					NSS_ETH_CLK_DIV_MASK,
					mac_unit)));
			} else {
				clrsetbits_le32((NSS_REG_BASE +
					NSS_ETH_CLK_DIV0),
					(NSS_ETH_CLK_DIV(
					NSS_ETH_CLK_DIV_MASK,
					mac_unit)),
					(NSS_ETH_CLK_DIV(clk_div,
					mac_unit)));
			}
			setbits_le32((NSS_REG_BASE + NSS_ETH_CLK_GATE_CTL),
					nss_eth_clk_gate_val);
		} else {
			/* this part of code forces the speed of MAC 2 to
			 * 1000Mbps disabling the Autoneg in case
			 * of AP148/DB147 since it is connected to switch
			 */
			setbits_le32((QSGMII_REG_BASE + PCS_ALL_CH_CTL),
				PCS_CHn_FORCE_SPEED(mac_unit));

			clrbits_le32((QSGMII_REG_BASE + PCS_ALL_CH_CTL),
				PCS_CHn_SPEED_MASK(mac_unit));

			setbits_le32((QSGMII_REG_BASE + PCS_ALL_CH_CTL),
				PCS_CHn_SPEED(mac_unit,
					PCS_CH_SPEED_1000));

			setbits_le32((NSS_REG_BASE + NSS_ETH_CLK_GATE_CTL),
				nss_eth_clk_gate_val);
		}
		break;
	}
}

static void ipq_gmac_mii_clk_init(IpqEthDev *priv, unsigned clk_div,
				  const ipq_gmac_board_cfg_t *gmac_cfg)
{
	u32 nss_gmac_ctl_val;
	u32 nss_eth_clk_gate_ctl_val;
	int gmac_idx = priv->mac_unit;
	u32 interface = priv->interface;

	switch (interface) {
	case PHY_INTERFACE_MODE_RGMII:
		nss_gmac_ctl_val = (GMAC_PHY_RGMII | GMAC_IFG |
				GMAC_IFG_LIMIT(GMAC_IFG));
		nss_eth_clk_gate_ctl_val =
			(GMACn_RGMII_RX_CLK(gmac_idx) |
			GMACn_RGMII_TX_CLK(gmac_idx) |
			GMACn_PTP_CLK(gmac_idx));
		setbits_le32((NSS_REG_BASE + NSS_GMACn_CTL(gmac_idx)),
				nss_gmac_ctl_val);
		setbits_le32((NSS_REG_BASE + NSS_ETH_CLK_GATE_CTL),
				nss_eth_clk_gate_ctl_val);
		setbits_le32((NSS_REG_BASE + NSS_ETH_CLK_SRC_CTL),
				(0x1 << gmac_idx));
		write32((NSS_REG_BASE + NSS_ETH_CLK_DIV0),
			(NSS_ETH_CLK_DIV(1, gmac_idx)));
		break;
	case PHY_INTERFACE_MODE_SGMII:
		gmac_sgmii_clk_init(gmac_idx, clk_div, gmac_cfg);
		break;
	default :
		ipq_info(" default : no rgmii sgmii for gmac %d\n", gmac_idx);
		return;
	}
}

static NetDevice *ipq_gmac_init(const ipq_gmac_board_cfg_t *gmac_cfg)
{
	static int s17_init_done = 0;
	NetDevice *dev;
	IpqEthDev *ipq_dev = 0;
	unsigned clk_div_val;
	int i;

	dev = xzalloc(sizeof(*dev));
	ipq_dev = xzalloc(sizeof(IpqEthDev));
	dev->dev_data = ipq_dev;

	/* Board is supposed to supply a MAC address. */
	if (!get_eth_mac_address(ipq_dev->mac_addr.addr))
		goto failed;

	ipq_info("Ipq806x MAC addr");
	for (i = 0; i < sizeof(ipq_dev->mac_addr.addr); i++)
		ipq_info("%c%2.2x", i ? ':' : ' ', ipq_dev->mac_addr.addr[i]);
	ipq_info("\n");

	ipq_dev->mac_unit = gmac_cfg->unit;
	ipq_dev->mac_regs_p = (struct eth_mac_regs *)(gmac_cfg->base);
	ipq_dev->dma_regs_p =
		(struct eth_dma_regs *)(gmac_cfg->base + DW_DMA_BASE_OFFSET);
	ipq_dev->interface = gmac_cfg->phy;
	ipq_dev->mdio_addr = gmac_cfg->mdio_addr;

		/* tx/rx Descriptor initialization */
	if (ipq_gmac_tx_rx_desc_ring(ipq_dev))
		goto failed;

	if ((gmac_cfg->unit == GMAC_UNIT2 ||
		gmac_cfg->unit == GMAC_UNIT3) &&
		(gmac_cfg->mac_conn_to_phy)) {

		get_phy_speed_duplexity(dev);

		switch (ipq_dev->speed) {
		case SPEED_1000M:
			ipq_info("Port:%d speed 1000Mbps\n",
				 gmac_cfg->unit);
			ipq_dev->mac_ps = GMII_PORT_SELECT;
			clk_div_val = (CLK_DIV_SGMII_1000M - 1);
			break;
		case SPEED_100M:
			ipq_info("Port:%d speed 100Mbps\n",
				 gmac_cfg->unit);
			ipq_dev->mac_ps = MII_PORT_SELECT;
			clk_div_val = (CLK_DIV_SGMII_100M - 1);
			break;
		case SPEED_10M:
			ipq_info("Port:%d speed 10Mbps\n",
				 gmac_cfg->unit);
			ipq_dev->mac_ps = MII_PORT_SELECT;
			clk_div_val = (CLK_DIV_SGMII_10M - 1);
			break;
		default:
			ipq_info("Port speed unknown\n");
			goto failed;
		}
	} else {
			/* Force it to zero for GMAC 0 & 1 */
		clk_div_val = 0;
	}

		ipq_gmac_mii_clk_init(ipq_dev, clk_div_val, gmac_cfg);

		if (!s17_init_done) {
			ipq_switch_init(gmac_cfg);
			s17_init_done = 1;
			ipq_info("S17 inits done\n");
		}

	return dev;

 failed:
	if (dev) {
		if (dev->dev_data)
			free(dev->dev_data);
		free(dev);
	}

	if (ipq_dev)
		free(ipq_dev);

	return NULL;
}

static void ipq_gmac_core_reset(const ipq_gmac_board_cfg_t *gmac_cfg)
{
	write32(GMAC_CORE_RESET(gmac_cfg->unit), 0);
	if (gmac_cfg->is_macsec)
		write32(GMACSEC_CORE_RESET(gmac_cfg->unit), 0);

	write32((void *)GMAC_AHB_RESET, 0);
	write32((MSM_TCSR_BASE + TCSR_PXO_SEL), 0);
}

unsigned ipq_mdio_read(unsigned phy_addr,
		       unsigned reg_offset, u16 *data)
{
	u8 *reg_base = NSS_GMAC0_BASE;
	int poll_period;
	u32 cycles;
	unsigned miiaddr;
	unsigned ret_val;

	miiaddr = (((phy_addr << MIIADDRSHIFT) & MII_ADDRMSK) |
				((reg_offset << MIIREGSHIFT) & MII_REGMSK));

	miiaddr |= (MII_BUSY | MII_CLKRANGE_250_300M);
	write32((reg_base + MII_ADDR_REG_ADDR), miiaddr);
	udelay(10);

	/* Convert to microseconds. */
	poll_period = 1000;
	cycles = (MII_MDIO_TIMEOUT * 1000) / poll_period;

	while (cycles--) {
		if (!(read32(reg_base + MII_ADDR_REG_ADDR) & MII_BUSY)) {
			ret_val = read32(reg_base + MII_DATA_REG_ADDR);
			*data = ret_val;
			return 0;
		}
		udelay(poll_period);
	}

	printf("%s:%d timeout!\n", __func__, __LINE__);
	return -1;
}

unsigned ipq_mdio_write(unsigned phy_addr, unsigned reg_offset, u16 data)
{
	u8 *reg_base = NSS_GMAC0_BASE;

	unsigned miiaddr;
	int poll_period;
	u32 cycles;

	write32((reg_base + MII_DATA_REG_ADDR), data);

	miiaddr = (((phy_addr << MIIADDRSHIFT) & MII_ADDRMSK) |
			((reg_offset << MIIREGSHIFT) & MII_REGMSK) |
			(MII_WRITE));

	miiaddr |= (MII_BUSY | MII_CLKRANGE_250_300M);
	write32((reg_base + MII_ADDR_REG_ADDR), miiaddr);
	udelay(10);

	poll_period = 1000;
	cycles = (MII_MDIO_TIMEOUT * 1000) / poll_period;

	while (cycles--) {
		if (!(read32(reg_base + MII_ADDR_REG_ADDR) & MII_BUSY))
			return 0;

		udelay(1000);
	}
	return -1;
}

void ipq_gmac_common_init(const ipq_gmac_board_cfg_t *gmac_cfg)
{
	unsigned pcs_qsgmii_ctl_val;
	unsigned pcs_mode_ctl_val;

	/* Take the switch out of reset */
	write32(GPIO_IN_OUT_ADDR(gmac_cfg->switch_reset_gpio), 0);

	pcs_mode_ctl_val = (PCS_CHn_ANEG_EN(GMAC_UNIT1) |
				PCS_CHn_ANEG_EN(GMAC_UNIT2) |
				PCS_CHn_ANEG_EN(GMAC_UNIT3) |
				PCS_CHn_ANEG_EN(GMAC_UNIT0) |
				PCS_SGMII_MAC);

	pcs_qsgmii_ctl_val = (PCS_QSGMII_ATHR_CSCO_AUTONEG |
				PCS_QSGMII_SW_VER_1_7 |
				PCS_QSGMII_SHORT_THRESH |
				PCS_QSGMII_SHORT_LATENCY |
				PCS_QSGMII_DEPTH_THRESH(1) |
				PCS_CHn_SERDES_SN_DETECT(0) |
				PCS_CHn_SERDES_SN_DETECT(1) |
				PCS_CHn_SERDES_SN_DETECT(2) |
				PCS_CHn_SERDES_SN_DETECT(3) |
				PCS_CHn_SERDES_SN_DETECT_2(0) |
				PCS_CHn_SERDES_SN_DETECT_2(1) |
				PCS_CHn_SERDES_SN_DETECT_2(2) |
				PCS_CHn_SERDES_SN_DETECT_2(3));

	write32((NSS_REG_BASE + NSS_MACSEC_CTL), MACSEC_BYPASS_EXT_EN);
	write32((QSGMII_REG_BASE + QSGMII_PHY_MODE_CTL), 0);
	write32((QSGMII_REG_BASE + PCS_QSGMII_SGMII_MODE), 0);

	setbits_le32((QSGMII_REG_BASE + NSS_PCS_MODE_CTL), pcs_mode_ctl_val);
	write32((QSGMII_REG_BASE + PCS_QSGMII_CTL), pcs_qsgmii_ctl_val);

	/*
	 * MDIO lines for all the MACs are connected through MAC0.
	 * Regardless of MAC 0 being used or not, it has to be pulled
	 * out of reset. Else, MDIO writes to configure other MACs
	 * will fail.
	 */
	write32(GMAC_CORE_RESET(0), 0);

	/*
	 * Pull out of reset the MACs that are applicable to the
	 * current board.
	 */
	ipq_gmac_core_reset(gmac_cfg);
}

void ipq_configure_gpio(const gpio_func_data_t *gpio, unsigned count)
{
	int i;

	for (i = 0; i < count; i++) {
		gpio_tlmm_config(gpio->gpio, gpio->func, gpio->dir,
				gpio->pull, gpio->drvstr, gpio->enable);
		gpio++;
	}
}

static const uip_eth_addr *ipq_get_mac(NetDevice *dev)
{
	IpqEthDev *priv = dev->dev_data;

	return &priv->mac_addr;
}

static ipq_gmac_board_cfg_t gmac_cfg = {
	.base		 = NSS_GMAC0_BASE,
	.phy		 = PHY_INTERFACE_MODE_RGMII,
	.switch_reset_gpio = 26 /* This in fact is board specific. */
};

const gpio_func_data_t gmac0_gpio[] = {
	{
		.gpio = 0,
		.func = 1,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 1,
		.func = 1,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_DISABLE
	},
	{
		.gpio = 2,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_8MA,
		.enable = GPIO_ENABLE
	},
	{
		.gpio = 66,
		.func = 0,
		.dir = GPIO_OUTPUT,
		.pull = GPIO_NO_PULL,
		.drvstr = GPIO_16MA,
		.enable = GPIO_ENABLE
	}
};

static int ipq_eth_init(void)
{
	NetDevice *ipq_network_device;

	/* Port number happens to match port's MDIO address. */
	gmac_cfg.mdio_addr = board_wan_port_number();
	ipq_info("Using port #%d\n", gmac_cfg.mdio_addr);

	ipq_gmac_common_init(&gmac_cfg);
	ipq_configure_gpio(gmac0_gpio, ARRAY_SIZE(gmac0_gpio));
	ipq_network_device = ipq_gmac_init(&gmac_cfg);

	if (!ipq_network_device)
		return -1;

	ipq_network_device->ready = ipq_phy_check_link;
	ipq_network_device->recv = ipq_eth_recv;
	ipq_network_device->send = ipq_eth_send;
	ipq_network_device->get_mac = ipq_get_mac;

	net_add_device(ipq_network_device);

	return 0;
}

static void ipq_net_poller(struct NetPoller *poller)
{
	static int initted;
	if (!initted && !ipq_eth_init())
		initted = 1;
}

static NetPoller net_poller = {
	.poll = ipq_net_poller
};

static int ipq_eth_driver_register(void)
{
	list_insert_after(&net_poller.list_node, &net_pollers);

	return 0;
}

INIT_FUNC(ipq_eth_driver_register);
