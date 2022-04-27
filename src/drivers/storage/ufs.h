// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Driver
 *
 * Copyright (C) 2018, The Linux Foundation.  All rights reserved.
 * Copyright (C) 2022, Intel Corporation.
 */

#ifndef __DRIVERS_STORAGE_UFS_H__
#define __DRIVERS_STORAGE_UFS_H__

#include <endian.h>
#include <stdint.h>
#include <libpayload.h>
#include "drivers/storage/blockdev.h"

/**************** Start HCI definitions *********************/

#define UFSHCI_CAP			0x0
#define UFSHCI_VER			0x8
#define UFSHCI_HCDDID			0x10
#define UFSHCI_HCPMID			0x14
#define UFSHCI_IS			0x20
#define UFSHCI_IE			0x24
#define UFSHCI_HCS			0x30
#define UFSHCI_HCE			0x34
#define UFSHCI_UECPA			0x38
#define UFSHCI_UECDL			0x3C
#define UFSHCI_UECN			0x40
#define UFSHCI_UECT			0x44
#define UFSHCI_UECDME			0x48
#define UFSHCI_UTRIACR			0x4C
#define UFSHCI_UTRLBA			0x50
#define UFSHCI_UTRLBAU			0x54
#define UFSHCI_UTRLDBR			0x58
#define UFSHCI_UTRLCLR			0x5c
#define UFSHCI_UTRLRSR			0x60
#define UFSHCI_UTRLCNR			0x64
#define UFSHCI_UTMRLBA			0x70
#define UFSHCI_UTMRLBAU			0x74
#define UFSHCI_UTMRLDBR			0x78
#define UFSHCI_UTMRLCLR			0x7c
#define UFSHCI_UTMRLRSR			0x80
#define UFSHCI_UICCMD			0x90
#define UFSHCI_UICCMDARG1		0x94
#define UFSHCI_UICCMDARG2		0x98
#define UFSHCI_UICCMDARG3		0x9C

/* Bit field of UFSHCI_IS register */
#define BMSK_UTRCS			BIT(0)
#define BMSK_UDEPRI			BIT(1)
#define BMSK_UE				BIT(2)
#define BMSK_UTMS			BIT(3)
#define BMSK_UPMS			BIT(4)
#define BMSK_UHXS			BIT(5)
#define BMSK_UHES			BIT(6)
#define BMSK_ULLS			BIT(7)
#define BMSK_ULSS			BIT(8)
#define BMSK_UTMRCS			BIT(9)
#define BMSK_UCCS			BIT(10)
#define BMSK_DFES			BIT(11)
#define BMSK_UTPES			BIT(12)
#define BMSK_HCFES			BIT(16)
#define BMSK_SBFES			BIT(17)

#define UFS_IS_MASK_FATAL_ERROR		(BMSK_ULLS | BMSK_DFES | BMSK_HCFES | BMSK_SBFES)
#define UFS_IS_MASK_ALL_ERROR		(BMSK_UE | UFS_IS_MASK_FATAL_ERROR)

/* Bit field of UFSHCI_HCS register */
#define UPMCRS_SHIFT			8
#define BMSK_UPMCRS			(0x07 << UPMCRS_SHIFT)
#define UPMCRS_PWR_OK			0
#define UPMCRS_PWR_LOCAL		1
#define UPMCRS_PWR_REMOTE		2
#define UPMCRS_PWR_BUSY			3
#define UPMCRS_PWR_ERROR_CAP		4
#define UPMCRS_PWR_FATAL_ERROR		5
#define BMSK_UCRDY			BIT(3)
#define BMSK_UTMRLRDY			BIT(2)
#define BMSK_UTRLRDY			BIT(1)
#define BMSK_DP				BIT(0)

/* Bit field of UFSHCI_HCE register */
#define BMSK_HCE			BIT(0)

/* Bit field of UFSHCI_UTRLRSR register */
#define BMSK_RSR			BIT(0)

/* Bit field of UFSHCI_UICCMD register */
#define UICCMDR_DME_GET			0x01
#define UICCMDR_DME_SET			0x02
#define UICCMDR_DME_PEER_GET		0x03
#define UICCMDR_DME_PEER_SET		0x04
#define UICCMDR_DME_POWERON		0x10
#define UICCMDR_DME_POWEROFF		0x11
#define UICCMDR_DME_ENABLE		0x12
#define UICCMDR_DME_RESET		0x14
#define UICCMDR_DME_ENDPOINTRESET	0x15
#define UICCMDR_DME_LINKSTARTUP		0x16
#define UICCMDR_DME_HIBERNATE_ENTER	0x17
#define UICCMDR_DME_HIBERNATE_EXIT	0x18
#define UICCMDR_DME_TEST_MODE		0x1a

