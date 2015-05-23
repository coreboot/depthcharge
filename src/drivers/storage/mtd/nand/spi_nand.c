/*
 * Copyright (C) 2014 Imagination Technologies
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 */

#include "nand.h"
#include "spi_nand.h"
#include "base/container_of.h"

/* SPI NAND commands */
#define	SPI_NAND_WRITE_ENABLE		0x06
#define	SPI_NAND_WRITE_DISABLE		0x04
#define	SPI_NAND_GET_FEATURE		0x0f
#define	SPI_NAND_SET_FEATURE		0x1f
#define	SPI_NAND_PAGE_READ		0x13
#define	SPI_NAND_READ_CACHE		0x03
#define	SPI_NAND_FAST_READ_CACHE	0x0b
#define	SPI_NAND_READ_CACHE_X2		0x3b
#define	SPI_NAND_READ_CACHE_X4		0x6b
#define	SPI_NAND_READ_CACHE_DUAL_IO	0xbb
#define	SPI_NAND_READ_CACHE_QUAD_IO	0xeb
#define	SPI_NAND_READ_ID		0x9f
#define	SPI_NAND_PROGRAM_LOAD		0x02
#define	SPI_NAND_PROGRAM_LOAD4		0x32
#define	SPI_NAND_PROGRAM_EXEC		0x10
#define	SPI_NAND_PROGRAM_LOAD_RANDOM	0x84
#define	SPI_NAND_PROGRAM_LOAD_RANDOM4	0xc4
#define	SPI_NAND_BLOCK_ERASE		0xd8
#define	SPI_NAND_RESET			0xff

/* Registers common to all devices */
#define SPI_NAND_LOCK_REG		0xa0
#define SPI_NAND_PROT_UNLOCK_ALL	0x0

#define SPI_NAND_FEATURE_REG		0xb0
#define SPI_NAND_ECC_EN			(1 << 4)

#define SPI_NAND_STATUS_REG		0xc0
#define SPI_NAND_STATUS_REG_ECC_MASK	0x3
#define SPI_NAND_STATUS_REG_ECC_SHIFT	4
#define SPI_NAND_STATUS_REG_PROG_FAIL	(1 << 3)
#define SPI_NAND_STATUS_REG_ERASE_FAIL	(1 << 2)
#define SPI_NAND_STATUS_REG_WREN	(1 << 1)
#define SPI_NAND_STATUS_REG_BUSY	(1 << 0)

#define SPI_NAND_GD5F_ECC_MASK		((1 << 0) | (1 << 1) | (1 << 2))
#define SPI_NAND_GD5F_ECC_UNCORR	((1 << 0) | (1 << 1) | (1 << 2))
#define SPI_NAND_GD5F_ECC_SHIFT		4

#define NAND_READY_TIMEOUT		100000 /* 1 SEC */

#define NAND_ID_MAN(id)			((id) & 0xFF)
#define NAND_ID_DEV(id)			(((id) >> 8) & 0xFF)
#define SPI_NAND_READID_LEN		2
#define MTD_SPI_NAND_DEV(mtd)		((struct spi_nand_dev *)((mtd)->priv))

/* Debug macros */
#define SNAND_DEBUG 0
#define snand_debug(...) do { if (SNAND_DEBUG) printf(__VA_ARGS__); } while (0)

/*
 * Given Depthcharge's MTD testing options are very limited, let's
 * add an option to poison the read buffers.
 */
/*#define SNAND_DEBUG_POISON_READ_BUFFER */
#define SNAND_POISON_BYTE 0x5a

struct spi_nand_flash_dev {
	char *name;
	uint8_t id;
	unsigned int pagesize;
	unsigned int chipsize;
	unsigned int erasesize;
	unsigned int oobsize;
};

struct spi_nand_cmd {
	/* Command and address. I/O errors have been observed if a
	 * separate spi_transfer is used for command and address,
	 * so keep them together
	 */
	uint32_t n_cmd;
	uint8_t cmd[4];

	/* Tx data */
	uint32_t n_tx;
	uint8_t *tx_buf;

	/* Rx data */
	uint32_t n_rx;
	uint8_t *rx_buf;
};

struct spi_nand_dev {
	SpiOps *spi;
	struct spi_nand_cmd cmd;

	unsigned char *buffers;
	unsigned char *pad_dat;
	unsigned char *pad_oob;
	unsigned char *zero_page;
	unsigned char *zero_oob;

