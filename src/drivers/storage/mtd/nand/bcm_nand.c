/*
 * Copyright (C) 2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "nand.h"
#include "bcm_nand.h"

/***********************************************************************
 * Definitions
 ***********************************************************************/
#define DRV_NAME			"bcm_nand"
#ifdef NAND_DEBUG
#define debug(args...)			printf(args)
#else
#define debug(args...)
#endif

#define CMD_TIMEOUT_US			200000
#define AUTO_INIT_TIMEOUT_US		200000

#define DATA_BUFFER_WORDS		2048

/*
 * Percentage of the BCH ECC correction capability above which
 * correctable errors will be reported
 */
#define CORR_THRESHOLD_PERCENT		60

/* NAND small/large page bad block position */
#define NAND_SMALL_BADBLOCK_POS		5
#define NAND_LARGE_BADBLOCK_POS		0

/*
 * NAND controller commands
 */
#define CMD_PAGE_READ			0x01
#define CMD_SPARE_AREA_READ		0x02
#define CMD_STATUS_READ			0x03
#define CMD_PROGRAM_PAGE		0x04
#define CMD_PROGRAM_SPARE_AREA		0x05
#define CMD_COPY_BACK			0x06
#define CMD_DEVICE_ID_READ		0x07
#define CMD_BLOCK_ERASE			0x08
#define CMD_FLASH_RESET			0x09
#define CMD_BLOCKS_LOCK			0x0a
#define CMD_BLOCKS_LOCK_DOWN		0x0b
#define CMD_BLOCKS_UNLOCK		0x0c
#define CMD_READ_BLOCKS_LOCK_STATUS	0x0d
#define CMD_PARAMETER_READ		0x0e
#define CMD_PARAMETER_CHANGE_COL	0x0f
#define CMD_LOW_LEVEL_OP		0x10
#define CMD_PAGE_READ_MULTI		0x11
#define CMD_STATUS_READ_MULTI		0x12
#define CMD_PROGRAM_PAGE_MULTI		0x13
#define CMD_PROGRAM_PAGE_MULTI_CACHE	0x14
#define CMD_BLOCK_ERASE_MULTI		0x15

/*
 * NAND controller register offset
 */
#define NCREG_REVISION			0x000	/* Revision */
#define NCREG_CMD_START			0x004	/* Command Start */
#define NCREG_CMD_EXT_ADDRESS		0x008	/* Command Extended Address */
#define NCREG_CMD_ADDRESS		0x00c	/* Command Address */
#define NCREG_CMD_END_ADDRESS		0x010	/* Command End Address */
#define NCREG_INTFC_STATUS		0x014	/* Interface Status */
#define NCREG_CS_NAND_SELECT		0x018	/* EBI CS Select */
#define NCREG_CS_NAND_XOR		0x01c	/* EBI CS Address XOR with
						   1FC0 Control */
#define NCREG_LL_OP			0x020	/* Low Level Operation */
#define NCREG_MPLANE_BASE_EXT_ADDRESS	0x024	/* Multiplane base extended
						   address */
#define NCREG_MPLANE_BASE_ADDRESS	0x028	/* Multiplane base address */
#define NCREG_ACC_CONTROL_CS0		0x050	/* Access Control CS0 */
#define NCREG_CONFIG_CS0		0x054	/* Config CS0 */
#define NCREG_TIMING_1_CS0		0x058	/* Timing Parameters 1 CS0 */
#define NCREG_TIMING_2_CS0		0x05c	/* Timing Parameters 2 CS0 */
#define NCREG_ACC_CONTROL_CS1		0x060	/* Access Control CS1 */
#define NCREG_CONFIG_CS1		0x064	/* Config CS1 */
#define NCREG_TIMING_1_CS1		0x068	/* Timing Parameters 1 CS1 */
#define NCREG_TIMING_2_CS1		0x06c	/* Timing Parameters 2 CS1 */
#define NCREG_CORR_STAT_THRESHOLD	0x0c0	/* Correctable Error Reporting
						   Threshold */
#define NCREG_BLK_WR_PROTECT		0x0c8	/* Block Write Protect Enable
						   and Size for EBI_CS0b */
#define NCREG_MULTIPLANE_OPCODES_1	0x0cc	/* Multiplane Customized
						   Opcodes */
#define NCREG_MULTIPLANE_OPCODES_2	0x0d0	/* Multiplane Customized
						   Opcodes */
#define NCREG_MULTIPLANE_CTRL		0x0d4	/* Multiplane Control */
#define NCREG_UNCORR_ERROR_COUNT	0x0fc	/* Total Uncorrectable Error
						   Count */
#define NCREG_CORR_ERROR_COUNT		0x100	/* Correctable Error Count */
#define NCREG_READ_ERROR_COUNT		0x104	/* Total Correctable Error
						   Count */
#define NCREG_BLOCK_LOCK_STATUS		0x108	/* Block Lock Status */
#define NCREG_ECC_CORR_EXT_ADDR		0x10c	/* ECC Correctable Error
						   Extended Address */
#define NCREG_ECC_CORR_ADDR		0x110	/* ECC Correctable Error
						   Address */
#define NCREG_ECC_UNC_EXT_ADDR		0x114	/* ECC Uncorrectable Error
						   Extended Address */
#define NCREG_ECC_UNC_ADDR		0x118	/* ECC Uncorrectable Error
						   Address */
#define NCREG_FLASH_READ_EXT_ADDR	0x11c	/* Read Data Extended Address */
#define NCREG_FLASH_READ_ADDR		0x120	/* Read Data Address */
#define NCREG_PROGRAM_PAGE_EXT_ADDR	0x124	/* Page Program Extended
						   Address */
#define NCREG_PROGRAM_PAGE_ADDR		0x128	/* Page Program Address */
#define NCREG_COPY_BACK_EXT_ADDR	0x12c	/* Copy Back Extended Address */
#define NCREG_COPY_BACK_ADDR		0x130	/* Copy Back Address */
#define NCREG_BLOCK_ERASE_EXT_ADDR	0x134	/* Block Erase Extended
						   Address */