#define DME_ATTR_SET_TYPE_NORMAL	0
#define DME_ATTR_SET_TYPE_STATIC	1

#define UFS_MASK_UIC_ERROR		0xff

#define UFS_UECDL_PA_INIT_ERROR		BIT(13)
#define UFS_UECDL_FATAL_MSK		UFS_UECDL_PA_INIT_ERROR

/* Overall command status values */
typedef enum {
	OCS_SUCCESS			= 0x0,
	OCS_INVALID_CMD_TABLE_ATTR	= 0x1,
	OCS_INVALID_PRDT_ATTR		= 0x2,
	OCS_MISMATCH_DATA_BUF_SIZE	= 0x3,
	OCS_MISMATCH_RESP_UPIU_SIZE	= 0x4,
	OCS_PEER_COMM_FAILURE		= 0x5,
	OCS_ABORTED			= 0x6,
	OCS_FATAL_ERROR			= 0x7,
	OCS_DEVICE_FATAL_ERROR		= 0x8,
	OCS_INVALID_CRYPTO_CONFIG	= 0x9,
	OCS_GENERAL_CRYPTO_ERROR	= 0xA,
	OCS_INVALID_COMMAND_STATUS	= 0xF,
	OCS_MASK			= 0xF,
} utp_ocs_t;

// L1.5 - PHY Adapter
#define PA_ACTIVETXDATALANES		0x1560
#define PA_ACTIVERXDATALANES		0x1580
#define PA_TXTRAILINGCLOCKS		0x1564
#define PA_PHY_TYPE			0x1500
#define PA_AVAILTXDATALANES		0x1520
#define PA_AVAILRXDATALANES		0x1540
#define PA_MINRXTRAILINGCLOCKS		0x1543

#define PA_TXPWRSTATUS			0x1567
#define PA_RXPWRSTATUS			0x1582
#define PA_TXFORCECLOCK			0x1562
#define PA_TXPWRMODE			0x1563

#define PA_MAXTXSPEEDFAST		0x1521
#define PA_MAXTXSPEEDSLOW		0x1522
#define PA_MAXRXSPEEDFAST		0x1541
#define PA_MAXRXSPEEDSLOW		0x1542
#define PA_TXLINKSTARTUPHS		0x1544

#define PA_LOCAL_TX_LCC_ENABLE		0x155E
#define PA_PEER_TX_LCC_ENABLE		0x155F

#define PA_TXSPEEDFAST			0x1565
#define PA_TXSPEEDSLOW			0x1566
#define PA_REMOTEVERINFO		0x15A0

#define PA_TXGEAR			0x1568
#define PA_TXTERMINATION		0x1569
#define PA_HSSERIES			0x156A
#define PA_PWRMODE			0x1571
#define PA_RXGEAR			0x1583
#define PA_RXTERMINATION		0x1584
#define PA_MAXRXPWMGEAR			0x1586
#define PA_MAXRXHSGEAR			0x1587
#define PA_RXHSUNTERMINATIONCAPABILITY	0x15A5
#define PA_RxLSUNTERMINATIONCAPABILITY	0x15A6
#define PA_HIVERN8TIME			0x15A7
#define PA_TACTIVATE			0x15A8
#define PA_LOCALVERINFO			0x15A9
#define PA_PACPFRAMECOUNT		0x15C0
#define PA_PACPERRORCOUNT		0x15C1
#define PA_PHYTESTCONTROL		0x15C2
#define PA_CONNECTEDTXDATALANES		0x1561
#define PA_CONNECTEDRXDATALANES		0x1581
#define PA_LOGICALLANEMAP		0x15A1
#define PA_PWRMODEUSERDATA(X)		(0x15B0 + (X))

#define PA_SLEEPNOCONFIGTIME		0x15A2
#define PA_STALLNOCONFIGTIME		0x15A3
#define PA_SAVECONFIGTIME		0x15A4

#define PA_VS_CONFIG_REG1		0x9000

#define DME_LOCALFC0PROTECTIONTIMEOUTVAL 0xD041
#define DME_LOCALTC0REPLAYTIMEOUTVAL	 0xD042
#define DME_LOCALAFC0REQTIMEOUTVAL	 0xD043