	/* Fields from nand_chip */
	unsigned page_shift;
	unsigned phys_erase_shift;
};

static struct spi_nand_flash_dev spi_nand_flash_ids[] = {
	{
		.name = "SPI NAND 512MiB 3,3V",
		.id = 0xb4,
		.chipsize = 512,
		.pagesize = 4 * 1024,
		.erasesize = 256 * 1024,
		.oobsize = 256,
	},
	{
		.name = "SPI NAND 512MiB 1,8V",
		.id = 0xa4,
		.chipsize = 512,
		.pagesize = 4 * 1024,
		.erasesize = 256 * 1024,
		.oobsize = 256,
	},
};

static void spi_nand_debug_poison_buf(uint8_t *buf, unsigned int len)
{
#ifdef SNAND_DEBUG_POISON_READ_BUFFER
	memset(buf, SNAND_POISON_BYTE, len);
#endif
}

static int spi_nand_transfer(SpiOps *spi, struct spi_nand_cmd *cmd)
{
	if (spi->start(spi)) {
		printf("%s: failed to start flash transaction.\n", __func__);
		return -EIO;
	}

	if (!cmd->n_cmd) {
		printf("%s: failed to send empty command\n", __func__);
			return -EINVAL;
	}

	if (cmd->n_tx && cmd->n_rx) {
		printf("%s: cannot send and receive data at the same time\n",
			__func__);
		return -EINVAL;
	}

	/* Command and address */
	if (spi->transfer(spi, NULL, cmd->cmd, cmd->n_cmd)) {
		printf("%s: failed to send command and address\n", __func__);
		spi->stop(spi);
		return -EIO;
	}

	/* Data to be transmitted */
	if (cmd->n_tx && spi->transfer(spi, NULL, cmd->tx_buf, cmd->n_tx)) {
		printf("%s: failed to send data\n", __func__);
		spi->stop(spi);
		return -EIO;
	}

	/* Data to be received */
	if (cmd->n_rx && spi->transfer(spi, cmd->rx_buf, NULL, cmd->n_rx)) {
		printf("%s: failed to receive data\n", __func__);
		spi->stop(spi);
		return -EIO;
	}

	if (spi->stop(spi)) {
		printf("%s: Failed to stop transaction.\n", __func__);
		return -EIO;
	}

	return 0;
}

static int spi_nand_write_enable(struct spi_nand_dev *dev)
{
	struct spi_nand_cmd *cmd = &dev->cmd;

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 1;
	cmd->cmd[0] = SPI_NAND_WRITE_ENABLE;

	return spi_nand_transfer(dev->spi, cmd);
}

static int spi_nand_read_reg(struct spi_nand_dev *dev, uint8_t opcode,
			     uint8_t *buf)
{
	struct spi_nand_cmd *cmd = &dev->cmd;

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 2;
	cmd->cmd[0] = SPI_NAND_GET_FEATURE;
	cmd->cmd[1] = opcode;
	cmd->n_rx = 1;
	cmd->rx_buf = buf;

	snand_debug("read reg 0x%x\n", opcode);

	return spi_nand_transfer(dev->spi, cmd);
}

static int spi_nand_write_reg(struct spi_nand_dev *dev, uint8_t opcode,
			      uint8_t *buf)
{
	struct spi_nand_cmd *cmd = &dev->cmd;

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 2;
	cmd->cmd[0] = SPI_NAND_SET_FEATURE;
	cmd->cmd[1] = opcode;
	cmd->n_tx = 1;
	cmd->tx_buf = buf;

	snand_debug("write reg 0x%x\n", opcode);

	return spi_nand_transfer(dev->spi, cmd);
}

static int spi_nand_load_page(struct spi_nand_dev *dev, unsigned int page_addr)
{
	struct spi_nand_cmd *cmd = &dev->cmd;

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 4;
	cmd->cmd[0] = SPI_NAND_PAGE_READ;
	cmd->cmd[1] = (uint8_t)((page_addr & 0xff0000) >> 16);
	cmd->cmd[2] = (uint8_t)((page_addr & 0xff00) >> 8);
	cmd->cmd[3] = (uint8_t)(page_addr & 0xff);

	snand_debug("%s: page 0x%x\n", __func__, page_addr);

	return spi_nand_transfer(dev->spi, cmd);
}