#define NCREG_BLOCK_ERASE_ADDR		0x138	/* Block Erase Address */
#define NCREG_INV_READ_EXT_ADDR		0x13c	/* Invalid Data Extended
						   Address */
#define NCREG_INV_READ_ADDR		0x140	/* Invalid Data Address */
#define NCREG_INIT_STATUS		0x144	/* Initialization status */
#define NCREG_ONFI_STATUS		0x148	/* ONFI Status */
#define NCREG_ONFI_DEBUG_DATA		0x14c	/* ONFI Debug Data */
#define NCREG_SEMAPHORE			0x150	/* Semaphore */
#define NCREG_FLASH_DEVICE_ID		0x194	/* Device ID */
#define NCREG_FLASH_DEVICE_ID_EXT	0x198	/* Extended Device ID */
#define NCREG_LL_RDDATA			0x19c	/* Low Level Read Data */
#define NCREG_SPARE_AREA_READ_OFS_0	0x200	/* Spare Area Read Bytes */
#define NCREG_SPARE_AREA_WRITE_OFS_0	0x280	/* Spare Area Write Bytes */
#define NCREG_FLASH_CACHE_BASE		0x400	/* Cache Buffer Access */
#define NCREG_INTERRUPT_BASE		0xf00	/* Interrupt Base Address */

/*
 * NAND controller register fields
 */
#define NCFLD_CMD_START_OPCODE_SHIFT				24
#define NCFLD_INTFC_STATUS_FLASH_STATUS_MASK			0x000000FF
#define NCFLD_CS_NAND_SELECT_AUTO_DEVID_CONFIG			0x40000000
#define NCFLD_CS_NAND_SELECT_WP					0x20000000
#define NCFLD_CS_NAND_SELECT_DIRECT_ACCESS_CS_MASK		0x000000FF
#define NCFLD_CS_NAND_XOR_CS_MASK				0x000000FF
#define NCFLD_CONFIG_CS0_BLOCK_SIZE_MASK			0x70000000
#define NCFLD_CONFIG_CS0_BLOCK_SIZE_SHIFT			28
#define NCFLD_CONFIG_CS0_DEVICE_SIZE_MASK			0x0f000000
#define NCFLD_CONFIG_CS0_DEVICE_SIZE_SHIFT			24
#define NCFLD_CONFIG_CS0_DEVICE_WIDTH_MASK			0x00800000
#define NCFLD_CONFIG_CS0_DEVICE_WIDTH_SHIFT			23
#define NCFLD_CONFIG_CS0_PAGE_SIZE_MASK				0x00300000
#define NCFLD_CONFIG_CS0_PAGE_SIZE_SHIFT			20
#define NCFLD_CONFIG_CS0_FUL_ADR_BYTES_MASK			0x00070000
#define NCFLD_CONFIG_CS0_FUL_ADR_BYTES_SHIFT			16
#define NCFLD_CONFIG_CS0_COL_ADR_BYTES_MASK			0x00007000
#define NCFLD_CONFIG_CS0_COL_ADR_BYTES_SHIFT			12
#define NCFLD_CONFIG_CS0_BLK_ADR_BYTES_MASK			0x00000700
#define NCFLD_CONFIG_CS0_BLK_ADR_BYTES_SHIFT			8
#define NCFLD_ACC_CONTROL_CS0_RD_ECC_EN_MASK			0x80000000
#define NCFLD_ACC_CONTROL_CS0_RD_ECC_EN_SHIFT			31
#define NCFLD_ACC_CONTROL_CS0_WR_ECC_EN_MASK			0x40000000
#define NCFLD_ACC_CONTROL_CS0_WR_ECC_EN_SHIFT			30
#define NCFLD_ACC_CONTROL_CS0_FAST_PGM_RDIN_MASK		0x10000000
#define NCFLD_ACC_CONTROL_CS0_FAST_PGM_RDIN_SHIFT		28
#define NCFLD_ACC_CONTROL_CS0_RD_ERASED_ECC_EN_MASK		0x08000000
#define NCFLD_ACC_CONTROL_CS0_RD_ERASED_ECC_EN_SHIFT		27
#define NCFLD_ACC_CONTROL_CS0_PARTIAL_PAGE_EN_MASK		0x04000000
#define NCFLD_ACC_CONTROL_CS0_PARTIAL_PAGE_EN_SHIFT		26
#define NCFLD_ACC_CONTROL_CS0_PAGE_HIT_EN_MASK			0x01000000
#define NCFLD_ACC_CONTROL_CS0_PAGE_HIT_EN_SHIFT			24
#define NCFLD_ACC_CONTROL_CS0_ECC_LEVEL_MASK			0x001f0000
#define NCFLD_ACC_CONTROL_CS0_ECC_LEVEL_SHIFT			16
#define NCFLD_ACC_CONTROL_CS0_SECTOR_SIZE_1K_MASK		0x00000080
#define NCFLD_ACC_CONTROL_CS0_SECTOR_SIZE_1K_SHIFT		7
#define NCFLD_ACC_CONTROL_CS0_SPARE_AREA_SIZE_MASK		0x0000007f
#define NCFLD_ACC_CONTROL_CS0_SPARE_AREA_SIZE_SHIFT		0
#define NCFLD_CORR_STAT_THRESHOLD_CS0_MASK			0x0000003f
#define NCFLD_CORR_STAT_THRESHOLD_CS0_SHIFT 			0
#define NCFLD_CORR_STAT_THRESHOLD_CS1_MASK			0x00000fc0
#define NCFLD_CORR_STAT_THRESHOLD_CS1_SHIFT			6
#define NCFLD_INIT_STATUS_INIT_SUCCESS				0x20000000

/*
 * IDM NAND register offset
 */
#define IDMREG_IO_CONTROL_DIRECT				0x408
#define IDMREG_IO_STATUS					0x500
#define IDMREG_RESET_CONTROL					0x800

/*
 * IDM NAND IO Control register fields
 */
#define IDMFLD_NAND_IO_CONTROL_DIRECT_APB_LE_MODE		(1UL << 24)

/*
 * Interrupts
 */