// L1 - M-TX PHY
#define TX_HSMODE_CAPABILITY		0x01
#define TX_HSGEAR_CAPABILITY		0x02
#define TX_PWMG0_CAPABILITY		0x03
#define TX_PWMGEAR_CAPABILITY		0x04
#define TX_PHY_MAJMIN_REL_CAPABILITY	0x0d

#define TX_MODE				0x21
#define TX_HSRATE_SERIES		0x22
#define TX_HSGEAR			0x23
#define TX_PWMGEAR			0x24
#define TX_AMPLITUDE			0x25
#define TX_HS_SLEWRATE			0x26
#define TX_SYNC_SOURCE			0x27
#define TX_HS_SYNC_LENGTH		0x28
#define TX_HS_PREPARE_LENGTH		0x29
#define TX_LS_PREPARE_LENGTH		0x2A
#define TX_HIBERN8_CONTROL		0x2B
#define TX_LCC_ENABLE			0x2C
#define TX_PWM_BURST_CLOSURE_EXTENSION	0x2D
#define TX_BYPASS_8B10B_ENABLE		0x2E
#define TX_DRIVER_POLARITY		0x2F
#define TX_LCC_SEQUENCER		0x32
#define TX_MIN_ACTIVE_TIME		0x33

#define TX_FSM_STATE			0x41

// Default values from UniPro v1.61 Table 45 Data Link Layer (gettable, settable) Attributes
#define DL_FC0PROTECTIONTIMEOUTVAL	8191
#define DL_TC0REPLAYTIMEOUTVAL		65535
#define DL_AFC0REQTIMEOUTVAL		32767
#define DL_FC1PROTECTIONTIMEOUTVAL	8191
#define DL_TC1REPLAYTIMEOUTVAL		65535
#define DL_AFC1REQTIMEOUTVAL		32767

/**************** End HCI definitions *********************/

// Timeouts
#define HCI_DISABLE_TIMEOUT_US		( 10 * USECS_PER_MSEC)
#define HCI_ENABLE_TIMEOUT_US		(100 * USECS_PER_MSEC)
#define HCI_LINK_STARTUP_TIMEOUT_US	( 10 * USECS_PER_MSEC)
#define HCI_UIC_TIMEOUT_US		(100 * USECS_PER_MSEC)
#define HCI_UTRD_POLL_TIMEOUT_US	( 30 * USECS_PER_SEC)
#define UFS_HCI_UPMS_POLL_TIMEOUT_US	(500 * USECS_PER_MSEC)
#define UFS_DEVICEINIT_TIMEOUT_US	(  5 * USECS_PER_SEC)

// Retry counts
#define MAX_UFS_NOP_PING_RETRY		10
#define UFS_LINK_STARTUP_RETRY		5
#define UIC_COMMAND_RETRY		3

// Retry after failure to initialize
#define UFS_INIT_RETRY			5

#define UPIU_STD_HDR_LEN		0x20
#define UPIU_TYPE_NOP_OUT		0x00
#define UPIU_TYPE_NOP_IN		0x20
#define UPIU_TYPE_COMMAND		0x01
#define UPIU_TYPE_RESPONSE		0x21
#define UPIU_TYPE_QUERY_REQ		0x16
#define UPIU_TYPE_QUERY_RESP		0x36
#define UPIU_TYPE_REJECT		0x3F
#define UPIU_TYPE_TASK_MAN		0x04
#define UPIU_TYPE_TASK_MAN_RESP		0x24
// data_dir field
#define UTRD_DIR_NODATA			0x0
#define UTRD_DIR_WRITE			0x1
#define UTRD_DIR_READ			0x2
// intr field
#define UTRD_INTR_AGGREGATED		0x0
#define UTRD_INTR_NORMAL		0x1
// cmd_type field
#define UTRD_CMDTYPE_SCSI		0x0
#define UTRD_CMDTYPE_NATIVE_UFS		0x1
#define UTRD_CMDTYPE_MANAGEMENT		0x2
#define UTRD_CMDTYPE_UFS_STORAGE	0x1

#define UFS_CDB_SZ			16