static int spi_nand_read_cache(struct spi_nand_dev *dev,
			       unsigned int page_offset,
			       unsigned int length, uint8_t *read_buf)
{
	struct spi_nand_cmd *cmd = &dev->cmd;

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 4;
	cmd->cmd[0] = SPI_NAND_READ_CACHE;
	cmd->cmd[1] = 0;
	cmd->cmd[2] = (uint8_t)((page_offset & 0xff00) >> 8);
	cmd->cmd[3] = (uint8_t)(page_offset & 0xff);
	cmd->n_rx = length;
	cmd->rx_buf = read_buf;

	snand_debug("%s: %d bytes\n", __func__, length);

	return spi_nand_transfer(dev->spi, cmd);
}

static int spi_nand_write_from_cache(struct spi_nand_dev *dev,
				     unsigned int page_addr)
{
	struct spi_nand_cmd *cmd = &dev->cmd;

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 4;
	cmd->cmd[0] = SPI_NAND_PROGRAM_EXEC;
	cmd->cmd[1] = (uint8_t)((page_addr & 0xff0000) >> 16);
	cmd->cmd[2] = (uint8_t)((page_addr & 0xff00) >> 8);
	cmd->cmd[3] = (uint8_t)(page_addr & 0xff);

	snand_debug("%s: page 0x%x\n", __func__, page_addr);

	return spi_nand_transfer(dev->spi, cmd);
}

/*
 * The SPI_NAND_PROGRAM_LOAD is used to write to the chip page cache,
 * prior to programming the chip's page.
 * Notice that it supports an offset, which might be useful to write only
 * the OOB (e.g. in spi_nand_block_markbad).
 */
static int spi_nand_store_cache(struct spi_nand_dev *dev, unsigned int length,
				uint8_t *write_buf)
{
	struct spi_nand_cmd *cmd = &dev->cmd;

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 3;
	cmd->cmd[0] = SPI_NAND_PROGRAM_LOAD;
	cmd->cmd[1] = 0;
	cmd->cmd[2] = 0;
	cmd->n_tx = length;
	cmd->tx_buf = write_buf;

	snand_debug("%s: %d bytes\n", __func__, length);

	return spi_nand_transfer(dev->spi, cmd);
}

static int spi_nand_ecc_enable(struct spi_nand_dev *dev)
{
	int ret;
	uint8_t val;

	ret = spi_nand_read_reg(dev, SPI_NAND_FEATURE_REG, &val);
	if (ret < 0)
		return ret;

	val |= SPI_NAND_ECC_EN;
	ret = spi_nand_write_reg(dev, SPI_NAND_FEATURE_REG, &val);
	if (ret < 0)
		return ret;

	snand_debug("ecc enabled\n");

	return 0;
}

static int spi_nand_ecc_disable(struct spi_nand_dev *dev)
{
	int ret;
	uint8_t val;

	ret = spi_nand_read_reg(dev, SPI_NAND_FEATURE_REG, &val);
	if (ret < 0)
		return ret;

	val &= ~SPI_NAND_ECC_EN;
	ret = spi_nand_write_reg(dev, SPI_NAND_FEATURE_REG, &val);
	if (ret < 0)
		return ret;

	snand_debug("ecc disabled\n");

	return 0;
}

/*
 * Wait until the status register busy bit is cleared.
 * Returns a negatie errno on error or time out, and a non-negative status
 * value if the device is ready.
 */
static int spi_nand_wait_till_ready(struct spi_nand_dev *dev)
{
	uint8_t status;
	int ret, count = 0;

	while (count < NAND_READY_TIMEOUT) {
		ret = spi_nand_read_reg(dev, SPI_NAND_STATUS_REG, &status);
		if (ret < 0) {
			printf("%s: error reading status register\n", __func__);
			return ret;
		} else if (!(status & SPI_NAND_STATUS_REG_BUSY)) {
			return status;
		}

		udelay(10);
		count++;
	}

	printf("%s: operation timed out\n", __func__);

	return -ETIMEDOUT;
}

/* This is GD5F specific */
static void spi_nand_get_ecc_status(unsigned int status,
				    unsigned int *corrected,
				    unsigned int *ecc_error)
{
	unsigned int ecc_status = (status >> SPI_NAND_GD5F_ECC_SHIFT) &
					     SPI_NAND_GD5F_ECC_MASK;

	*ecc_error = (ecc_status == SPI_NAND_GD5F_ECC_UNCORR) ? 1 : 0;
	if (*ecc_error == 0)
		*corrected = 2 + ecc_status;
}