#define NCINTR_NP_READ						0
#define NCINTR_BLKERA						1
#define NCINTR_CPYBK						2
#define NCINTR_PGMPG						3
#define NCINTR_CTLRDY						4
#define NCINTR_RBPIN						5
#define NCINTR_UNC						6
#define NCINTR_CORR						7

/* 512B flash cache in the NAND controller HW */
#define FC_SHIFT	9U
#define FC_BYTES	512U
#define FC_WORDS	(FC_BYTES >> 2)
#define FC(x)		(NCREG_FLASH_CACHE_BASE + ((x) << 2))

/* 64B oob cache in the NAND controller HW */
#define MAX_CONTROLLER_OOB_BYTES	64
#define MAX_CONTROLLER_OOB_WORDS	(MAX_CONTROLLER_OOB_BYTES >> 2)

/* Status bits */
#define NAND_STATUS_FAIL	0x01
#define NAND_STATUS_FAIL_N1	0x02
#define NAND_STATUS_TRUE_READY	0x20
#define NAND_STATUS_READY	0x40
#define NAND_STATUS_WP		0x80

/*
 * Register access macros - NAND flash controller
 */
#define NAND_REG_RD(x)		readl(ctrl.nand_regs + (x))
#define NAND_REG_WR(x, y)	writel((y), ctrl.nand_regs + (x))
#define NAND_REG_CLR(x, y)	NAND_REG_WR((x), NAND_REG_RD(x) & ~(y))
#define NAND_REG_SET(x, y)	NAND_REG_WR((x), NAND_REG_RD(x) | (y))

/*
 * IRQ operations
 */
#define NAND_ACK_IRQ(bit) writel(1, ((u32 *)ctrl.nand_intr_regs) + (bit))
#define NAND_TEST_IRQ(bit) (readl(((u32 *)ctrl.nand_intr_regs) + (bit)) & 1)

/*
 * Data access macros (APB access is big endian by default)
 */
#define NAND_BEGIN_DATA_ACCESS()					\
	writel(readl(ctrl.nand_idm_io_ctrl_direct_reg) |		\
	       IDMFLD_NAND_IO_CONTROL_DIRECT_APB_LE_MODE,		\
	       ctrl.nand_idm_io_ctrl_direct_reg)
#define NAND_END_DATA_ACCESS()						\
	writel(readl(ctrl.nand_idm_io_ctrl_direct_reg) &		\
	       ~IDMFLD_NAND_IO_CONTROL_DIRECT_APB_LE_MODE,		\
	       ctrl.nand_idm_io_ctrl_direct_reg)
/*
 * Misc NAND controller configuration/status macros
 */
#define NC_REG_CONFIG(cs) (NCREG_CONFIG_CS0 + ((cs) << 4))

#define WR_CONFIG(cs, field, val) do {					\
	uint32_t reg = NC_REG_CONFIG(cs), contents = NAND_REG_RD(reg);	\
	contents &= ~(NCFLD_CONFIG_CS0_##field##_MASK);			\
	contents |= (val) << NCFLD_CONFIG_CS0_##field##_SHIFT;		\
	NAND_REG_WR(reg, contents);					\
} while (0)