#define UPIU_QUERY_OP_NOP		0x0
#define UPIU_QUERY_OP_READ_DESCRIPTOR	0x1
#define UPIU_QUERY_OP_WRITE_DESCRIPTOR	0x2
#define UPIU_QUERY_OP_READ_ATTRIBUTE	0x3
#define UPIU_QUERY_OP_WRITE_ATTRIBUTE	0x4
#define UPIU_QUERY_OP_READ_FLAG		0x5
#define UPIU_QUERY_OP_SET_FLAG		0x6
#define UPIU_QUERY_OP_CLEAR_FLAG	0x7
#define UPIU_QUERY_OP_TOGGLE_FLAG	0x8

#define UFS_DESCRIPTOR_MAX_SIZE		255

// Query Response Code
#define UPIU_RESP_SUCCESS		0x0
#define UPIU_RESP_NOT_READABLE		0xF6
#define UPIU_RESP_NOT_WRITEABLE		0xF7
#define UPIU_RESP_ALREADY_WRITTEN	0xF8
#define UPIU_RESP_INVALID_LEN		0xF9
#define UPIU_RESP_INVALID_VAL		0xFA
#define UPIU_RESP_INVALID_SELECTOR	0xFB
#define UPIU_RESP_INVALID_INDEX		0xFC
#define UPIU_RESP_INVALID_IDN		0xFD
#define UPIU_RESP_INVALID_OPCODE	0xFE
#define UPIU_RESP_FAILED		0xFF

// Query function define
#define UPIU_FNC_STD_READ		0x01
#define UPIU_FNC_STD_WRITE		0x81

// FLAGS for indication of read or write
#define UPIU_FLAGS_READ			0x40
#define UPIU_FLAGS_WRITE		0x20
#define UPIU_FLAGS_ATTR_SIMPLE		0x00
#define UPIU_FLAGS_ATTR_ORDERED		0x01
#define UPIU_FLAGS_ATTR_HOQ		0x02

// Redefine the name for higher layer use
#define UFS_XFER_FLAGS_READ		UPIU_FLAGS_READ
#define UFS_XFER_FLAGS_WRITE		UPIU_FLAGS_WRITE
#define UFS_XFER_FLAGS_ATTR_SIMPLE	UPIU_FLAGS_ATTR_SIMPLE
#define UFS_XFER_FLAGS_ATTR_ORDERED	UPIU_FLAGS_ATTR_ORDERED
#define UFS_XFER_FLAGS_ATTR_HOQ		UPIU_FLAGS_ATTR_HOQ

// Flags definitions
#define UFS_IDN_FDEVICEINIT		0x01
#define UFS_IDN_FPERMANENTWPEN		0x02
#define UFS_IDN_FPOWERONWPEN		0x03
#define UFS_IDN_FPURGEENABLE		0x06

// Define SCSI commands
#define SCSI_CMD_TEST_UNIT_RDY		0x00
#define SCSI_CMD_REQUEST_SENSE		0x03
#define SCSI_CMD_FORMAT			0x04
#define SCSI_CMD_INQUIRY		0x12
#define SCSI_CMD_START_STOP_UNIT	0x1B
#define SCSI_CMD_READ_CAPACITY10	0x25
#define SCSI_CMD_READ10			0x28
#define SCSI_CMD_WRITE10		0x2A
#define SCSI_CMD_SYNC_CACHE10		0x35
#define SCSI_CMD_WRITE_BUFFER		0x3B
#define SCSI_CMD_READ_BUFFER		0x3C
#define SCSI_CMD_UNMAP			0x42
#define SCSI_CMD_REPORT_LUNS		0xA0

// SCSI status value
#define SCSI_STATUS_GOOD		0x00
#define SCSI_STATUS_CHK_COND		0x02
#define SCSI_STATUS_BUSY		0x08
#define SCSI_STATUS_TASK_SET_FULL	0x28

#define SCSI_FLAG_FUA			0x08

// SCSI Sense Keys
#define SENSE_KEY_NO_SENSE		0x0
#define SENSE_KEY_RECOVERED_ERROR	0x1
#define SENSE_KEY_NOT_READY		0x2
#define SENSE_KEY_MEDIUM_ERROR		0x3
#define SENSE_KEY_HARDWARE_ERROR	0x4
#define SENSE_KEY_ILLEGAL_REQUEST	0x5
#define SENSE_KEY_UNIT_ATTENTION	0x6
#define SENSE_KEY_DATA_PROTECT		0x7
#define SENSE_KEY_BLANK_CHECK		0x8
#define SENSE_KEY_VENDOR_SPECIFIC	0x9
#define SENSE_KEY_ABORTED_COMMAND	0xB
#define SENSE_KEY_VOLUME_OVERFLOW	0xD
#define SENSE_KEY_MISCOMPARE		0xE
#define SCSI_SENSE_FIXED_FORMAT		0x70
#define SCSI_SENSE_SIZE			96
#define SCSI_INQUIRY_SIZE		36
#define UFS_SENSE_SIZE			18
// Sense data including 2-byte length
#define UFS_SENSE_DATA_SIZE		(UFS_SENSE_SIZE + 2)