static int spi_nand_write_page(MtdDev *mtd, int pageno, unsigned int length,
			       uint8_t *write_buf)
{
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);
	int ret;

	/* Store the page to cache */
	ret = spi_nand_store_cache(dev, length, write_buf);
	if (ret < 0) {
		printf("spi_nand: error %d storing page %d to cache\n",
			ret, pageno);
		return ret;
	}

	ret = spi_nand_write_enable(dev);
	if (ret < 0) {
		printf("spi_nand: write enable on page program failed\n");
		return ret;
	}

	ret = spi_nand_write_from_cache(dev, pageno << dev->page_shift);
	if (ret < 0) {
		printf("spi_nand; error %d writing to page %d\n",
			ret, pageno);
		return ret;
	}

	ret = spi_nand_wait_till_ready(dev);
	if (ret < 0)
		return ret;
	if (ret & SPI_NAND_STATUS_REG_PROG_FAIL)
		return -EIO;

	return 0;
}

/*
 * Read a page worth of data and oob.
 */
static int spi_nand_read_page(MtdDev *mtd, int pageno,
			      unsigned int page_offset,
			      unsigned int length,
			      uint8_t *read_buf, int raw)
{
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);
	unsigned int corrected = 0, ecc_error = 0;
	int ret;

	/* Load a page into the cache register */
	ret = spi_nand_load_page(dev, pageno);
	if (ret < 0) {
		printf("spi_nand: error %d loading page %d to cache\n",
		       ret, pageno);
		return ret;
	}

	ret = spi_nand_wait_till_ready(dev);
	if (ret < 0)
		return ret;

	if (!raw) {
		spi_nand_get_ecc_status(ret, &corrected, &ecc_error);
		mtd->ecc_stats.corrected += corrected;

		/*
		 * If there's an ECC error, print a message and notify MTD
		 * about it. Then complete the read, to load actual data on
		 * the buffer (instead of the status result).
		 */
		if (ecc_error) {
			printf("spi_nand: ECC error reading page %d\n",
			       pageno);
			mtd->ecc_stats.failed++;
		}
	}

	/* Get page from the device cache into our internal buffer */
	ret = spi_nand_read_cache(dev, page_offset, length, read_buf);
	if (ret < 0) {
		printf("spi_nand: error %d reading page %d from cache\n",
		       ret, pageno);
		return ret;
	}
	return 0;
}

/*
 * Estimate the no. of pages to read, based on the data length and oob
 * length.
 */
static int spi_nand_get_read_page_count(MtdDev *mtd, struct mtd_oob_ops *ops)
{
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);

	if (ops->datbuf != NULL) {
		return (ops->len + mtd->writesize - 1) >> dev->page_shift;
	} else {
		if (mtd->oobsize == 0)
			return 0;

		return (ops->ooblen + mtd->oobsize - 1)
			/ mtd->oobsize;
	}
}

/*
 * Copy the OOB data from the internal buffer, to the user buffer, if
 * the internal buffer was used for the read.
 */
static void spi_nand_read_oobcopy(MtdDev *mtd, struct mtd_oob_ops *ops)
{
	unsigned int ooblen, read_ooblen;
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);

	if (ops->oobbuf == NULL)
		return;

	read_ooblen = ops->ooblen - ops->oobretlen;
	ooblen = MIN(read_ooblen, mtd->oobsize);
	memcpy(ops->oobbuf + ops->oobretlen, dev->pad_oob, ooblen);

	ops->oobretlen += ooblen;
}

/*
 * Copy the in-band data from the internal buffer, to the user buffer,
 * if the internal buffer was used for the read.
 */
static void spi_nand_read_datcopy(MtdDev *mtd, struct mtd_oob_ops *ops)
{
	unsigned int datlen, read_datlen;
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);

	if (ops->datbuf == NULL)
		return;

	read_datlen = ops->len - ops->retlen;
	datlen = MIN(read_datlen, mtd->writesize);
	memcpy(ops->datbuf + ops->retlen, dev->pad_dat, datlen);

	ops->retlen += datlen;
}

