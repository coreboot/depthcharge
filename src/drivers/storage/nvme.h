/*
 * NVMe storage driver for depthcharge
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __DRIVERS_STORAGE_NVME_H__
#define __DRIVERS_STORAGE_NVME_H__

#include <pci.h>
#include <stdint.h>
#include <arch/barrier.h>
#include "drivers/storage/blockdev.h"
#include "base/list.h"

//#define DEBUG_PRINTS
#ifdef DEBUG_PRINTS
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

/* BSD style bit manipulation */
#define SET(t, f)	((t) |= (f))
#define ISSET(t, f)	((t) & (f))
#define CLR(t, f)	((t) &= ~(f))

/* Architecture memory page size
 * These should eventually reference the arch header definitions
 */
#define NVME_PAGE_SHIFT		12
#define NVME_PAGE_SIZE		(1UL << NVME_PAGE_SHIFT)

/* Max 1 PRP list per transfer */
#define MAX_PRP_LISTS 1
/* 8 bytes per entry */
#define PRP_ENTRY_SHIFT 3
/* 1 page per list */
#define PRP_LIST_SHIFT NVME_PAGE_SHIFT
/* 1 page of memory addressed per entry*/
#define PRP_ENTRY_XFER_SHIFT NVME_PAGE_SHIFT
#define PRP_ENTRIES_PER_LIST (1UL << (PRP_LIST_SHIFT - PRP_ENTRY_SHIFT))
#define NVME_MAX_XFER_BYTES  ((MAX_PRP_LISTS * PRP_ENTRIES_PER_LIST ) << PRP_ENTRY_XFER_SHIFT)

/* Loop used to poll for command completions
 * timeout in milliseconds
 */
#define WAIT_WHILE(expr, timeout)				\
	({							\
		typeof(timeout) __counter = timeout * 1000;	\
		typeof(expr) __expr_val;			\
		while ((__expr_val = (expr)) && __counter--)	\
			udelay(1);				\
		__expr_val;					\
	})

/* Command timeout measured in milliseconds */
#define NVME_GENERIC_TIMEOUT	5000

#define writel_with_flush(a,b)	do { writel(a, b); readl(b); } while (0)

typedef int NVME_STATUS;
#define NVME_SUCCESS							0
#define NVME_UNSUPPORTED						-1
#define NVME_DEVICE_ERROR						-2
#define NVME_OUT_OF_RESOURCES						-3
#define NVME_TIMEOUT							-4
#define NVME_INVALID_PARAMETER						-5
#define NVME_NOT_FOUND							-6

#define NVME_ERROR(err) ((err) < 0?1:0)

#define PCI_CLASS_MASS_STORAGE			0x01  /* mass storage class */
#define PCI_CLASS_MASS_STORAGE_NVM		0x08  /* mass storage sub-class non-volatile memory. */
#define PCI_IF_NVMHCI					0x02  /* mass storage programming interface NVMHCI. */

/* Queue Definitions
 * NOTE: The size of the IO queue is tuned for max_transfer_size as
 * a performance optimization. Smaller size saves host memory at
 * cost of performance.
 */
#define NVME_ASQ_SIZE	2	/* Number of admin submission queue entries, only 2 */
#define NVME_ACQ_SIZE	2	/* Number of admin completion queue entries, only 2 */

#define NVME_CSQ_SIZE	11	/* Number of I/O submission queue entries per queue, min 2, max 64 */
#define NVME_CCQ_SIZE	11	/* Number of I/O completion queue entries per queue, min 2, max 64 */

#define NVME_NUM_QUEUES	2	/* Number of queues (Admin + IO) supported by the driver, only 2 supported */
#define NVME_NUM_IO_QUEUES	(NVME_NUM_QUEUES - 1) /* Number of IO queues (not counting Admin Queue) */
#define NVME_ADMIN_QUEUE_INDEX	0	/* Admin queu index must be 0 */
#define NVME_IO_QUEUE_INDEX		1	/* IO queue */