#define UFS_1_LANES			1
#define UFS_2_LANES			2
#define UFS_PWM_GEAR1			1
#define UFS_PWM_GEAR2			2
#define UFS_PWM_GEAR3			3
#define UFS_PWM_GEAR4			4
#define UFS_PWM_GEAR5			5
#define UFS_PWM_GEAR6			6
#define UFS_PWM_GEAR7			7
#define UFS_GEAR1			1
#define UFS_GEAR2			2
#define UFS_GEAR3			3
#define UFS_GEAR4			4
#define UFS_GEAR5			5
#define UFS_FAST_MODE			1
#define UFS_SLOW_MODE			2
#define UFS_FASTAUTO_MODE		4
#define UFS_SLOWAUTO_MODE		5
#define UFS_PHY_TERM_HS			0x01
#define UFS_PHY_TERM_LOW		0x00
#define UFS_HS_SERIES_A			1
#define UFS_HS_SERIES_B			2

#define UPIU_CMDTYPE_SCSI		0x0
#define UPIU_CMDTYPE_NATIVE_UFS		0x1

// Define all the basic WLUN type
#define UFS_WLUN_REPORT			0x81
#define UFS_WLUN_DEVICE			0xD0
#define UFS_WLUN_BOOT			0xB0
#define UFS_WLUN_RPMB			0xC4

// UFS descriptor type ident value
#define UFS_DESC_IDN_DEVICE		0x00
#define UFS_DESC_IDN_CONFIGURATION	0x01
#define UFS_DESC_IDN_UNIT		0x02
#define UFS_DESC_IDN_INTERCONNECT	0x04
#define UFS_DESC_IDN_STRING		0x05
#define UFS_DESC_IDN_GEOMETRY		0x07
#define UFS_DESC_IDN_POWER		0x08

#define UFS_DESC_IDN_BYTE		1

// Attributes definitions
#define UFS_IDN_BBOOTLUNEN		0x00
#define UFS_IDN_BCURRENTPOWERMODE	0X01
#define UFS_IDN_BACTIVEICCLEVEL		0x03
#define UFS_IDN_BPURGESTATUS		0x06
#define UFS_IDN_BREFCLKFREQ		0x0A
#define UFS_IDN_BCONFIGDESCRLOCK	0x0B

#define UFS_REFCLKFREQ_19_2		0
#define UFS_REFCLKFREQ_26		1
#define UFS_REFCLKFREQ_38_4		2
#define UFS_REFCLKFREQ_52		3

// Memory size parameters

// UTRL has 32 slots, 32 bytes each
#define UFS_REQ_LIST_SZ			1024
// UPIU big enough to contain e.g. 255 byte descriptor
// Use a power of 2 to meet alignment requirements
#define UFS_MAX_UPIU_SZ			512
// Command UPIU size
#define UFS_CMD_UPIU_LEN		UFS_MAX_UPIU_SZ
// Response UPIU size
#define UFS_RESP_UPIU_LEN		UFS_MAX_UPIU_SZ
// Response UPIU offset (directly after Command UPIU)
#define UFS_RESP_UPIU_OFFS		UFS_CMD_UPIU_LEN
// Physical Region Descriptor Table offset (directly after Response UPIU)
#define UFS_PRDT_OFFS			(UFS_RESP_UPIU_OFFS + UFS_RESP_UPIU_LEN)