static int spi_nand_read_oob(MtdDev *mtd, uint64_t from,
			     struct mtd_oob_ops *ops)
{
	int start, pages, i;
	uint32_t corrected;
	unsigned read_len, read_off;
	uint8_t *read_buf;
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);
	int ret = 0;

	/* We don't support MTD_OOB_PLACE as of yet. */
	if (ops->mode == MTD_OOB_PLACE)
		return -ENOSYS;

	/* Check for reads past end of device */
	if (ops->datbuf && (from + ops->len) > mtd->size)
		return -EINVAL;

	if (from & (mtd->writesize - 1))
		return -EINVAL;

	if (ops->ooboffs != 0)
		return -EINVAL;

	if (ops->mode == MTD_OOB_RAW)
		spi_nand_ecc_disable(dev);

	start = from >> dev->page_shift;
	pages = spi_nand_get_read_page_count(mtd, ops);

	read_len = 0;
	if (ops->datbuf)
		read_len += mtd->writesize;
	if (ops->oobbuf)
		read_len += mtd->oobsize;

	if (ops->datbuf) {
		read_buf = dev->pad_dat;
		read_off = 0;
	} else {
		read_buf = dev->pad_oob;
		read_off = mtd->writesize;
	}

	snand_debug("Start of page: %d\n", start);
	snand_debug("No of pages to read: %d\n", pages);
	snand_debug("Page offset: 0x%x\n", read_off);
	snand_debug("Read size %u bytes\n", read_len);

	corrected = mtd->ecc_stats.corrected;

	for (i = start; i < (start + pages); i++) {

		spi_nand_debug_poison_buf(read_buf, read_len);

		/*
		 * Read into the internal buffers. Depending on the request
		 * this reads either: page data plus OOB on pad_dat, page data
		 * only on pad_dat, or OOB only on pad_oob.
		 */
		ret = spi_nand_read_page(mtd, i, read_off, read_len, read_buf,
					 (ops->mode == MTD_OOB_RAW));
		if (ret < 0)
			goto done;

		spi_nand_read_datcopy(mtd, ops);
		spi_nand_read_oobcopy(mtd, ops);
	}

	if (mtd->ecc_stats.corrected != corrected)
		ret = -EUCLEAN;

done:
	if (ops->mode == MTD_OOB_RAW)
		spi_nand_ecc_enable(dev);
	return ret;
}

static int spi_nand_read(MtdDev *mtd, uint64_t from, size_t len,
			 size_t *retlen, unsigned char *buf)
{
	int ret;
	struct mtd_oob_ops ops;

	ops.mode = MTD_OOB_AUTO;
	ops.len = len;
	ops.retlen = 0;
	ops.ooblen = 0;
	ops.oobretlen = 0;
	ops.ooboffs = 0;
	ops.datbuf = (uint8_t *)buf;
	ops.oobbuf = NULL;

	ret = spi_nand_read_oob(mtd, from, &ops);
	*retlen = ops.retlen;

	return ret;
}

static int spi_nand_write_oob(MtdDev *mtd, uint64_t to, struct mtd_oob_ops *ops)
{
	uint8_t *write_buf;
	unsigned int write_len;
	int i, start, pages, ret = 0;
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);

	/* We don't support MTD_OOB_PLACE as of yet. */
	if (ops->mode == MTD_OOB_PLACE)
		return -ENOSYS;

	/* Check for writes past end of device. */
	if ((to + ops->len) > mtd->size)
		return -EINVAL;

	if (to & (mtd->writesize - 1))
		return -EINVAL;

	if (ops->len & (mtd->writesize - 1))
		return -EINVAL;

	if (ops->ooboffs != 0)
		return -EINVAL;

	if (ops->datbuf == NULL)
		return -EINVAL;

	start = to >> dev->page_shift;
	pages = ops->len >> dev->page_shift;

	/* Only support a single page OOB write request */
	if (ops->oobbuf && pages > 1)
		return -EINVAL;
	/* Only support a contiguous buffer for data and OOB */
	if (ops->oobbuf && ops->oobbuf != (ops->datbuf + ops->len + 1))
		return -EINVAL;

	if (ops->mode == MTD_OOB_RAW)
		spi_nand_ecc_disable(dev);

	/*
	 * This currently supports two modes of writing:
	 * 1. Data only (no OOB), on a per-page basis.
	 * 2. Data plus OOB.
	 *
	 * Therefore, the write buffer always starts at the data buffer
	 * of the mtd_oob_ops. The write length is at least a chip page.
	 */
	write_buf = ops->datbuf;
	write_len = mtd->writesize;
	if (ops->oobbuf)
		write_len += mtd->oobsize;
	ops->retlen = 0;
	ops->oobretlen = 0;

	snand_debug("Start of page: %d\n", start);
	snand_debug("No of pages to write: %d\n", pages);

	for (i = start; i < (start + pages); i++) {

		ret = spi_nand_write_page(mtd, i, write_len, write_buf);
		if (ret < 0)
			goto done;

		ops->retlen += mtd->writesize;
		write_buf += mtd->writesize;

		if (ops->oobbuf) {
			ops->oobretlen += mtd->oobsize;
			write_buf += mtd->oobsize;
		}
	}

