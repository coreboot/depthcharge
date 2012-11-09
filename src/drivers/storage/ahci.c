/*
 * Copyright (C) Freescale Semiconductor, Inc. 2006.
 * Author: Jason Jin<Jason.jin@freescale.com>
 *         Zhang Wei<wei.zhang@freescale.com>
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * with the reference on libata and ahci drvier in kernel
 *
 */

#include <endian.h>
#include <libpayload.h>
#include <stdint.h>

#include "drivers/storage/ahci.h"
#include "drivers/storage/ata.h"
#include "drivers/storage/blockdev.h"

typedef struct AhciDevData {
	AhciHost *host;
	AhciIoPorts *port;
} AhciDevData;

#define writel_with_flush(a,b)	do { writel(a, b); readl(b); } while (0)

static void *ahci_port_base(void *base, int port)
{
	return (uint8_t *)base + 0x100 + (port * 0x80);
}

static void ahci_setup_port(AhciIoPorts *port, void *base, int idx)
{
	base = ahci_port_base(base, idx);

	port->cmd_addr = base;
	port->scr_addr = (uint8_t *)base + PORT_SCR;
}

#define WAIT_UNTIL(expr, timeout)				\
	({							\
		typeof(timeout) __counter = timeout * 1000;	\
		typeof(expr) __expr_val;			\
		while (!(__expr_val = (expr)) && __counter--)	\
			udelay(1);				\
		__expr_val;					\
	})

#define WAIT_WHILE(expr, timeout) !WAIT_UNTIL(!(expr), (timeout))

static const char *ahci_decode_speed(uint32_t speed)
{
	switch (speed) {
	case 1: return "1.5";
	case 2: return "3";
	case 3: return "6";
	default: return "?";
	}
}

static const char *ahci_decode_mode(uint32_t mode)
{
	switch (mode) {
	case 0x0101: return "IDE";
	case 0x0104: return "RAID";
	case 0x0106: return "SATA";
	default: return "unknown";
	}
}

static void ahci_print_info(AhciHost *host)
{
	pcidev_t pdev = host->dev;
	void *mmio = host->mmio_base;
	uint32_t cap, cap2;

	cap = host->cap;
	cap2 = readl(mmio + HOST_CAP2);

	uint8_t *vers = (uint8_t *)mmio + HOST_VERSION;
	printf("AHCI %02x%02x.%02x%02x", vers[3], vers[2], vers[1], vers[0]);
	printf(" %u slots", ((cap >> 8) & 0x1f) + 1);
	printf(" %u ports", (cap & 0x1f) + 1);
	uint32_t speed = (cap >> 20) & 0xf;
	printf(" %s Gbps", ahci_decode_speed(speed));
	printf(" %#x impl", host->port_map);
	uint32_t mode = pci_read_config16(pdev, 0xa);
	printf(" %s mode\n", ahci_decode_mode(mode));

	printf("flags: ");
	if (cap & (1 << 31)) printf("64bit ");
	if (cap & (1 << 30)) printf("ncq ");
	if (cap & (1 << 28)) printf("ilck ");
	if (cap & (1 << 27)) printf("stag ");
	if (cap & (1 << 26)) printf("pm ");
	if (cap & (1 << 25)) printf("led ");
	if (cap & (1 << 24)) printf("clo ");
	if (cap & (1 << 19)) printf("nz ");
	if (cap & (1 << 18)) printf("only ");
	if (cap & (1 << 17)) printf("pmp ");
	if (cap & (1 << 16)) printf("fbss ");
	if (cap & (1 << 15)) printf("pio ");
	if (cap & (1 << 14)) printf("slum ");
	if (cap & (1 << 13)) printf("part ");
	if (cap & (1 << 7)) printf("ccc ");
	if (cap & (1 << 6)) printf("ems ");
	if (cap & (1 << 5)) printf("sxs ");
	if (cap2 & (1 << 2)) printf("apst ");
	if (cap2 & (1 << 1)) printf("nvmp ");
	if (cap2 & (1 << 0)) printf("boh ");
	puts("\n");
}