// Maximum data transfer length for a single PRDT entry is 256 KiB
#define PRDT_DBC_MAX			0x40000
// PRDT entry is 4 double words
#define PRDT_ENTRY_SZ			16
// SCSI READ (10) / WRITE (10) are limited to 65535 logical blocks.
// Assuming at least 4 KiB logical block size, that needs 1024 PRDT entries
#define MAX_PRDT_ENTRIES		1024
// Size of single entry PRDT
#define UFS_PRDT_SZ			(MAX_PRDT_ENTRIES * PRDT_ENTRY_SZ)
// UTP Command Descriptor is 2 UPIU and 1 PRDT
#define UFS_UCD_SZ			(UFS_CMD_UPIU_LEN + UFS_RESP_UPIU_LEN + UFS_PRDT_SZ)
// Memory size for Request List plus one UTP Command Descriptor
#define UFS_MEM_SZ			(UFS_REQ_LIST_SZ + UFS_UCD_SZ)
// UFSHCI requires 1KB alignment for Request List
#define UFS_DMA_ALIGN			1024

// Maximum number of LUNs (excluding Well-Known LUNs)
#define MAX_LUN				32

// Default Request List slot
#define UFS_DFLT_TAG			0

#define ufs_err(fmt, rc, args...) ({				\
	printf("%s: " fmt ", error %d\n", __func__, ## args, rc);	\
	rc;							\
})

// Return codes
enum {
	UFS_ENOENT = -100,
	UFS_EIO,
	UFS_ENOMEM,
	UFS_EBUSY,
	UFS_ENODEV,
	UFS_EINVAL,
	UFS_EPROTO,
	UFS_ETIMEDOUT,
	UFS_ECHKCND,
	UFS_ENOLUN,
	UFS_EUAC,
};

// JESD220B Table14.4 - Device Descriptor (big-endian)
typedef struct __packed {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint8_t		bDevice;
	uint8_t		bDeviceClass;
	uint8_t		bDeviceSubClass;
	uint8_t		bProtocol;
	uint8_t		bNumberLU;
	uint8_t		bNumberWLU;
	uint8_t		bBootEnable;
	uint8_t		bDescrAccessEn;
	uint8_t		bInitPowerMode;
	uint8_t		bHighPriorityLUN;
	uint8_t		bSecureRemovalType;
	uint8_t		bSecurityLU;
	uint8_t		bBackgroundOpsTermLat;
	uint8_t		bInitActiveICCLevel;
	uint16_t	wSpecVersion;
	uint16_t	wManufactureDate;
	uint8_t		iManufacturerName;
	uint8_t		iProductName;
	uint8_t		iSerialNumber;
	uint8_t		iOemID;
	uint16_t	wManufacturerID;
	uint8_t		bUD0BaseOffset;
	uint8_t		bUDConfigPLength;
	uint8_t		bDeviceRTTCap;
	uint16_t	wPeriodicRTCUpdate;
} UfsDescDev;

// JESD220B Table 14.10 - Unit Descriptor (big-endian)
typedef struct __packed {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint8_t		bUnitIndex;
	uint8_t		bLUEnable;
	uint8_t		bBootLunID;
	uint8_t		bLUWriteProtect;
	uint8_t		bLUQueueDepth;
	uint8_t		bPSASensitive;
	uint8_t		bMemoryType;
	uint8_t		bDataReliability;
	uint8_t		bLogicalBlockSize;
	uint64_t	qLogicalBlockCount;
	uint32_t	dEraseBlockSize;
	uint8_t		bProvisioningType;
	uint64_t	qPhyMemResourceCount;
	uint16_t	wContextCapabilities;
	uint8_t		bLargeUnitGranularity_M1;
} UfsDescUnit;

// JESD223D Section 6.1.1 UTP Transfer Request Descriptor (little-endian)
typedef struct __packed {
	uint8_t		cci;
	uint8_t		rsrvd1;
	uint8_t		rsrvd2	:  7;
	uint8_t		ce	:  1;
	uint8_t		intr	:  1;
	uint8_t		ddir	:  2;
	uint8_t		rsrvd3	:  1;
	uint8_t		ctyp	:  4;
	uint32_t	dunl;
	uint8_t		ocs;
	uint8_t		rsrvd4[3];
	uint32_t	dunu;
	uint32_t	ucdbal;
	uint32_t	ucdbau;
	uint16_t	resp_len;
	uint16_t	resp_offs;
	uint16_t	prdt_len;
	uint16_t	prdt_offs;
} UfsUTRD;

// JESD223D Figure 7 Physical Region Table (little-endian)
typedef struct __packed {
	uint64_t	base_addr;
	uint32_t	rsrvd;
	uint32_t	byte_cnt;
} UfsPRDT;