done:
	if (ops->mode == MTD_OOB_RAW)
		spi_nand_ecc_enable(dev);
	return ret;
}

static int spi_nand_write(MtdDev *mtd, uint64_t to, size_t len,
			  size_t *retlen, const unsigned char *buf)
{
	int ret;
	struct mtd_oob_ops ops;

	ops.mode = MTD_OOB_AUTO;
	ops.len = len;
	ops.retlen = 0;
	ops.ooblen = 0;
	ops.oobretlen = 0;
	ops.ooboffs = 0;
	ops.datbuf = (uint8_t *)buf;
	ops.oobbuf = NULL;

	ret = spi_nand_write_oob(mtd, to, &ops);
	*retlen = ops.retlen;

	return ret;
}

/* Read the first byte of the out-of-band region.
 * If this byte is clean 0xff, the block is good.
 * Otherwise, it's been marked as bad.
 */
static int spi_nand_block_isbad(MtdDev *mtd, uint64_t offs)
{
	int ret;
	uint8_t oobbuf;
	struct mtd_oob_ops ops;

	/* Check for out of bounds or unaligned offset */
	if ((offs > mtd->size) ||
	    (offs & (mtd->erasesize - 1)))
		return -EINVAL;

	ops.mode = MTD_OOB_RAW;
	ops.len = 0;
	ops.retlen = 0;
	ops.ooblen = 1;
	ops.oobretlen = 0;
	ops.ooboffs = 0;
	ops.datbuf = NULL;
	ops.oobbuf = &oobbuf;

	ret = spi_nand_read_oob(mtd, offs, &ops);
	if (ret < 0)
		return ret;

	return oobbuf != 0xFF;
}

static int spi_nand_block_markbad(MtdDev *mtd, uint64_t offs)
{
	int ret;
	struct mtd_oob_ops ops;
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);

	/* Check for invalid offset */
	if (offs > mtd->size)
		return -EINVAL;

	if (offs & (mtd->erasesize - 1))
		return -EINVAL;

	ops.mode = MTD_OOB_RAW;
	ops.len = mtd->writesize;
	ops.retlen = 0;
	ops.ooblen = mtd->oobsize;
	ops.oobretlen = 0;
	ops.ooboffs = 0;
	ops.datbuf = dev->zero_page;
	ops.oobbuf = dev->zero_oob;

	ret = spi_nand_write_oob(mtd, offs, &ops);
	if (!ret)
		mtd->ecc_stats.badblocks++;

	return ret;
}

/*
 * Erase the specified block.
 */
static int spi_nand_erase_block(MtdDev *mtd, int blockno)
{
	int ret;
	uint64_t offs;
	uint32_t page_addr;
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);
	struct spi_nand_cmd *cmd = &dev->cmd;

	offs = blockno << dev->phys_erase_shift;
	page_addr = offs >> dev->page_shift;

	snand_debug("erasing block %d (page 0x%x)\n", blockno, page_addr);

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 4;
	cmd->cmd[0] = SPI_NAND_BLOCK_ERASE;
	cmd->cmd[1] = (uint8_t)((page_addr & 0xff0000) >> 16);
	cmd->cmd[2] = (uint8_t)((page_addr & 0xff00) >> 8);
	cmd->cmd[3] = (uint8_t)(page_addr & 0xff);

	ret = spi_nand_transfer(dev->spi, cmd);
	if (ret < 0)
		return ret;

	ret = spi_nand_wait_till_ready(dev);
	if (ret < 0)
		return ret;
	if (ret & SPI_NAND_STATUS_REG_ERASE_FAIL)
		return -EIO;
	return 0;
}