static int ahci_host_init(AhciHost *host)
{
	pcidev_t pdev = host->dev;
	void *mmio = host->mmio_base;

	uint32_t cap_save = readl(mmio + HOST_CAP);
	cap_save &= ((1 << 28) | (1 << 17));
	cap_save |= (1 << 27);

	// Global controller reset.
	uint32_t host_ctl = readl(mmio + HOST_CTL);
	if ((host_ctl & HOST_RESET) == 0)
		writel_with_flush(host_ctl | HOST_RESET,
			(uintptr_t)mmio + HOST_CTL);

	// Reset must complete within 1 second.
	if (WAIT_WHILE((readl(mmio + HOST_CTL) & HOST_RESET), 1000)) {
		printf("Controller reset failed.\n");
		return -1;
	}

	writel_with_flush(HOST_AHCI_EN, mmio + HOST_CTL);
	writel(cap_save, mmio + HOST_CAP);
	writel_with_flush(0xf, mmio + HOST_PORTS_IMPL);

	uint16_t vendor = pci_read_config16(pdev, REG_VENDOR_ID);
	if (vendor == 0x8086) {
		pci_write_config16(pdev, 0x92,
			pci_read_config16(pdev, 0x92) | 0xf);
	}

	host->cap = readl(mmio + HOST_CAP);
	host->port_map = readl(mmio + HOST_PORTS_IMPL);
	host->n_ports = (host->cap & 0x1f) + 1;

	printf("cap %#x  port_map %#x  n_ports %d\n",
	      host->cap, host->port_map, host->n_ports);

	for (int i = 0; i < host->n_ports; i++) {
		host->port[i].port_mmio = ahci_port_base(mmio, i);
		uint8_t *port_mmio = (uint8_t *)host->port[i].port_mmio;
		ahci_setup_port(&host->port[i], mmio, i);

		/* make sure port is not active */
		uint32_t port_cmd = readl(port_mmio + PORT_CMD);
		uint32_t port_cmd_bits =
			PORT_CMD_LIST_ON | PORT_CMD_FIS_ON |
			PORT_CMD_FIS_RX | PORT_CMD_START;
		if (port_cmd & port_cmd_bits) {
			printf("Port %d is active. Deactivating.\n", i);
			port_cmd &= ~port_cmd_bits;
			writel_with_flush(port_cmd, port_mmio + PORT_CMD);

			/* spec says 500 msecs for each bit, so
			 * this is slightly incorrect.
			 */
			mdelay(500);
		}

		printf("Spinning up port %d... ", i);
		writel(PORT_CMD_SPIN_UP, port_mmio + PORT_CMD);
	}

	// Wait another 10ms to be sure communication has been established with
	// any devices.
	udelay(10000);

	for (int i = 0; i < host->n_ports; i++) {
		uint8_t *port_mmio = (uint8_t *)host->port[i].port_mmio;
		if ((readl(port_mmio + PORT_SCR_STAT) & 0xf) == 0x3)
			printf("ok.\n");
		else
			printf("communication not established.\n");

		uint32_t port_scr_err = readl(port_mmio + PORT_SCR_ERR);
		printf("PORT_SCR_ERR %#x\n", port_scr_err);
		writel(port_scr_err, port_mmio + PORT_SCR_ERR);

		/* ack any pending irq events for this port */
		uint32_t port_irq_stat = readl(port_mmio + PORT_IRQ_STAT);
		printf("PORT_IRQ_STAT 0x%x\n", port_irq_stat);
		if (port_irq_stat)
			writel(port_irq_stat, port_mmio + PORT_IRQ_STAT);

		writel(1 << i, mmio + HOST_IRQ_STAT);

		/* set irq mask (enables interrupts) */
		writel(DEF_PORT_IRQ, port_mmio + PORT_IRQ_MASK);

		/* register linkup ports */
		uint32_t port_scr_stat = readl(port_mmio + PORT_SCR_STAT);
		printf("Port %d status: 0x%x\n", i, port_scr_stat);
		if ((port_scr_stat & 0xf) == 0x3)
			host->link_port_map |= (0x1 << i);
	}

	host_ctl = readl(mmio + HOST_CTL);
	printf("HOST_CTL 0x%x\n", host_ctl);
	writel(host_ctl | HOST_IRQ_EN, mmio + HOST_CTL);
	host_ctl = readl(mmio + HOST_CTL);
	printf("HOST_CTL 0x%x\n", host_ctl);

	pci_write_config16(pdev, REG_COMMAND,
		pci_read_config16(pdev, REG_COMMAND) | REG_COMMAND_BM);

	return 0;
}

#define MAX_DATA_BYTE_COUNT  (4 * 1024 * 1024)