/*
 * NVMe Controller Registers
 */

/* controller register offsets */
#define NVME_CAP_OFFSET		0x0000  /* Controller Capabilities */
#define NVME_VER_OFFSET		0x0008  /* Version */
#define NVME_INTMS_OFFSET	0x000c  /* Interrupt Mask Set */
#define NVME_INTMC_OFFSET	0x0010  /* Interrupt Mask Clear */
#define NVME_CC_OFFSET		0x0014  /* Controller Configuration */
#define NVME_CSTS_OFFSET	0x001c  /* Controller Status */
#define NVME_AQA_OFFSET		0x0024  /* Admin Queue Attributes */
#define NVME_ASQ_OFFSET		0x0028  /* Admin Submission Queue Base Address */
#define NVME_ACQ_OFFSET		0x0030  /* Admin Completion Queue Base Address */
#define NVME_SQ0_OFFSET		0x1000  /* Submission Queue 0 (admin) Tail Doorbell */
#define NVME_CQ0_OFFSET		0x1004  /* Completion Queue 0 (admin) Head Doorbell */

/* 3.1.1 Offset 00h: CAP - Controller Capabilities */
typedef uint64_t NVME_CAP;
#define NVME_CAP_TO(x)		(500 * (((x) >> 24) & 0xff))		/* Timeout, ms (TO is in 500ms increments)*/
#define NVME_CAP_DSTRD(x)	(1 << (2 + (((x) >> 32) & 0xf)))	/* Doorbell Stride, bytes */
#define NVME_CAP_CSS(x)		(((x) >> 37) & 0x7f)				/* Command Set Supported */
#define NVME_CAP_CSS_NVM	(1)
#define NVME_CAP_MPSMIN(x)	(12 + (((x) >> 48) & 0xf))			/* Memory Page Size Minimum */
#define NVME_CAP_MQES(x)	(((x) & 0xffff) + 1)				/* Max Queue Entries Supported per queue */

/* 3.1.5 Offset 14h: CC - Controller Configuration */
typedef uint32_t NVME_CC;
#define NVME_CC_EN	(1 << 0)
#define  NVME_CC_IOCQES(x)	(((x) & 0xf) << 20)
#define  NVME_CC_IOSQES(x)	(((x) & 0xf) << 16)
#define NVME_CC_SHN_MASK	(3 << 14)
#define NVME_CC_SHN_NONE	(0 << 14)
#define NVME_CC_SHN_NORMAL	(1 << 14)
#define NVME_CC_SHN_ABRUPT	(2 << 14)

/* 3.1.6 Offset 1Ch: CSTS - Controller Status */
typedef uint32_t NVME_CSTS;
#define NVME_CSTS_RDY	(1 << 0)

/* CSTS.SHST - Controller Shutdown Status */
#define NVME_CSTS_SHST_NONE		(0 << 2)
#define NVME_CSTS_SHST_INPROGRESS	(1 << 2)
#define NVME_CSTS_SHST_COMPLETE		(2 << 2)
#define NVME_CSTS_SHST_MASK		(3 << 2)

/* 3.1.8 Offset 24h: AQA - Admin Queue Attributes */
typedef uint32_t NVME_AQA;
#define NVME_AQA_ASQS(x)	((x) - 1)
#define NVME_AQA_ACQS(x)	(((x) - 1) << 16)

/* 3.1.9 Offset 28h: ASQ - Admin Submission Queue Base Address */
typedef uint64_t NVME_ASQ;

/* 3.1.10 Offset 30h: ACQ - Admin Completion Queue Base Address */
typedef uint64_t NVME_ACQ;

/* 3.1.11 Offset (1000h + ((2y) * (DSTRD bytes)))
 * SQyTDBL - Submission Queue y Tail Doorbell
 */
typedef uint32_t NVME_SQTDBL;

/* 3.1.12 Offset (1000h + ((2y + 1) * (DSTRD bytes)))
 * CQyHDBL - Completion Queue y Head Doorbell
 */
typedef uint32_t NVME_CQHDBL;