static int spi_nand_erase(MtdDev *mtd, struct erase_info *instr)
{
	int i;
	int blocks;
	int start;
	uint64_t offs;
	int ret = 0;
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);

	/* Check for erase past end of device. */
	if ((instr->addr + instr->len) > mtd->size)
		return -EINVAL;

	if (instr->addr & (mtd->erasesize - 1))
		return -EINVAL;

	if (instr->len & (mtd->erasesize - 1))
		return -EINVAL;

	start = instr->addr >> dev->phys_erase_shift;
	blocks = instr->len >> dev->phys_erase_shift;
	snand_debug("number of blks to erase: %d\n", blocks);

	for (i = start; i < (start + blocks); i++) {
		offs = i << dev->phys_erase_shift;

		if (!instr->scrub && spi_nand_block_isbad(mtd, offs)) {
			printf("spi_nand: attempt to erase a bad block");
			return -EIO;
		}

		ret = spi_nand_write_enable(dev);
		if (ret < 0) {
			printf("%s: write enable on block %d erase failed\n",
			       __func__, i);
			return ret;
		}

		ret = spi_nand_erase_block(mtd, i);
		if (ret < 0) {
			instr->fail_addr = offs;
			break;
		}
	}

	return ret;
}

/*
 * Read the ID from the flash device.
 */
static int spi_nand_readid(struct spi_nand_dev *dev, uint32_t *id)
{
	struct spi_nand_cmd *cmd = &dev->cmd;

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 1;
	cmd->cmd[0] = SPI_NAND_READ_ID;
	cmd->n_rx = SPI_NAND_READID_LEN;
	cmd->rx_buf = (uint8_t *)id;

	return spi_nand_transfer(dev->spi, cmd);
}

/*
 * Retreive the flash info entry using the device ID.
 */
static const struct spi_nand_flash_dev *flash_get_dev(uint8_t dev_id)
{
	int i;

	for (i = 0; spi_nand_flash_ids[i].id; i++) {
		if (spi_nand_flash_ids[i].id == dev_id)
			return &spi_nand_flash_ids[i];
	}

	return NULL;
}

/*
 * Populate flash parameters for non-ONFI devices.
 */
static int nand_get_info(MtdDev *mtd, uint32_t flash_id)
{
	uint8_t man_id;
	uint8_t dev_id;
	const struct spi_nand_flash_dev *flash_dev;

	man_id = NAND_ID_MAN(flash_id);
	dev_id = NAND_ID_DEV(flash_id);

	printf("Manufacturer ID: %x\n", man_id);
	printf("Device ID: %x\n", dev_id);

	flash_dev = flash_get_dev(dev_id);
	if (!flash_dev) {
		printf("spi_nand: unknown NAND device!\n");
		return -ENOENT;
	}

	mtd->size = (uint64_t)flash_dev->chipsize * MiB;
	mtd->writesize = flash_dev->pagesize;
	mtd->erasesize = flash_dev->erasesize;
	mtd->oobsize = flash_dev->oobsize;

	return 0;
}

/*
 * Read the device ID, and populate the MTD callbacks and the device
 * parameters.
 */
int spi_nand_scan(MtdDev *mtd)
{
	int ret;
	uint32_t nand_id1 = 0;
	uint32_t nand_id2 = 0;
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);

	ret = spi_nand_readid(dev, &nand_id1);
	if (ret < 0)
		return ret;

	ret = spi_nand_readid(dev, &nand_id2);
	if (ret < 0)
		return ret;

	if (nand_id1 != nand_id2) {
		/*
		 * Bus-hold or other interface concerns can cause
		 * random data to appear. If the two results do not
		 * match, we are reading garbage.
		 */

		printf("spi_nand: device ID mismatch (0x%02x - 0x%02x)\n",
		       nand_id1, nand_id2);
		return -ENODEV;
	}

	ret = nand_get_info(mtd, nand_id1);
	if (ret < 0)
		return ret;

	mtd->erase = spi_nand_erase;
	mtd->read = spi_nand_read;
	mtd->write = spi_nand_write;
	mtd->read_oob = spi_nand_read_oob;
	mtd->write_oob = spi_nand_write_oob;
	mtd->block_isbad = spi_nand_block_isbad;
	mtd->block_markbad = spi_nand_block_markbad;

	dev->page_shift = __ffs(mtd->writesize);
	dev->phys_erase_shift = __ffs(mtd->erasesize);

	return 0;
}