static int ahci_fill_sg(AhciSg *sg, void *buf, int len)
{
	uint32_t sg_count = ((len - 1) / MAX_DATA_BYTE_COUNT) + 1;
	if (sg_count > AHCI_MAX_SG) {
		printf("Error: Too much sg!\n");
		return -1;
	}

	for (int i = 0; i < sg_count; i++) {
		sg->addr = htolel((uintptr_t)buf + i * MAX_DATA_BYTE_COUNT);
		sg->addr_hi = 0;
		sg->flags_size =
			htolel(MIN(len, MAX_DATA_BYTE_COUNT - 1) & 0x3fffff);
		sg++;
		len -= MAX_DATA_BYTE_COUNT;
	}

	return sg_count;
}


static void ahci_fill_cmd_slot(AhciIoPorts *pp, uint32_t opts)
{
	pp->cmd_slot->opts = htolel(opts);
	pp->cmd_slot->status = 0;
	pp->cmd_slot->tbl_addr = htolel((uint32_t)(uintptr_t)pp->cmd_tbl);
	pp->cmd_slot->tbl_addr_hi = 0;
}


static int ahci_port_start(AhciIoPorts *port, int index)
{
	uint8_t *port_mmio = port->port_mmio;

	port->index = index;

	uint32_t status = readl(port_mmio + PORT_SCR_STAT);
	printf("Port %d status: %x\n", index, status);
	if ((status & 0xf) != 0x3) {
		printf("No link on this port!\n");
		return -1;
	}

	uint8_t *mem = memalign(2048, AHCI_PORT_PRIV_DMA_SZ);
	if (!mem) {
		printf("No mem for table!\n");
		return -1;
	}
	memset(mem, 0, AHCI_PORT_PRIV_DMA_SZ);

	/*
	 * First item in chunk of DMA memory: 32-slot command table,
	 * 32 bytes each in size
	 */
	port->cmd_slot = (AhciCommandHeader *)mem;
	mem += (AHCI_CMD_SLOT_SZ + 224);

	/*
	 * Second item: Received-FIS area
	 */
	port->rx_fis = mem;
	mem += AHCI_RX_FIS_SZ;

	/*
	 * Third item: data area for storing a single command
	 * and its scatter-gather table
	 */
	port->cmd_tbl = mem;
	mem += AHCI_CMD_TBL_HDR;

	port->cmd_tbl_sg = (AhciSg *)mem;

	writel_with_flush((uintptr_t)port->cmd_slot, port_mmio + PORT_LST_ADDR);

	writel_with_flush((uintptr_t)port->rx_fis, port_mmio + PORT_FIS_ADDR);

	writel_with_flush(PORT_CMD_ICC_ACTIVE | PORT_CMD_FIS_RX |
			  PORT_CMD_POWER_ON | PORT_CMD_SPIN_UP |
			  PORT_CMD_START, port_mmio + PORT_CMD);

	return 0;
}


static int ahci_device_data_io(AhciIoPorts *port, void *fis, int fis_len,
				void *buf, int buf_len, int is_write)
{

	uint8_t *port_mmio = port->port_mmio;

	uint32_t port_status = readl(port_mmio + PORT_SCR_STAT);
	if ((port_status & 0xf) != 0x3) {
		printf("No link on port %d!\n", port->index);
		return -1;
	}

	memcpy(port->cmd_tbl, fis, fis_len);

	int sg_count = ahci_fill_sg(port->cmd_tbl_sg, buf, buf_len);
	uint32_t opts = (fis_len >> 2) | (sg_count << 16) | (is_write << 6);
	ahci_fill_cmd_slot(port, opts);

	writel_with_flush(1, port_mmio + PORT_CMD_ISSUE);

	// Wait for the command to complete.
	if (WAIT_WHILE((readl(port_mmio + PORT_CMD_ISSUE) & 0x1), 150)) {
		printf("timeout exit!\n");
		return -1;
	}

	return 0;
}

/*
 * Some controllers limit number of blocks they can read/write at once.
 * Contemporary SSD devices work much faster if the read/write size is aligned
 * to a power of 2.  Let's set default to 128 and allowing to be overwritten if
 * needed.
 */
#ifndef MAX_SATA_BLOCKS_READ_WRITE
#define MAX_SATA_BLOCKS_READ_WRITE	0x80
#endif