/* These register offsets are defined as 0x1000 + (N * (DSTRD bytes))
 * Get the doorbell stride bit shift value from the controller capabilities.
 */
#define NVME_SQTDBL_OFFSET(QID, DSTRD)	(0x1000 + ((2 * (QID)) * (DSTRD)))	/* Submission Queue y (NVM) Tail Doorbell */
#define NVME_CQHDBL_OFFSET(QID, DSTRD)	(0x1000 + (((2 * (QID)) + 1) * (DSTRD)))	/* Completion Queue y (NVM) Head Doorbell */

/*
 * NVMe Command Set Types
 */

/* NVMe Admin Cmd Opcodes */
#define NVME_ADMIN_CRIOSQ_OPC	1
#define NVME_ADMIN_CRIOSQ_QID(x)	(x)
#define NVME_ADMIN_CRIOSQ_QSIZE(x)	(((x)-1) << 16)
#define NVME_ADMIN_CRIOSQ_CQID(x)	((x) << 16)

#define NVME_ADMIN_SETFEATURES_OPC	9
#define NVME_ADMIN_SETFEATURES_NUMQUEUES	7

#define NVME_ADMIN_CRIOCQ_OPC	5
#define NVME_ADMIN_CRIOCQ_QID(x)	(x)
#define NVME_ADMIN_CRIOCQ_QSIZE(x)	(((x)-1) << 16)

#define NVME_ADMIN_IDENTIFY_OPC	6

#define NVME_IO_FLUSH_OPC	0
#define NVME_IO_WRITE_OPC	1
#define NVME_IO_READ_OPC	2

/* Submission Queue */
typedef struct {
	uint8_t opc;	/* Opcode */
	uint8_t flags;	/* FUSE and PSDT, only 0 setting supported */
	uint16_t cid;	/* Command Identifier */
	uint32_t nsid;	/* Namespace Identifier */
	uint64_t rsvd1;
	uint64_t mptr;	/* Metadata Pointer */
	uint64_t prp[2];	/* PRP entries only, SGL not supported */
	uint32_t cdw10;
	uint32_t cdw11;
	uint32_t cdw12;
	uint32_t cdw13;
	uint32_t cdw14;
	uint32_t cdw15;
} NVME_SQ;

/* Completion Queue */
typedef struct {
	uint32_t cdw0;
	uint32_t rsvd1;
	uint16_t sqhd;	/* Submission Queue Head Pointer */
	uint16_t sqid;	/* Submission Queue Identifier */
	uint16_t cid;	/* Command Identifier */
	uint16_t flags;
#define NVME_CQ_FLAGS_PHASE	0x1
#define NVME_CQ_FLAGS_SC(x)		(((x) & 0x1FE) >> 1)
#define NVME_CQ_FLAGS_SCT(x)	(((x) & 0xE00) >> 9)
} NVME_CQ;

typedef struct {
	uint32_t power_flags; /* MP, MPS and NOPS */
	uint32_t enlat;	/* Entry Latency */
	uint32_t exlat;	/* Exit Latency */
	uint32_t latency_flags;
	uint8_t  rsvd7[16];	/* Reserved as of Nvm Express 1.1 Spec */
} NVME_PSDESCRIPTOR;

#define NVME_MODEL_NUMBER_LEN	40
#define NVME_SERIAL_NUMBER_LEN	20