/*
 * Setup the hardware and the driver state. Called after the scan and
 * is passed in the results of the scan.
 */
int spi_nand_post_scan_init(MtdDev *mtd)
{
	size_t alloc_size;
	struct spi_nand_dev *dev = MTD_SPI_NAND_DEV(mtd);
	uint8_t *buf;

	alloc_size = (mtd->writesize   /* For dev->pad_dat */
		      + mtd->oobsize   /* For dev->pad_oob */
		      + mtd->writesize /* For dev->zero_page */
		      + mtd->oobsize); /* For dev->zero_oob */

	dev->buffers = malloc(alloc_size);
	if (dev->buffers == NULL) {
		printf("spi_nand: failed to allocate memory\n");
		return -ENOMEM;
	}

	buf = dev->buffers;

	dev->pad_dat = buf;
	buf += mtd->writesize;

	dev->pad_oob = buf;
	buf += mtd->oobsize;

	dev->zero_page = buf;
	buf += mtd->writesize;

	dev->zero_oob = buf;
	buf += mtd->oobsize;

	memset(dev->zero_page, 0x0, mtd->writesize);
	memset(dev->zero_oob, 0x0, mtd->oobsize);

	printf("erase size: %d\n", mtd->erasesize);
	printf("page size: %d\n", mtd->writesize);
	printf("OOB size: %d\n", mtd->oobsize);

	return 0;
}

static int spi_nand_reset(struct spi_nand_dev *dev)
{
	struct spi_nand_cmd *cmd = &dev->cmd;
	int ret;

	memset(cmd, 0, sizeof(struct spi_nand_cmd));
	cmd->n_cmd = 1;
	cmd->cmd[0] = SPI_NAND_RESET;

	ret = spi_nand_transfer(dev->spi, cmd);
	if (ret < 0)
		return ret;

	ret = spi_nand_wait_till_ready(dev);
	if (ret < 0)
		return ret;
	return 0;
}

/*
 * Initialize controller and register as an MTD device.
 */
int spi_nand_init(SpiOps *spi, MtdDev **mtd_out)
{
	int ret;
	uint8_t val;
	MtdDev *mtd;
	struct spi_nand_dev *dev;

	printf("Initializing SPI NAND\n");

	mtd = xzalloc(sizeof(*mtd));
	dev = xzalloc(sizeof(*dev));

	dev->spi = spi;
	mtd->priv = dev;

	/* Reset Flash Memory */
	ret = spi_nand_reset(dev);
	if (ret < 0) {
		printf("spi_nand: flash reset timedout\n");
		return ret;
	}

	/* Identify the NAND device. */
	ret = spi_nand_scan(mtd);
	if (ret < 0) {
		printf("spi_nand: failed to identify device\n");
		return ret;
	}

	ret = spi_nand_post_scan_init(mtd);
	if (ret < 0)
		return ret;

	/*
	 * The device was detected so we need to unlock the entire device
	 * at this point to allow erase and write operations to work.
	 */
	val = SPI_NAND_PROT_UNLOCK_ALL;
	ret = spi_nand_write_reg(dev, SPI_NAND_LOCK_REG, &val);
	if (ret < 0)
		return ret;

	*mtd_out = mtd;
	return 0;
}

typedef struct {
	MtdDevCtrlr ctrlr;
	SpiOps *spiops;
} SpiNandDevCtrlr;

static int spi_nand_update(MtdDevCtrlr *ctrlr)
{
	if (ctrlr->dev)
		return 0;

	MtdDev *mtd;
	SpiNandDevCtrlr *spi_ctrlr = container_of(ctrlr,
						  SpiNandDevCtrlr, ctrlr);
	int ret = spi_nand_init(spi_ctrlr->spiops, &mtd);
	if (ret)
		return ret;
	ctrlr->dev = mtd;
	return 0;
}

/* External entrypoint for lazy NAND initialization */
MtdDevCtrlr *new_spi_nand(SpiOps *spiops)
{
	SpiNandDevCtrlr *ctrlr = xzalloc(sizeof(*ctrlr));
	ctrlr->ctrlr.update = spi_nand_update;
	ctrlr->spiops = spiops;
	return &ctrlr->ctrlr;
}