// JESD220E Table 10.28 Query Request UPIU (big-endian)
// JESD220E Table 10.41 Query Response UPIU (big-endian)
typedef struct __packed {
	uint8_t		type;
	uint8_t		flags;
	uint8_t		rsrvd1;
	uint8_t		task_tag;
	uint8_t		rsrvd2;
	uint8_t		func;
	uint8_t		response;
	uint8_t		rsrvd3;
	uint8_t		ehs_len;
	uint8_t		dev_info;
	uint16_t	data_segment_len;
	uint8_t		op;
	uint8_t		idn;
	uint8_t		idx;
	uint8_t		sel;
	uint8_t		rsrvd4[2];
	uint16_t	data_len;
	union {
		uint32_t	attr_val;
		struct {
			uint8_t	rsrvd5[3];
			uint8_t	flag_val;
		};
	};
	uint8_t		rsrvd6[8];
	uint8_t		data[];
} UfsQUPIU;

// Query Request UPIU and Query Response UPIU can be combined
typedef UfsQUPIU UfsQRespUPIU;

// JESD220E Table 10.11 Command UPIU (big-endian)
typedef struct __packed {
	uint8_t		type;
	uint8_t		flags;
	uint8_t		lun;
	uint8_t		task_tag;
	uint8_t		iid_cmd_set;
	uint8_t		rsrvd1[3];
	uint8_t		ehs_len;
	uint8_t		rsrvd2;
	uint16_t	data_segment_len;
	uint32_t	tfr_len;
	uint8_t		cdb[UFS_CDB_SZ];
} UfsCUPIU;

// JESD220E Table 10.13 Response UPIU (big-endian)
typedef struct __packed {
	uint8_t		type;
	uint8_t		flags;
	uint8_t		lun;
	uint8_t		task_tag;
	uint8_t		iid_cmd_set;
	uint8_t		rsrvd1;
	uint8_t		response;
	uint8_t		status;
	uint8_t		ehs_len;
	uint8_t		dev_info;
	uint16_t	data_segment_len;
	uint32_t	residual;
	uint8_t		cdb[UFS_CDB_SZ];
	uint8_t		data[];
} UfsCRespUPIU;

// JESD220E Table 10.17 SCSI fixed format sense data
typedef struct __packed {
	uint8_t		response_code	:  7;
	uint8_t		valid		:  1;
	uint8_t		not_used;
	uint8_t		sense_key	:  4;
	uint8_t		rsrvd1		:  1;
	uint8_t		ili		:  1;
	uint8_t		eom		:  1;
	uint8_t		filemark	:  1;
	uint8_t		information[4];
	uint8_t		addnl_length;
	uint8_t		rsrvd2[4];
	uint8_t		asc;
	uint8_t		ascq;
	uint8_t		fruc;
	uint8_t		rsrvd3		:  7;
	uint8_t		sksv		:  1;
	uint8_t		rsrvd4[2];
} UfsSense;

// Sense Data Segment from JESD220E Table 10.13 Response UPIU (big-endian)
typedef struct __packed {
	uint16_t	len;
	UfsSense	sense;
} UfsSenseData;

// Generic descriptor container
typedef struct UfsDesc {
	bool		read_already;		// Has descriptor been read
	uint8_t		len;			// Returned actual length of descriptor
	uint8_t		raw[UFS_DESCRIPTOR_MAX_SIZE];// Raw data (big-endian)
} UfsDesc;

// Receiver / transmitter settings
typedef struct UfsGear {
	uint32_t	pwr_mode;		// UIC Power Mode
	uint32_t	lanes;			// Number of lanes
	uint32_t	gear;			// Gear
} UfsGear;

// Transfer mode (gear, lanes etc)
typedef struct UfsTfrMode {
	bool		initialized;		// Is initialized
	int		hs_series;		// High Speed Series
	UfsGear		rx;			// Receiver
	UfsGear		tx;			// Transmitter
} UfsTfrMode;

// Error register values
typedef struct UfsErrRegs {
	uint32_t	uecpa;			// PHY Adapter Layer errors
	uint32_t	uecdl;			// Data Link Layer errors
	uint32_t	uecn;			// Network Layer errors
	uint32_t	uect;			// Transport Layer errors
	uint32_t	uecdme;			// UniPro DME errors
} UfsErrRegs;

struct UfsCtlr;
typedef struct UfsCtlr UfsCtlr;

// UFS Logical Unit
typedef struct UfsDevice {
	BlockDev	dev;			// Block device
	UfsCtlr		*ufs;			// UFS Controller
	int		lun;			// Logical Unit Number
	UfsDesc		unit_desc;		// Unit Descriptor
} UfsDevice;