#define RD_CONFIG(cs, field)						    \
	((NAND_REG_RD(NC_REG_CONFIG(cs)) & NCFLD_CONFIG_CS0_##field##_MASK) \
	>> NCFLD_CONFIG_CS0_##field##_SHIFT)

#define NC_REG_ACC_CONTROL(cs) (NCREG_ACC_CONTROL_CS0 + ((cs) << 4))

#define WR_ACC_CONTROL(cs, field, val) do {				    \
	uint32_t reg = NC_REG_ACC_CONTROL(cs), contents = NAND_REG_RD(reg); \
	contents &= ~(NCFLD_ACC_CONTROL_CS0_##field##_MASK);		    \
	contents |= (val) << NCFLD_ACC_CONTROL_CS0_##field##_SHIFT;	    \
	NAND_REG_WR(reg, contents);					    \
} while (0)

#define RD_ACC_CONTROL(cs, field)					\
	((NAND_REG_RD(NC_REG_ACC_CONTROL(cs)) &				\
	 NCFLD_ACC_CONTROL_CS0_##field##_MASK)				\
	 >> NCFLD_ACC_CONTROL_CS0_##field##_SHIFT)

#define CORR_ERROR_COUNT (NAND_REG_RD(NCREG_CORR_ERROR_COUNT))
#define UNCORR_ERROR_COUNT (NAND_REG_RD(NCREG_UNCORR_ERROR_COUNT))

#define WR_CORR_THRESH(cs, val) do {					   \
	uint32_t contents = NAND_REG_RD(NCREG_CORR_STAT_THRESHOLD);	   \
	uint32_t shift = NCFLD_CORR_STAT_THRESHOLD_CS1_SHIFT * (cs);	   \
	contents &= ~(NCFLD_CORR_STAT_THRESHOLD_CS0_MASK << shift);	   \
	contents |= ((val) & NCFLD_CORR_STAT_THRESHOLD_CS0_MASK) << shift; \
	NAND_REG_WR(NCREG_CORR_STAT_THRESHOLD, contents);		   \
} while (0)

#define NC_REG_TIMING1(cs) (NCREG_TIMING_1_CS0 + ((cs) << 4))
#define NC_REG_TIMING2(cs) (NCREG_TIMING_2_CS0 + ((cs) << 4))

/*
 * NAND controller timing configurations for ONFI timing modes [0-5]
 *
 * Clock tick = 10ns
 * Multiplier:
 * x1: tWP tWH tRP tREH tCLH tALH
 * x2: tCS tADL tWB tWHR
 */
#define NAND_TIMING_DATA					\
{								\
	/* ONFI timing mode 0 :				*/	\
	/* tWC=100ns tWP=50ns tWH=30ns			*/	\
	/* tRC=100ns tRP=50ns tREH=30ns			*/	\
	/* tCS=70ns tCLH=20ns tALH=20ns tADL=200ns	*/	\
	/* tWB=200ns tWHR=120ns tREA=40ns		*/	\
	{							\
		.timing1 = 0x6565435b,				\
		.timing2 = 0x00001e85,				\
	},							\
	/* ONFI timing mode 1 :				*/	\
	/* tWC=45 tWP=25ns tWH=15ns			*/	\
	/* tRC=50 tRP=25ns tREH=15ns			*/	\
	/* tCS=35ns tCLH=10ns tALH=10ns tADL=100ns	*/	\
	/* tWB=100ns tWHR=80ns tREA=30ns		*/	\
	{							\
		.timing1 = 0x33333236,				\
		.timing2 = 0x00001064,				\
	},							\
	/* ONFI timing mode 2 :				*/	\
	/* tWC=35ns tWP=17ns tWH=15ns			*/	\
	/* tRC=35ns tRP=17ns tREH=15ns			*/	\
	/* tCS=25ns tCLH=10ns tALH=10ns tADL=100ns	*/	\
	/* tWB=100ns tWHR=80ns tREA=25ns		*/	\
	{							\
		.timing1 = 0x32322226,				\
		.timing2 = 0x00001063,				\
	},							\
	/* ONFI timing mode 3 :				*/	\
	/* tWC=30ns tWP=15ns tWH=10ns			*/	\
	/* tRC=30ns tRP=15ns tREH=10ns			*/	\
	/* tCS=25ns tCLH=5ns tALH=5ns tADL=100ns	*/	\
	/* tWB=100ns tWHR=60ns tREA=20ns		*/	\
	{							\
		.timing1 = 0x22222126,				\
		.timing2 = 0x00001043,				\
	},							\
	/* ONFI timing mode 4 :				*/	\
	/* tWC=25ns tWP=12ns tWH=10ns			*/	\
	/* tRC=25ns tRP=12ns tREH=10ns			*/	\
	/* tCS=20ns tCLH=5ns tALH=5ns tADL=70ns		*/	\
	/* tWB=100ns tWHR=60ns tREA=20ns		*/	\
	{							\
		.timing1 = 0x21212114,				\
		.timing2 = 0x00001042,				\
	},							\
	/* ONFI timing mode 5 :				*/	\
	/* tWC=20ns tWP=10ns tWH=7ns			*/	\
	/* tRC=20ns tRP=10ns tREH=7ns			*/	\
	/* tCS=15ns tCLH=5ns tALH=5ns tADL=70ns		*/	\
	/* tWB=100ns tWHR=60ns tREA=16ns		*/	\
	{							\
		.timing1 = 0x11111114,				\
		.timing2 = 0x00001042,				\
	},							\
}

#define ONFI_TIMING_MODES	6

/*
 * Internal structures
 */

struct nand_timing {
	uint32_t timing1;
	uint32_t timing2;
};

enum nand_ecc_code {
	ECC_CODE_BCH,
	ECC_CODE_HAMMING,
};

struct nand_cfg {
	uint64_t device_size;
	uint32_t block_size;
	uint32_t page_size;
	uint32_t spare_area_size;
	uint32_t device_width;
	uint32_t col_adr_bytes;
	uint32_t blk_adr_bytes;
	uint32_t ful_adr_bytes;
	uint32_t sector_size_1k;
	uint32_t ecc_level;
	enum nand_ecc_code ecc_code;
};

struct nand_host_controller {
	uint32_t cs;

	void *nand_regs;
	void *nand_idm_regs;

	void *nand_intr_regs;
	void *nand_idm_io_ctrl_direct_reg;

	struct nand_cfg cfg;
	int initialized;

	/*
	 * tmode controls NAND interface timing.
	 * Values:
	 * [0-5]: change timing register values to comply with ONFI
	 *        timing mode [0-5]
	 * other: use default timing register values
	 */
	int tmode;

	/*
	 * This flag indicates that existing data cache should not be used.
	 * When PAGE_HIT is enabled and a read operation is performed from
	 * the same address, the controller is not reading from the NAND,
	 * considering that data is already available in the controller data
	 * cache. In that case the correctable or uncorrectable interrupts
	 * are not asserted, so the read would appear successful.
	 * This flag will be used to temporary disable PAGE_HIT to force the
	 * data to be read again from the NAND and have correct ECC results.
	 */
	int data_cache_invalid;

	uint32_t *buf;
};

/*
 * Global variables
 */
static const uint32_t block_sizes[] = { 8, 16, 128, 256, 512, 1024, 2048,
					    0 /* invalid */ };
static const uint32_t page_sizes[] = { 512, 2048, 4096, 8192 };
static const struct nand_timing onfi_tmode[ONFI_TIMING_MODES] =
			NAND_TIMING_DATA;
static struct nand_host_controller ctrl;

/***********************************************************************
 * Private functions
 ***********************************************************************/
static int nand_cmd(int cmd, uint64_t addr, int us)
{
	debug("%s: cmd %d addr 0x%llx\n", __func__, cmd, addr);

	if (cmd != CMD_PROGRAM_PAGE) {
		/*
		 * For program page the address has been already set
		 * before writing data to controller flash cache.
		 */
		NAND_REG_WR(NCREG_CMD_EXT_ADDRESS,
			    (ctrl.cs << 16) | ((addr >> 32) & 0xffff));
		NAND_REG_WR(NCREG_CMD_ADDRESS, addr & 0xffffffff);
	}
	NAND_REG_WR(NCREG_CMD_START, cmd << NCFLD_CMD_START_OPCODE_SHIFT);

	while (us > 0) {
		if (NAND_TEST_IRQ(NCINTR_CTLRDY)) {
			NAND_ACK_IRQ(NCINTR_CTLRDY);
			break;
		}
		udelay(10);
		us -= 10;
	}
	if (us < 0) {
		printf(DRV_NAME ": command completion timeout\n"
		       "\t\tcmd %d addr 0x%llx\n"
		       "\t\tintfc status 0x%x\n",
		       cmd, addr,
		       NAND_REG_RD(NCREG_INTFC_STATUS));
		return -EIO;
	}
	return 0;
}

static unsigned int hweight8(unsigned int w)
{
	unsigned int res = w - ((w >> 1) & 0x55);
	res = (res & 0x33) + ((res >> 2) & 0x33);
	return (res + (res >> 4)) & 0x0F;
}

/*
 * This function counts the 0 bits in a sector data and oob bytes.
 * If the count result is less than a threshold it considers that
 * the sector is an erased sector and it sets all its data and oob
 * bits to 1.
 * The threshold is set to half of the ECC strength to allow for
 * a sector that also has some bits stuck on 1 to be correctable.
 */
static int erased_sector(uint8_t *buf, uint8_t *oob,
			 unsigned int *bitflips)
{
	int data_bytes = 512 << ctrl.cfg.sector_size_1k;
	int oob_bytes = ctrl.cfg.spare_area_size << ctrl.cfg.sector_size_1k;
	/* Set the bitflip threshold to half of the BCH ECC strength */
	int threshold = (ctrl.cfg.ecc_level << ctrl.cfg.sector_size_1k) / 2;
	int counter = 0;
	int i;

	debug("%s buf %p oob %p\n", __func__, buf, oob);

	if (ctrl.cfg.ecc_code != ECC_CODE_BCH)
		return 0;

	/* Count bitflips in OOB first */
	for (i = 0; i < oob_bytes; i++) {
		counter += hweight8(~oob[i]);
		if (counter > threshold)
			return 0;
	}

	/* Count bitflips in data */
	for (i = 0; i < data_bytes; i++) {
		counter += hweight8(~buf[i]);
		if (counter > threshold)
			return 0;
	}

	/* Clear data and oob */
	memset(buf, 0xFF, data_bytes);
	memset(oob, 0xFF, oob_bytes);

	*bitflips = counter;
	return 1;
}

static int nand_read_page(uint64_t addr, uint32_t *buf,
			  uint32_t *stats_corrected,
			  uint32_t *stats_failed)
{
	uint32_t trans;
	uint64_t page_addr = addr;
	int i, j;
	int res;
	uint32_t max_bitflips, bitflips;
	uint32_t corr_error_count, uncorr_error_count;
	static uint8_t sector_oob[2 * MAX_CONTROLLER_OOB_BYTES];
	int oob_bytes;
	uint32_t *sector_buf = buf;
	uint8_t *oob = sector_oob;
	uint32_t w;

	debug("%s 0x%llx -> 0x%x\n", __func__, addr, (uint32_t)buf);

	NAND_ACK_IRQ(NCINTR_UNC);
	NAND_ACK_IRQ(NCINTR_CORR);
	max_bitflips = 0;
	corr_error_count = 0;
	uncorr_error_count = 0;

	trans = ctrl.cfg.page_size >> FC_SHIFT;
	for (i = 0; i < trans; i++, addr += FC_BYTES) {

		if (!ctrl.cfg.sector_size_1k || ((i & 0x1) == 0)) {
			sector_buf = buf;
			oob = sector_oob;
		}

		if (ctrl.data_cache_invalid) {
			if ((i == 0) && RD_ACC_CONTROL(ctrl.cs, PAGE_HIT_EN))
				/*
				 * Temporary disable the PAGE_HIT to force
				 * data to be read from NAND.
				 */
				WR_ACC_CONTROL(ctrl.cs, PAGE_HIT_EN, 0);
			else
				ctrl.data_cache_invalid = 0;
		}

		res = nand_cmd(CMD_PAGE_READ, addr, CMD_TIMEOUT_US);

		if (ctrl.data_cache_invalid) {
			/* Re-enable PAGE_HIT if it was temporary disabled */
			WR_ACC_CONTROL(ctrl.cs, PAGE_HIT_EN, 1);
			ctrl.data_cache_invalid = 0;
		}

		if (res)
			return res;

		NAND_BEGIN_DATA_ACCESS();
		for (j = 0; j < FC_WORDS; j++, buf++)
			*buf = NAND_REG_RD(FC(j));
		NAND_END_DATA_ACCESS();

		/* OOB bytes per sector */
		oob_bytes = ctrl.cfg.spare_area_size << ctrl.cfg.sector_size_1k;
		/* OOB bytes per 512B transfer */
		if (ctrl.cfg.sector_size_1k && (i & 0x01)) {
			if (oob_bytes > MAX_CONTROLLER_OOB_BYTES)
				oob_bytes -= MAX_CONTROLLER_OOB_BYTES;
			else
				oob_bytes = 0;
		}
		if (oob_bytes > MAX_CONTROLLER_OOB_BYTES)
			oob_bytes = MAX_CONTROLLER_OOB_BYTES;

		for (j = 0; j < oob_bytes; j++, oob++) {
			if ((j & 0x3) == 0)
				w = NAND_REG_RD(NCREG_SPARE_AREA_READ_OFS_0 +
						j);
			/* APB read is big endian */
			*oob = w >> (24 - ((j & 0x03) << 3));
		}

		if (!ctrl.cfg.sector_size_1k || (i & 0x1)) {
			/* Check uncorrectable errors */
			if (NAND_TEST_IRQ(NCINTR_UNC)) {
				if (erased_sector((uint8_t *)sector_buf,
						  sector_oob,
						  &bitflips)) {
					corr_error_count += bitflips;
					if (bitflips > max_bitflips)
						max_bitflips = bitflips;
				} else {
					uncorr_error_count += 1;
				}
				NAND_ACK_IRQ(NCINTR_UNC);
				ctrl.data_cache_invalid = 1;
			}
			/* Check correctable errors */
			if (NAND_TEST_IRQ(NCINTR_CORR)) {
				bitflips = CORR_ERROR_COUNT;
				corr_error_count += bitflips;
				if (bitflips > max_bitflips)
					max_bitflips = bitflips;
				NAND_ACK_IRQ(NCINTR_CORR);
				ctrl.data_cache_invalid = 1;
			}
		}
	}

	if (stats_corrected)
		*stats_corrected += corr_error_count;
	if (stats_failed)
		*stats_failed += uncorr_error_count;

	if (uncorr_error_count) {
		printf(DRV_NAME ": uncorrectable error at 0x%llx count %d\n",
		       page_addr, uncorr_error_count);
		return -EIO;
	}
	if (max_bitflips) {
		printf(DRV_NAME ": correctable error at 0x%llx count %d\n",
		       page_addr, max_bitflips);
	}
	return max_bitflips;
}

static int nand_block_isbad(uint64_t addr, int *bad)
{
	uint32_t w;
	uint8_t b;
	uint32_t badblockpos;
	int res;

	debug("%s 0x%llx\n", __func__, addr);

	/* Set the bad block position */
	if (ctrl.cfg.page_size > 512)
		badblockpos = NAND_LARGE_BADBLOCK_POS;
	else
		badblockpos = NAND_SMALL_BADBLOCK_POS;

	res = nand_cmd(CMD_PAGE_READ, addr, CMD_TIMEOUT_US);

	/*
	 * Force next read to be done from NAND,
	 * to have correct ECC correction status.
	 */
	ctrl.data_cache_invalid = 1;

	if (res) {
		*bad = 1;
		return res;
	}

	/* APB read is big endian */
	w = NAND_REG_RD(NCREG_SPARE_AREA_READ_OFS_0 +
		(badblockpos & ~0x3));
	b = w >> (24 - ((badblockpos & 0x03) << 3));
	if (b != 0xFF) {
		*bad = 1;
		return 0;
	}
	*bad = 0;
	return 0;
}

static int nand_get_cfg(struct nand_cfg *cfg)
{
	int res = 0;

	cfg->block_size = block_sizes[RD_CONFIG(ctrl.cs, BLOCK_SIZE)] << 10;
	cfg->device_size = (4ULL << 20) << RD_CONFIG(ctrl.cs, DEVICE_SIZE);
	cfg->page_size = page_sizes[RD_CONFIG(ctrl.cs, PAGE_SIZE)];
	cfg->device_width = RD_CONFIG(ctrl.cs, DEVICE_WIDTH) ? 16 : 8;
	cfg->col_adr_bytes = RD_CONFIG(ctrl.cs, COL_ADR_BYTES);
	cfg->blk_adr_bytes = RD_CONFIG(ctrl.cs, BLK_ADR_BYTES);
	cfg->ful_adr_bytes = RD_CONFIG(ctrl.cs, FUL_ADR_BYTES);
	cfg->spare_area_size = RD_ACC_CONTROL(ctrl.cs, SPARE_AREA_SIZE);
	cfg->sector_size_1k = RD_ACC_CONTROL(ctrl.cs, SECTOR_SIZE_1K);
	cfg->ecc_level = RD_ACC_CONTROL(ctrl.cs, ECC_LEVEL);

	/* Check configuration validity */
	if (cfg->block_size == 0)
		res = -EINVAL;
	if (cfg->ful_adr_bytes != cfg->col_adr_bytes + cfg->blk_adr_bytes)
		res = -EINVAL;
	if ((cfg->spare_area_size < 16) || (cfg->spare_area_size > 64))
		res = -EINVAL;
	if ((cfg->device_width == 16) && (cfg->spare_area_size & 0x1))
		res = -EINVAL;
	if ((cfg->sector_size_1k == 0) && (cfg->ecc_level > 17))
		res = -EINVAL;
	if ((cfg->sector_size_1k == 1) && (cfg->ecc_level > 20))
		res = -EINVAL;
	if (cfg->spare_area_size < ((14 * cfg->ecc_level + 7) / 8))
		res = -EINVAL;

	/* Special case: using Hamming code */
	if ((cfg->ecc_level == 15) && (cfg->spare_area_size == 16) &&
	    (cfg->sector_size_1k == 0))
		cfg->ecc_code = ECC_CODE_HAMMING;
	else
		cfg->ecc_code = ECC_CODE_BCH;

	if (res)
		debug("%s: cs %d config 0x%x acc_control 0x%x\n",
		      __func__, ctrl.cs,
		      NAND_REG_RD(NC_REG_CONFIG(ctrl.cs)),
		      NAND_REG_RD(NC_REG_ACC_CONTROL(ctrl.cs)));

	return res;
}

static void nand_print_cfg(struct nand_cfg *cfg)
{
	printf("NAND %u-bit %u addr-cycles %uMB total\n"
	       "     %uKB blocks, %uB pages\n"
	       "     %ubit/%uB %s-ECC %uB/512B OOB\n",
	       cfg->device_width,
	       cfg->ful_adr_bytes,
	       (uint32_t)(cfg->device_size >> 20),
	       cfg->block_size >> 10,
	       cfg->page_size,
	       cfg->ecc_code == ECC_CODE_HAMMING ?
				1 : cfg->ecc_level << cfg->sector_size_1k,
	       cfg->sector_size_1k ? 1024 : 512,
	       cfg->ecc_code == ECC_CODE_HAMMING ? "HAMMING" : "BCH",
	       cfg->spare_area_size);
}

static int nand_init(void)
{
	int res;
	int timeout;

	ctrl.cs = 0;

	NAND_REG_WR(NCREG_SEMAPHORE, 0);

	/* Reset the NAND controller */
	writel(1, ctrl.nand_idm_regs + IDMREG_RESET_CONTROL);
	udelay(1);
	writel(0, ctrl.nand_idm_regs + IDMREG_RESET_CONTROL);
	udelay(10);

	/* Execute controller auto configuration */
	NAND_REG_CLR(NCREG_CS_NAND_SELECT,
		     NCFLD_CS_NAND_SELECT_AUTO_DEVID_CONFIG);
	NAND_REG_SET(NCREG_CS_NAND_SELECT,
		     NCFLD_CS_NAND_SELECT_AUTO_DEVID_CONFIG);
	timeout = AUTO_INIT_TIMEOUT_US;
	while (timeout > 0) {
		udelay(10);
		if (NCFLD_INIT_STATUS_INIT_SUCCESS &
		    NAND_REG_RD(NCREG_INIT_STATUS)) {
			printf(DRV_NAME ": auto-init success\n");
			break;
		}
		timeout -= 10;
	}
	if (timeout <= 0) {
		debug("%s: auto-init status 0x%x\n", __func__,
		      NAND_REG_RD(NCREG_INIT_STATUS));
		printf(DRV_NAME ": auto-init failed\n");
		return -EIO;
	}

	/* Perform basic controller initialization */
	NAND_REG_CLR(NCREG_CS_NAND_SELECT,
		     NCFLD_CS_NAND_SELECT_AUTO_DEVID_CONFIG);
	NAND_REG_CLR(NCREG_CS_NAND_SELECT,
		     NCFLD_CS_NAND_SELECT_DIRECT_ACCESS_CS_MASK);
	NAND_REG_CLR(NCREG_CS_NAND_XOR, NCFLD_CS_NAND_XOR_CS_MASK);

	/* Set write-protection */
	NAND_REG_SET(NCREG_CS_NAND_SELECT, NCFLD_CS_NAND_SELECT_WP);

	/* Clear IRQ */
	NAND_ACK_IRQ(NCINTR_CTLRDY);

	/* Configure ACC_CONTROL for CS 0 */
	WR_ACC_CONTROL(ctrl.cs, RD_ECC_EN, 1);
	WR_ACC_CONTROL(ctrl.cs, WR_ECC_EN, 1);
	WR_ACC_CONTROL(ctrl.cs, FAST_PGM_RDIN, 0);
	WR_ACC_CONTROL(ctrl.cs, RD_ERASED_ECC_EN, 0);
	WR_ACC_CONTROL(ctrl.cs, PARTIAL_PAGE_EN, 0);
	WR_ACC_CONTROL(ctrl.cs, PAGE_HIT_EN, 1);

	res = nand_get_cfg(&ctrl.cfg);
	if (res) {
		printf(DRV_NAME ": invalid configuration\n");
		return res;
	}

	/* Set BCH correctable error threshold */
	if (ctrl.cfg.ecc_code == ECC_CODE_BCH) {
		/* threshold = ceil(ECC-STRENGTH * Percentage) */
		WR_CORR_THRESH(ctrl.cs,
			       (CORR_THRESHOLD_PERCENT *
				(ctrl.cfg.ecc_level <<
				 ctrl.cfg.sector_size_1k) + 99) /
			       100);
	}

	/* Set semaphore to indicate initializatin done */
	NAND_REG_WR(NCREG_SEMAPHORE, 0xFF);

	nand_print_cfg(&ctrl.cfg);

	return 0;
}

static void nand_timing_setup(void)
{
	debug("%s: tmode %u\n", __func__, ctrl.tmode);
	if ((ctrl.tmode < 0) || (ctrl.tmode >= ONFI_TIMING_MODES))
		return; /* use default timing */

	NAND_REG_WR(NC_REG_TIMING1(ctrl.cs), onfi_tmode[ctrl.tmode].timing1);
	NAND_REG_WR(NC_REG_TIMING2(ctrl.cs), onfi_tmode[ctrl.tmode].timing2);
	printf(DRV_NAME ": tmode %u\n", ctrl.tmode);
}

static int nand_write_page(uint64_t addr, uint32_t *buf)
{
	uint32_t trans = ctrl.cfg.page_size >> FC_SHIFT;
	uint64_t page_addr = addr;
	int i, j;
	int status;
	int res = 0;

	debug("%s 0x%llx <- 0x%x\n", __func__, addr, (uint32_t)buf);

	/* Set all OOB bytes to 0xFF */
	for (j = 0; j < MAX_CONTROLLER_OOB_WORDS; j++) {
		NAND_REG_WR(NCREG_SPARE_AREA_WRITE_OFS_0 + (j << 2),
			    0xffffffff);
	}

	/* Clear write-protection */
	NAND_REG_CLR(NCREG_CS_NAND_SELECT, NCFLD_CS_NAND_SELECT_WP);

	for (i = 0; i < trans; i++, addr += FC_BYTES) {

		/* Full address MUST be set before populating FC */
		NAND_REG_WR(NCREG_CMD_EXT_ADDRESS,
			    (ctrl.cs << 16) | ((addr >> 32) & 0xffff));
		NAND_REG_WR(NCREG_CMD_ADDRESS, addr & 0xffffffff);

		/* Copy data to controller buffer */
		NAND_BEGIN_DATA_ACCESS();
		for (j = 0; j < FC_WORDS; j++, buf++)
			NAND_REG_WR(FC(j), *buf);
		NAND_END_DATA_ACCESS();

		res = nand_cmd(CMD_PROGRAM_PAGE, addr, CMD_TIMEOUT_US);
		if (res)
			break;
	}

	if (!res) {
		status = NAND_REG_RD(NCREG_INTFC_STATUS) &
				NCFLD_INTFC_STATUS_FLASH_STATUS_MASK;
		if (status & NAND_STATUS_FAIL) {
			printf(DRV_NAME ": program failed at 0x%llx\n",
			       page_addr);
			res = -EIO;
		}
	}

	/* Set write-protection */
	NAND_REG_SET(NCREG_CS_NAND_SELECT, NCFLD_CS_NAND_SELECT_WP);
	return res;
}

static int nand_erase_block(uint64_t addr)
{
	int status;
	int res = 0;

	debug("%s 0x%llx\n", __func__, addr);

	/* Clear write-protection */
	NAND_REG_CLR(NCREG_CS_NAND_SELECT, NCFLD_CS_NAND_SELECT_WP);

	res = nand_cmd(CMD_BLOCK_ERASE, addr, CMD_TIMEOUT_US);
	if (res)
		goto erase_exit;

	status = NAND_REG_RD(NCREG_INTFC_STATUS) &
				NCFLD_INTFC_STATUS_FLASH_STATUS_MASK;
	if (status & NAND_STATUS_FAIL) {
		printf(DRV_NAME ": erase failed at 0x%llx\n", addr);
		res = -EIO;
	}

erase_exit:
	/* Set write-protection */
	NAND_REG_SET(NCREG_CS_NAND_SELECT, NCFLD_CS_NAND_SELECT_WP);
	return res;
}

static int bcm_nand_block_isbad(MtdDev *mtd, uint64_t offs)
{
	int ret;
	int bad_block;

	debug("%s 0x%llx\n", __func__, offs);

	if (!ctrl.initialized)
		return -EPERM;
	if (offs >= mtd->size)
		return -EINVAL;
	if (offs & (mtd->erasesize - 1))
		return -EINVAL;

	ret = nand_block_isbad(offs, &bad_block);
	if (ret < 0)
		return ret;

	return bad_block;
}

/*
 * Notes:
 * - "from" is expected to be a NAND page address
 * - "len" does not have to be multiple of NAND page size
 */
static int bcm_nand_read(MtdDev *mtd, uint64_t from, size_t len,
			 size_t *retlen, unsigned char *buf)
{
	int bufcopy = 0;
	int ret;
	size_t bytes;

	debug("%s 0x%x <= 0x%llx [0x%x]\n", __func__,
	      (uint32_t)buf, from, len);

	*retlen = 0;

	if ((from + len) > mtd->size)
		return -EINVAL;
	if (from & (mtd->writesize - 1))
		return -EINVAL;
	if (buf == NULL)
		return -EINVAL;

	/*
	 * if buf is not 4 byte aligned, we need to use an aligned buffer
	 * to read the page and then copy the data to buf
	 */
	if ((uintptr_t)buf & 0x3)
		bufcopy = 1;

	while (len > 0) {
		bytes = mtd->writesize;
		if (len < mtd->writesize) {
			/* if the remaining length is not a complete page,
			 * we need to read the page on an aligned page buffer
			 * and copy the required data length to buf
			 */
			bytes = len;
			bufcopy = 1;
		}
		ret = nand_read_page(from,
				     bufcopy ? ctrl.buf : (uint32_t *)buf,
				     &mtd->ecc_stats.corrected,
				     &mtd->ecc_stats.failed);
		if (ret < 0)
			return ret;
		if (bufcopy)
			memcpy(buf, ctrl.buf, bytes);
		from += bytes;
		buf += bytes;
		len -= bytes;
		*retlen += bytes;
	}
	return 0;
}

/*
 * Notes:
 * - "to" is expected to be a NAND page address
 * - "len" is expected to be multiple of NAND page size
 */
static int bcm_nand_write(MtdDev *mtd, uint64_t to, size_t len,
			  size_t *retlen, const unsigned char *buf)
{
	int bufcopy = 0;
	int ret;

	debug("%s 0x%x => 0x%llx [0x%x]\n", __func__,
	      (uint32_t)buf, to, len);

	*retlen = 0;

	if ((to + len) > mtd->size)
		return -EINVAL;
	if (to & (mtd->writesize - 1))
		return -EINVAL;
	if (len & (mtd->writesize - 1))
		return -EINVAL;
	if (buf == NULL)
		return -EINVAL;

	/*
	 * if buf is not 4 byte aligned, we need to copy each
	 * page data to an aligned buffer before writing
	 */
	if ((uintptr_t)buf & 0x3)
		bufcopy = 1;

	while (len > 0) {
		if (bufcopy)
			memcpy(ctrl.buf, buf, mtd->writesize);

		ret = nand_write_page(to,
				      bufcopy ? ctrl.buf : (uint32_t *)buf);
		if (ret < 0)
			return ret;
		to += mtd->writesize;
		buf += mtd->writesize;
		len -= mtd->writesize;
		*retlen += mtd->writesize;
	}

	return 0;
}

static int bcm_nand_erase(MtdDev *mtd, struct erase_info *instr)
{
	uint64_t offs = instr->addr;
	uint64_t size = instr->len;
	int ret;
	int bad_block;

	debug("%s 0x%llx [0x%llx]\n", __func__, offs, size);

	if (!ctrl.initialized)
		return -EPERM;
	if ((offs + size) > mtd->size)
		return -EINVAL;
	if (offs & (mtd->erasesize - 1))
		return -EINVAL;
	if (size & (mtd->erasesize - 1))
		return -EINVAL;

	while (size > 0) {
		ret = nand_block_isbad(offs, &bad_block);
		if (ret < 0) {
			instr->fail_addr = offs;
			return ret;
		}
		if (!instr->scrub && bad_block) {
			printf(DRV_NAME ": cannot erase bad block 0x%llx\n",
			       offs);
			return -EIO;
		}
		ret = nand_erase_block(offs);
		if (ret < 0) {
			instr->fail_addr = offs;
			return ret;
		}
		offs += mtd->erasesize;
		size -= mtd->erasesize;
	}
	return 0;
}

static int bcm_nand_update(MtdDevCtrlr *mtd)
{
	int ret;

	debug("%s\n", __func__);

	if (ctrl.initialized)
		return 0;

	ret = nand_init();
	if (ret)
		return ret;

	nand_timing_setup();

	mtd->dev->size = ctrl.cfg.device_size;
	mtd->dev->erasesize = ctrl.cfg.block_size;
	mtd->dev->writesize = ctrl.cfg.page_size;
	mtd->dev->oobsize = ctrl.cfg.spare_area_size;

	mtd->dev->erase = bcm_nand_erase;
	mtd->dev->read = bcm_nand_read;
	mtd->dev->write = bcm_nand_write;
	mtd->dev->block_isbad = bcm_nand_block_isbad;

	ctrl.initialized = 1;
	return 0;
}

MtdDevCtrlr *new_bcm_nand(void *nand_base,
			  void *nand_idm_base,
			  int tmode)
{
	MtdDevCtrlr *mtd = xzalloc(sizeof(*mtd));
	MtdDev *dev = xzalloc(sizeof(*dev));

	debug("%s: nand_base 0x%x nand_idm_base 0x%x tmode %d\n", __func__,
	      (unsigned int)nand_base , (unsigned int)nand_idm_base, tmode);

	mtd->update = bcm_nand_update;
	mtd->dev = dev;
	mtd->dev->priv = &ctrl;

	/* Initialize register addresses */
	ctrl.nand_regs = nand_base;
	ctrl.nand_idm_regs = nand_idm_base;
	ctrl.nand_intr_regs = nand_base + NCREG_INTERRUPT_BASE;
	ctrl.nand_idm_io_ctrl_direct_reg = nand_idm_base +
						IDMREG_IO_CONTROL_DIRECT;
	ctrl.tmode = tmode;
	ctrl.buf = xzalloc(DATA_BUFFER_WORDS);
	ctrl.initialized = 0;
	return mtd;
}
