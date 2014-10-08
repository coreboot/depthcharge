/*
 * Copyright (C) 1999-2003 David Woodhouse <dwmw2@infradead.org> et al.
 *
 * Released under GPL
 */

#ifndef __DRIVERS_STORAGE_MTD_MTD_H__
#define __DRIVERS_STORAGE_MTD_MTD_H__

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * struct mtd_ecc_stats - error correction stats
 *
 * @corrected:	number of corrected bits
 * @failed:	number of uncorrectable errors
 * @badblocks:	number of bad blocks in this partition
 * @bbtblocks:	number of blocks reserved for bad block tables
 */
struct mtd_ecc_stats {
	uint32_t corrected;
	uint32_t failed;
	uint32_t badblocks;
	uint32_t bbtblocks;
};

#define MTD_ERASE_PENDING	0x01
#define MTD_ERASING		0x02
#define MTD_ERASE_SUSPEND	0x04
#define MTD_ERASE_DONE          0x08
#define MTD_ERASE_FAILED        0x10

#define MTD_FAIL_ADDR_UNKNOWN	-1LL

/*
 * Certain error codes from the Linux kernel
 * These aren't shared with the kernel, so they don't have to be
 * kept in sync, but they are made with equal values for ease of
 * interpretation.
 */
#define	ENODEV		19	/* No such device */
#define	EBUSY		16	/* Device or resource busy */
#define	EBADMSG		74	/* Not a data message */
#define	EUCLEAN		117	/* Structure needs cleaning */
#define	EINVAL		22	/* Invalid argument */
#define	EROFS		30	/* Read-only file system */
#define	ENOMEM		12	/* Out of memory */
#define	EFAULT		14	/* Bad address */
#define	EIO		 5	/* I/O error */
#define	EPERM		 1	/* Operation not permitted */
#define	ETIMEDOUT	110	/* Connection timed out */
#define	ENOENT		 2	/* No such file or directory */
#define	ENOSYS		38	/* Function not implemented */


/* If the erase fails, fail_addr might indicate exactly which block failed.  If
 * fail_addr = MTD_FAIL_ADDR_UNKNOWN, the failure was not at the device level
 * or was not specific to any particular block. */
struct erase_info {
	uint64_t addr;
	uint64_t len;
	uint64_t fail_addr;
	int scrub;
};

/*
 * oob operation modes
 *
 * MTD_OOB_PLACE:	oob data are placed at the given offset
 * MTD_OOB_AUTO:	oob data are automatically placed at the free areas
 *			which are defined by the ecclayout
 * MTD_OOB_RAW:		mode to read raw data+oob in one chunk. The oob data
 *			is inserted into the data. Thats a raw image of the
 *			flash contents.
 */
typedef enum {
	MTD_OOB_PLACE,
	MTD_OOB_AUTO,
	MTD_OOB_RAW,
} mtd_oob_mode_t;

/**
 * struct mtd_oob_ops - oob operation operands
 * @mode:	operation mode
 *
 * @len:	number of data bytes to write/read
 *
 * @retlen:	number of data bytes written/read
 *
 * @ooblen:	number of oob bytes to write/read
 * @oobretlen:	number of oob bytes written/read
 * @ooboffs:	offset of oob data in the oob area (only relevant when
 *		mode = MTD_OOB_PLACE)
 * @datbuf:	data buffer - if NULL only oob data are read/written
 * @oobbuf:	oob data buffer
 *
 * Note, it is allowed to read more then one OOB area at one go, but not write.
 * The interface assumes that the OOB write requests program only one page's
 * OOB area.
 */
struct mtd_oob_ops {
	mtd_oob_mode_t	mode;
	size_t		len;
	size_t		retlen;
	size_t		ooblen;
	size_t		oobretlen;
	uint32_t	ooboffs;
	uint8_t		*datbuf;
	uint8_t		*oobbuf;
};

typedef struct MtdDev {
	unsigned char type;
	uint32_t flags;
	uint64_t size;	 /* Total size of the MTD */

	/* "Major" erase size for the device. Naïve users may take this
	 * to be the only erase size available, or may use the more detailed
	 * information below if they desire
	 */
	uint32_t erasesize;
	/* Minimal writable flash unit size. In case of NOR flash it is 1 (even
	 * though individual bits can be cleared), in case of NAND flash it is
	 * one NAND page (or half, or one-fourths of it), in case of ECC-ed NOR
	 * it is of ECC block size, etc. It is illegal to have writesize = 0.
	 * Any driver registering a MtdDev must ensure a writesize of
	 * 1 or larger.
	 */
	uint32_t writesize;

	uint32_t oobsize;   /* Amount of OOB data per block (e.g. 16) */
	uint32_t oobavail;  /* Available OOB bytes per block */

	/*
	 * Erase is an asynchronous operation.  Device drivers are supposed
	 * to call instr->callback() whenever the operation completes, even
	 * if it completes with a failure.
	 * Callers are supposed to pass a callback function and wait for it
	 * to be called before writing to the block.
	 */
	int (*erase) (struct MtdDev *mtd, struct erase_info *instr);


	int (*read) (struct MtdDev *mtd, uint64_t from, size_t len,
		     size_t *retlen, unsigned char *buf);
	int (*write) (struct MtdDev *mtd, uint64_t to, size_t len,
		      size_t *retlen, const unsigned char *buf);

	int (*read_oob) (struct MtdDev *mtd, uint64_t from,
			 struct mtd_oob_ops *ops);
	int (*write_oob) (struct MtdDev *mtd, uint64_t to,
			 struct mtd_oob_ops *ops);

	/* Bad block management functions */
	int (*block_isbad) (struct MtdDev *mtd, uint64_t ofs);
	int (*block_markbad) (struct MtdDev *mtd, uint64_t ofs);

	/* ECC status information */
	struct mtd_ecc_stats ecc_stats;

	void *priv;
} MtdDev;

typedef struct MtdDevCtrlr {
	MtdDev *dev;
	int (*update)(struct MtdDevCtrlr *me);
} MtdDevCtrlr;

#endif /* __DRIVERS_STORAGE_MTD_MTD_H__ */