// Hook operations
typedef enum {
	UFS_OP_PRE_HCE,
	UFS_OP_PRE_LINK_STARTUP,
	UFS_OP_POST_LINK_STARTUP,
	UFS_OP_PRE_GEAR_SWITCH,
	UFS_OP_POST_GEAR_SWITCH,
} UfsHookOp;

typedef int (*UFSHookFn)(UfsCtlr *ufs, UfsHookOp op, void *data);

// UFS Host Controller
typedef struct UfsCtlr {
	BlockDevCtrlr	bctlr;			// Block device controller
	UFSHookFn	hook_fn;		// Hook callback function
	void		*hci_base;		// MMIO address
	bool		update_refclkfreq;	// Whether to update bRefClkFreq attribute
	uint8_t		refclkfreq;		// bRefClkFreq attribute value
	UfsTfrMode	tfr_mode;		// Transfer mode (gear, lanes etc)
	uint8_t		*ufs_req_list;		// Request List
	bool		ctlr_initialized;	// Controller is initialized
	UfsDesc		dev_desc;		// Device Descriptor
	UfsDevice	*ufs_dev[MAX_LUN];	// Block devices
	UfsDevice	*ufs_wlun_dev;		// Device Well Known LUN
} UfsCtlr;

// Use with Command UPIU request
typedef struct UfsCmdReq {
	uint8_t		lun;			// Target LUN to issue xfer to
	uint8_t		flags;			// Read or write
	uint32_t	expected_len;		// Expected total length
	uint64_t	data_buf_phy;		// Physical buffer address
	uint8_t		cdb[UFS_CDB_SZ];	// Command Descriptor Block
} UfsCmdReq;

// For Query UPIU request and NOP OUT
typedef struct UfsQryReq {
	uint8_t		type;			// Transaction Type
	uint8_t		resp_type;		// Response Type
	uint8_t		idn;			// Identification field, input
	uint8_t		idx;			// Index field, input
	uint8_t		sel;			// Selection field, input
	uint32_t	val;			// Value field, in/out
	uint16_t	resp_data_len;		// Response Length, used for descriptor
	uint8_t		*resp_data_buf;		// Response Data, used for descriptor
} UfsQryReq;

#define MIBattrSel(Id, Gs)	((((uint32_t)(Id) << 16) & 0xffff0000) | (Gs))
#define MIBattr(Id)		MIBattrSel((Id), 0)
#define AttrSetType(t)		(((t) & 0xff) << 16)

int32_t ufs_utp_uic_getset(UfsCtlr *ufs, uint32_t uic, uint32_t mib_attr_idx,
			   uint32_t mib_val_set, uint32_t *mib_val_get);

static inline int ufs_dme_get(UfsCtlr *ufs, int id, uint32_t *val)
{
	return ufs_utp_uic_getset(ufs, UICCMDR_DME_GET, MIBattr(id), 0, val);
}

static inline int ufs_dme_peer_get(UfsCtlr *ufs, int id, uint32_t *val)
{
	return ufs_utp_uic_getset(ufs, UICCMDR_DME_PEER_GET, MIBattr(id), 0, val);
}

static inline int ufs_dme_set(UfsCtlr *ufs, int id, uint32_t val)
{
	return ufs_utp_uic_getset(ufs, UICCMDR_DME_SET, MIBattr(id), val, NULL);
}

static inline int ufs_dme_peer_set(UfsCtlr *ufs, int id, uint32_t val)
{
	return ufs_utp_uic_getset(ufs, UICCMDR_DME_PEER_SET, MIBattr(id), val, NULL);
}

static inline uint32_t ufs_read32(UfsCtlr *ufs, int reg)
{
	return read32(ufs->hci_base + reg);
}

static inline void ufs_write32(UfsCtlr *ufs, int reg, uint32_t val)
{
	write32(ufs->hci_base + reg, val);
}

static inline UfsDescDev *ufs_dd(UfsCtlr *ufs)
{
	return (UfsDescDev *)ufs->dev_desc.raw;
}

static inline UfsDescUnit *ufs_ud(UfsDevice *ufs_dev)
{
	return (UfsDescUnit *)ufs_dev->unit_desc.raw;
}

int ufs_update(BlockDevCtrlrOps *bdev_ops);

#endif //__DRIVERS_STORAGE_UFS_H__