/* Identify Controller Data */
typedef struct {
	/* Controller Capabilities and Features 0-255 */
	uint16_t vid;	/* PCI Vendor ID */
	uint16_t ssvid;	/* PCI sub-system vendor ID */
	uint8_t  sn[NVME_SERIAL_NUMBER_LEN];	/* Product serial number */

	uint8_t  mn[NVME_MODEL_NUMBER_LEN];	/* Product model number */
	uint8_t  fr[8];	/* Firmware Revision */
	uint8_t  rab;	/* Recommended Arbitration Burst */
	uint8_t  ieee_oiu[3];	/* Organization Unique Identifier */
	uint8_t  cmic;	/* Multi-interface Capabilities */
	uint8_t  mdts;	/* Maximum Data Transfer Size */
	uint8_t  cntlid[2];	/* Controller ID */
	uint8_t  rsvd1[176];	/* Reserved as of Nvm Express 1.1 Spec */
	//
	// Admin Command Set Attributes
	//
	uint16_t oacs;	/* Optional Admin Command Support */
	uint8_t  acl;	/* Abort Command Limit */
	uint8_t  aerl;	/* Async Event Request Limit */
	uint8_t  frmw;	/* Firmware updates */
	uint8_t  lpa;	/* Log Page Attributes */
	uint8_t  elpe;	/* Error Log Page Entries */
	uint8_t  npss;	/* Number of Power States Support */
	uint8_t  avscc;	/* Admin Vendor Specific Command Configuration */
	uint8_t  apsta;	/* Autonomous Power State Transition Attributes */
	uint8_t  rsvd2[246];	/* Reserved as of Nvm Express 1.1 Spec */
	//
	// NVM Command Set Attributes
	//
	uint8_t  sqes;	/* Submission Queue Entry Size */
	uint8_t  cqes;	/* Completion Queue Entry Size */
	uint16_t rsvd3;	/* Reserved as of Nvm Express 1.1 Spec */
	uint32_t nn;	/* Number of Namespaces */
	uint16_t oncs;	/* Optional NVM Command Support */
	uint16_t fuses;	/* Fused Operation Support */
	uint8_t  fna;	/* Format NVM Attributes */
	uint8_t  vwc;	/* Volatile Write Cache */
	uint16_t awun;	/* Atomic Write Unit Normal */
	uint16_t awupf;	/* Atomic Write Unit Power Fail */
	uint8_t  nvscc;	/* NVM Vendor Specific Command Configuration */
	uint8_t  rsvd4;	/* Reserved as of Nvm Express 1.1 Spec */
	uint16_t acwu;	/* Atomic Compare & Write Unit */
	uint16_t rsvd5;	/* Reserved as of Nvm Express 1.1 Spec */
	uint32_t sgls;	/* SGL Support  */
	uint8_t  rsvd6[164];	/* Reserved as of Nvm Express 1.1 Spec */
	//
	// I/O Command set Attributes
	//
	uint8_t rsvd7[1344];	/* Reserved as of Nvm Express 1.1 Spec */
	//
	// Power State Descriptors
	//
	NVME_PSDESCRIPTOR ps_descriptor[32];

	uint8_t  vendor_data[1024];	/* Vendor specific data */
} NVME_ADMIN_CONTROLLER_DATA;

typedef struct {
	uint16_t ms;	/* Metadata Size */
	uint8_t  lbads;	/* LBA Data Size */
	uint8_t  rp;	/* Relative Performance */
} NVME_LBAFORMAT;

/* Identify Namespace Data */
typedef struct {
	uint64_t nsze;	/* Namespace Size (total blocks in fm'd namespace) */
	uint64_t ncap;	/* Namespace Capacity (max number of logical blocks) */
	uint64_t nuse;	/* Namespace Utilization */
	uint8_t  nsfeat;	/* Namespace Features */
	uint8_t  nlbaf;	/* Number of LBA Formats */
	uint8_t  flbas;	/* Formatted LBA size */
	uint8_t  mc;	/* Metadata Capabilities */
	uint8_t  dpc;	/* End-to-end Data Protection capabilities */
	uint8_t  dps;	/* End-to-end Data Protection Type Settings */
	uint8_t  nmic;	/* Namespace Multi-path I/O + NS Sharing Caps */
	uint8_t  rescap;	/* Reservation Capabilities */
	uint8_t  rsvd1[88];	/* Reserved as of Nvm Express 1.1 Spec */
	uint64_t eui64;	/* IEEE Extended Unique Identifier */

	NVME_LBAFORMAT lba_format[16];

	uint8_t rsvd2[192];	/* Reserved as of Nvm Express 1.1 Spec */
	uint8_t vendor_data[3712];	/* Vendor specific data */
} NVME_ADMIN_NAMESPACE_DATA;