static int ahci_read_write(BlockDev *dev, lba_t start, uint16_t count,
			   void *buf, int is_write)
{
	uint8_t fis[20];

	// Set up the FIS.
	memset(fis, 0, 20);
	fis[0] = 0x27;		 // Host to device FIS.
	fis[1] = 1 << 7;	 // Command FIS.
	// Command byte
	fis[2] = is_write ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA;

	AhciDevData *data = (AhciDevData *)dev->dev_data;
	AhciIoPorts *port = data->port;

	while (count) {
		uint16_t tblocks = MIN(MAX_SATA_BLOCKS_READ_WRITE, count);
		uintptr_t tsize = tblocks * dev->block_size;

		// LBA: only support LBA28 in this driver.
		fis[4] = (start >> 0) & 0xff;
		fis[5] = (start >> 8) & 0xff;
		fis[6] = (start >> 16) & 0xff;
		fis[7] = ((start >> 24) & 0xf) | 0xe0;

		// Block count.
		fis[12] = (tblocks >> 0) & 0xff;
		fis[13] = (tblocks >> 8) & 0xff;

		// Read/write from AHCI.
		if (ahci_device_data_io(port, fis, sizeof(fis), buf,
					tsize, is_write)) {
			printf("AHCI: %s command failed.\n",
			      is_write ? "write" : "read");
			return -1;
		}
		buf = (uint8_t *)buf + tsize;
		count -= tblocks;
		start += tblocks;
	}

	return 0;
}

static lba_t ahci_read(BlockDev *dev, lba_t start, lba_t count, void *buffer)
{
	if (ahci_read_write(dev, start, count, buffer, 0)) {
		printf("AHCI: Read failed.\n");
		return -1;
	}
	return count;
}

static lba_t ahci_write(BlockDev *dev, lba_t start, lba_t count,
			const void *buffer)
{
	if (ahci_read_write(dev, start, count, (void *)buffer, 1)) {
		printf("AHCI: Write failed.\n");
		return -1;
	}
	return count;
}

static int ahci_identify(AhciIoPorts *port, AtaIdentify *id)
{
	uint8_t fis[20];

	memset(fis, 0, 20);
	// Construct the FIS
	fis[0] = 0x27;		// Host to device FIS.
	fis[1] = 1 << 7;	// Command FIS.
	// Command byte.
	fis[2] = ATA_CMD_IDENTIFY_DEVICE;

	if (ahci_device_data_io(port, fis, 20, id, sizeof(AtaIdentify), 0)) {
		printf("AHCI: Identify command failed.\n");
		return -1;
	}
	printf("size of AtaIdentify is %d.\n", sizeof(AtaIdentify));

	return 0;
}

static int ahci_read_capacity(AhciIoPorts *port, lba_t *cap,
			      unsigned *block_size)
{
	AtaIdentify id;

	if (ahci_identify(port, &id))
		return -1;

	uint32_t cap32;
	memcpy(&cap32, &id.sectors28, sizeof(cap32));
	*cap = letohl(cap32);
	if (*cap == 0xfffffff) {
		memcpy(cap, id.sectors48, sizeof(*cap));
		*cap = letohll(*cap);
	}

	*block_size = 512;
	return 0;
}

void ahci_init(pcidev_t dev)
{
	AhciHost *host = malloc(sizeof(AhciHost));
	memset(host, 0, sizeof(AhciHost));
	host->dev = dev;

	host->mmio_base = (void *)pci_read_resource(dev, 5);
	printf("AHCI MMIO base = %p\n", host->mmio_base);

	// JMicron-specific fixup taken from kernel:
	// make sure we're in AHCI mode
	if (pci_read_config16(dev, REG_VENDOR_ID) == 0x197b)
		pci_write_config8(dev, 0x41, 0xa1);

	/* initialize adapter */
	if (ahci_host_init(host))
		return;

	ahci_print_info(host);

	uint32_t linkmap = host->link_port_map;

	for (int i = 0; i < sizeof(linkmap) * 8; i++) {
		if (((linkmap >> i) & 0x1)) {
			AhciIoPorts *port = &host->port[i];
			if (ahci_port_start(port, i)) {
				printf("Can not start port %d\n", i);
				continue;
			}
			lba_t cap;
			unsigned block_size;
			if (ahci_read_capacity(port, &cap, &block_size)) {
				printf("Can't read port %d's capacity.\n", i);
				continue;
			}

			BlockDev *sata_drive = malloc(sizeof(BlockDev));
			static const int name_size = 18;
			char *name = malloc(name_size);
			snprintf(name, name_size, "Sata port %d", i);
			sata_drive->name = name;
			sata_drive->read = &ahci_read;
			sata_drive->write = &ahci_write;
			AhciDevData *drive_data = malloc(sizeof(AhciDevData));
			drive_data->host = host;
			drive_data->port = port;
			sata_drive->dev_data = drive_data;
			sata_drive->block_size = block_size;
			sata_drive->block_count = cap;
			sata_drive->removable = 0;
			list_insert_after(&sata_drive->list_node,
					  &fixed_block_devices);
		}
	}
}