typedef struct PrpList {
	uint64_t prp_entry[PRP_ENTRIES_PER_LIST];
} PrpList;

typedef struct NvmeModelData {
	char model_id[NVME_MODEL_NUMBER_LEN];
	uint32_t namespace_id;
	unsigned int block_size;
	lba_t block_count;

	ListNode list_node;
} NvmeModelData;

/* NVMe S.M.A.R.T Log Data */
// Reference linux kernel v5.7 (include/linux/nvme.h)
typedef struct {
	uint8_t  critical_warning;
	uint16_t temperature;
	uint8_t  avail_spare;
	uint8_t  spare_thresh;
	uint8_t  percent_used;
	uint8_t  endu_grp_crit_warn_sumry;
	uint8_t  rsvd7[25];
	//
	// 128bit integers
	//
	uint8_t  data_units_read[16];
	uint8_t  data_units_written[16];
	uint8_t  host_reads[16];
	uint8_t  host_writes[16];
	uint8_t  ctrl_busy_time[16];
	uint8_t  power_cycles[16];
	uint8_t  power_on_hours[16];
	uint8_t  unsafe_shutdowns[16];
	uint8_t  media_errors[16];
	uint8_t  num_err_log_entries[16];

	uint32_t warning_temp_time;
	uint32_t critical_comp_time;
	uint16_t temp_sensor[8];

	uint32_t thm_temp1_trans_count;
	uint32_t thm_temp2_trans_count;
	uint32_t thm_temp1_total_time;
	uint32_t thm_temp2_total_time;

	uint8_t  rsvd232[280];
} __attribute__((packed)) NVME_SMART_LOG_DATA;

/*
 * Driver Types
 */
typedef struct NvmeCtrlr {
	BlockDevCtrlr ctrlr;
	ListNode drives;

	int enabled;
	pcidev_t dev;
	uint32_t ctrlr_regs;

	/* static namespace data */
	ListNode static_model_data;

	/* local copy of controller CAP register */
	NVME_CAP cap;

	/* virtual address of identify controller data */
	NVME_ADMIN_CONTROLLER_DATA *controller_data;

	/* virtual address of pre-allocated PRP Lists */
	PrpList *prp_list[NVME_CSQ_SIZE];

	/* virtual address of raw buffer, split into queues below */
	uint8_t *buffer;
	/* virtual addresses of queue buffers */
	NVME_SQ *sq_buffer[NVME_NUM_QUEUES];
	NVME_CQ *cq_buffer[NVME_NUM_QUEUES];

	NVME_SQTDBL sq_t_dbl[NVME_NUM_QUEUES];
	NVME_CQHDBL cq_h_dbl[NVME_NUM_QUEUES];

	/* current phase of each queue */
	uint8_t pt[NVME_NUM_QUEUES];
	/* sq head index as of most recent completion */
	uint16_t sqhd[NVME_NUM_QUEUES];
	/* current command id for each queue */
	uint16_t cid[NVME_NUM_QUEUES];

	/* Actual IO SQ size accounting for MQES */
	uint16_t iosq_sz;
	/* Actual IO CQ size accounting for MQES*/
	uint16_t iocq_sz;
} NvmeCtrlr;

typedef struct NvmeDrive {
	BlockDev dev;

	NvmeCtrlr *ctrlr;
	uint32_t namespace_id;

	ListNode list_node;
} NvmeDrive;

/* Provide NVMe device or the root port it is connected to */
NvmeCtrlr *new_nvme_ctrlr(pcidev_t dev);

/* Specify static namespace data and skip calling Identify Namespace */
void nvme_add_static_namespace(NvmeCtrlr *ctrlr, uint32_t namespace_id,
			       unsigned int block_size, lba_t block_count,
			       const char *model_id);

#endif /* __DRIVERS_STORAGE_NVME_H__ */
