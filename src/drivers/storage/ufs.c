// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Driver
 *
 * Copyright (C) 2018, The Linux Foundation.  All rights reserved.
 * Copyright (C) 2022, Intel Corporation.
 */

/*
 * This driver implements a minimal subset of UFS for depthcharge.
 * Features:
 *	host controller initialization
 *	retry initialization upon failure
 *	selects maximum gear and lanes
 *	hooks for customization
 *	enumerates logical units
 *	support for SCSI READ (10) / WRITE (10)
 *	retry SCSI commands upon Unit Attention Condition
 *	up to slightly under 256 MiB per data transfer
 * Caveats / not supported:
 *	no error recovery
 *	non-fatal errors are ignored
 *	only transfer list slot 0 is used
 *	no task management support
 *	no support for changing UFS device power mode (it is assumed to be active)
 *	DEVICE WLUN, BOOT WLUN and RPMB WLUN are not supported
 *	advanced UFS features are not supported
 */

#include <arch/barrier.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libpayload.h>
#include "base/container_of.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/ufs.h"

#define UFS_DEBUG 0

static inline int ufs_hook(UfsCtlr *ufs, UfsHookOp op, void *data)
{
	if (ufs->hook_fn)
		return ufs->hook_fn(ufs, op, data);
	return 0;
}

static void ufs_read_err_regs(UfsCtlr *ufs, UfsErrRegs *regs)
{
	// Error registers are ROC (Read Only and Read to clear)
	regs->uecpa  = ufs_read32(ufs, UFSHCI_UECPA);
	regs->uecdl  = ufs_read32(ufs, UFSHCI_UECDL);
	regs->uecn   = ufs_read32(ufs, UFSHCI_UECN);
	regs->uect   = ufs_read32(ufs, UFSHCI_UECT);
	regs->uecdme = ufs_read32(ufs, UFSHCI_UECDME);

	if (!UFS_DEBUG)
		return;

	if (regs->uecpa || regs->uecdl || regs->uecn || regs->uect || regs->uecdme) {
		printf("UECPA:%x,UECDL:%x,UECN:%x,UECT:%x,UECDME:%x\n",
		       regs->uecpa, regs->uecdl, regs->uecn, regs->uect, regs->uecdme);
	}
}

static int ufs_poll_completion(UfsCtlr *ufs, uint32_t mask, uint32_t drbl, uint64_t timeout_us)
{
	uint64_t start = timer_us(0);
	UfsErrRegs regs;
	uint32_t status;

	while (1) {
		bool timed_out = timer_us(start) > timeout_us;

		status = ufs_read32(ufs, UFSHCI_IS);
		// Check for success (including once after timed_out is true)
		if ((status & mask) && (!drbl || !(ufs_read32(ufs, UFSHCI_UTRLDBR) & drbl))) {
			// UFSHCI_IS is RWC i.e. write 1's to clear bits
			ufs_write32(ufs, UFSHCI_IS, mask);
			return 0;
		}
		// Return timed out error only after checking for success
		if (timed_out)
			return ufs_err("Timed out", UFS_ETIMEDOUT);
		if (status & UFS_IS_MASK_ALL_ERROR) {
			// UFSHCI_IS is RWC i.e. write 1's to clear bits
			ufs_write32(ufs, UFSHCI_IS, UFS_IS_MASK_ALL_ERROR);
			// Read and (if UFS_DEBUG) dump error registers
			ufs_read_err_regs(ufs, &regs);
			// Check for fatal error interrupts
			if (status & UFS_IS_MASK_FATAL_ERROR)
				return ufs_err("Fatal error status %#x", UFS_EIO, status);
			// Check for fatal data link errors
			if (regs.uecdl & UFS_UECDL_FATAL_MSK)
				return ufs_err("Fatal data link error", UFS_EIO);
		}
	}
}

static int ufs_wait(UfsCtlr *ufs, int reg, uint32_t mask, uint32_t val, uint64_t timeout_us)
{
	uint64_t start = timer_us(0);

	while (1) {
		bool timed_out = timer_us(start) > timeout_us;

		// Check for success (including once after timed_out is true)
		if ((ufs_read32(ufs, reg) & mask) == val)
			return 0;
		// Return timed out error only after checking for success
		if (timed_out)
			return UFS_ETIMEDOUT;
	}
}

// Sent a UIC command and wait for completion
// attr[31:16] = AttributeID
// attr[15:0] = GenSelectorIndex
// mib_val_set, mib_val_get = the attribute value
int ufs_utp_uic_do_getset(UfsCtlr *ufs, uint32_t uic, uint32_t attr,
			  uint32_t mib_val_set, uint32_t *mib_val_get)
{
	uint32_t arg2;
	uint32_t re;
	int rc;

	// Check to make sure hardware is ready to accept a UIC command
	rc = ufs_wait(ufs, UFSHCI_HCS, BMSK_UCRDY, BMSK_UCRDY, HCI_UIC_TIMEOUT_US);
	if (rc)
		return ufs_err("UCRDY timed out cmd %#x attr %#x", UFS_EIO, uic, attr);

	if (uic == UICCMDR_DME_SET || uic == UICCMDR_DME_PEER_SET)
		arg2 = AttrSetType(DME_ATTR_SET_TYPE_NORMAL); // Supporting normal only
	else
		arg2 = 0; // Reserved

	// Clear the UCCS bit in IS register
	// Note, UFSHCI_IS is RWC i.e. write 1's to clear bits
	ufs_write32(ufs, UFSHCI_IS, BMSK_UCCS);
	// Set UIC COMMAND Argument field
	ufs_write32(ufs, UFSHCI_UICCMDARG1, attr);
	ufs_write32(ufs, UFSHCI_UICCMDARG2, arg2);
	ufs_write32(ufs, UFSHCI_UICCMDARG3, mib_val_set);
	// Send the UIC command
	ufs_write32(ufs, UFSHCI_UICCMD, uic);

	// Poll the IS register for UCCS bit
	rc = ufs_poll_completion(ufs, BMSK_UCCS, 0, HCI_UIC_TIMEOUT_US);
	if (rc)
		return ufs_err("UIC failed, cmd %#x attr %#x", UFS_EIO, uic, attr);

	// Check the result code
	re = ufs_read32(ufs, UFSHCI_UICCMDARG2);
	if (re & UFS_MASK_UIC_ERROR)
		return ufs_err("UIC error %#x cmd %#x attr %#x", UFS_EIO, re, uic, attr);

	if (mib_val_get)
		*mib_val_get = ufs_read32(ufs, UFSHCI_UICCMDARG3);

	return 0;
}

int ufs_utp_uic_getset(UfsCtlr *ufs, uint32_t uic, uint32_t attr,
		       uint32_t mib_val_set, uint32_t *mib_val_get)
{
	int retries = 0;
	int rc;

	// UIC Command Error Handling: retry DME_PEER_GET / DME_PEER_GET
	if (uic == UICCMDR_DME_PEER_GET || uic == UICCMDR_DME_PEER_SET)
		retries = UIC_COMMAND_RETRY;

	do {
		rc = ufs_utp_uic_do_getset(ufs, uic, attr, mib_val_set, mib_val_get);
	} while (rc && --retries > 0);

	return rc;
}

static int ufs_utp_submit(UfsCtlr *ufs, uint32_t drbl)
{
	// Clear any interrupt pending
	// Note, UFSHCI_IS is RWC i.e. write 1's to clear bits
	ufs_write32(ufs, UFSHCI_IS, BMSK_UE | BMSK_UTRCS);

	// Check if UTRLRSR is set
	if (!(ufs_read32(ufs, UFSHCI_UTRLRSR)))
		return ufs_err("UFSHCI_UTRLRSR not set", UFS_EIO);

	// Trigger the transfer by ufs_write32 to doorbell register
	ufs_write32(ufs, UFSHCI_UTRLDBR, drbl);

	return 0;
}

static int ufs_process_request(UfsCtlr *ufs, int tag)
{
	uint32_t drbl = 1 << tag;
	int rc;

	rc = ufs_utp_submit(ufs, drbl);
	if (rc)
		return ufs_err("Submit failed", rc);

	rc = ufs_poll_completion(ufs, BMSK_UTRCS, drbl, HCI_UTRD_POLL_TIMEOUT_US);
	if (rc) {
		// A transfer is aborted by writing 0 to the corresponding bit
		// in UTRL Clear Register.
		ufs_write32(ufs, UFSHCI_UTRLCLR, ~drbl);
		return ufs_err("Completion failed", rc);
	}

	return 0;
}

static uint8_t ufs_query_op_to_fnc(uint8_t op)
{
	switch (op) {
	case UPIU_QUERY_OP_READ_DESCRIPTOR:
	case UPIU_QUERY_OP_READ_ATTRIBUTE:
	case UPIU_QUERY_OP_READ_FLAG:
		return UPIU_FNC_STD_READ;
	default:
		return UPIU_FNC_STD_WRITE;
	}
}

static inline uint64_t ufs_ucdba(UfsCtlr *ufs, int tag)
{
	return (uint64_t)virt_to_phys(ufs->ufs_req_list) + UFS_REQ_LIST_SZ + UFS_UCD_SZ * tag;
}

static inline void *ufs_ucd(UfsCtlr *ufs, int tag)
{
	return (void *)ufs->ufs_req_list + UFS_REQ_LIST_SZ + UFS_UCD_SZ * tag;
}

static UfsUTRD *ufs_utrd(UfsCtlr *ufs, int tag)
{
	UfsUTRD *req_list = (UfsUTRD *)ufs->ufs_req_list;
	UfsUTRD *utrd = &req_list[tag];
	uint64_t ucdba = ufs_ucdba(ufs, tag);

	memset(utrd, 0, sizeof(*utrd));

	// Set fixed values. Data transfer requires also ddir and prdt_len.
	utrd->intr	= UTRD_INTR_NORMAL;
	utrd->ctyp	= UTRD_CMDTYPE_UFS_STORAGE;
	utrd->ocs	= OCS_INVALID_COMMAND_STATUS;
	utrd->ucdbal    = htole32(ucdba);
	utrd->ucdbau    = htole32(ucdba >> 32);
	utrd->resp_len	= htole16(UFS_RESP_UPIU_LEN / 4);
	utrd->resp_offs = htole16(UFS_RESP_UPIU_OFFS / 4);
	utrd->prdt_offs = htole16(UFS_PRDT_OFFS / 4);

	return utrd;
}

// Process query requests and NOP OUT
static int ufs_dev_op(UfsCtlr *ufs, UfsQryReq *req, uint8_t op)
{
	int tag = UFS_DFLT_TAG;
	UfsQUPIU *c = ufs_ucd(ufs, tag);
	UfsQRespUPIU *r = (void *)c + UFS_RESP_UPIU_OFFS;
	UfsUTRD *utrd = ufs_utrd(ufs, tag);
	int rc;

	memset(c, 0, UFS_CMD_UPIU_LEN);

	c->type		= req->type;
	c->task_tag	= tag;

	if (req->type == UPIU_TYPE_QUERY_REQ) {
		c->func		= ufs_query_op_to_fnc(op);
		c->op		= op;
		c->idn		= req->idn;
		c->idx		= req->idx;
		c->sel		= req->sel;
		c->data_len	= htobe16(req->resp_data_len);
	}

	memset(r, 0, UFS_RESP_UPIU_LEN);

	rc = ufs_process_request(ufs, tag);
	if (rc)
		return rc;

	// Check Overall Command Status
	if (utrd->ocs)
		return ufs_err("OCS error %#x", UFS_EIO, utrd->ocs);

	// Check to make sure the response is the correct response type
	if (r->type != req->resp_type)
		return ufs_err("Invalid response type %#x", UFS_EPROTO, r->type);

	if (req->type != UPIU_TYPE_QUERY_REQ)
		return 0;

	// Check to opcode is correct and Query Response is SUCCESS
	if (r->op != op || r->response != UPIU_RESP_SUCCESS)
		return ufs_err("Error response %#x", UFS_EPROTO, r->response);

	// Copy response data
	if (req->resp_data_len && req->resp_data_buf)
		memcpy(req->resp_data_buf, &r->data, req->resp_data_len);

	// Extract the value and return it
	if (op == UPIU_QUERY_OP_READ_DESCRIPTOR)
		req->val = be16toh(r->data_len);
	else if (op == UPIU_QUERY_OP_READ_ATTRIBUTE)
		req->val = be32toh(r->attr_val);
	else if (op == UPIU_QUERY_OP_READ_FLAG)
		req->val = r->flag_val;

	return 0;
}

// UFS device query
static int ufs_dev_query_op(UfsCtlr *ufs, UfsQryReq *preq, uint8_t op)
{
	preq->type = UPIU_TYPE_QUERY_REQ;
	preq->resp_type = UPIU_TYPE_QUERY_RESP;
	return ufs_dev_op(ufs, preq, op);
}

/* Wait until device is responding to NOP UPIU pings */
static int ufs_wait_device_awake(UfsCtlr *ufs)
{
	UfsQryReq req = {
		.type      = UPIU_TYPE_NOP_OUT,
		.resp_type = UPIU_TYPE_NOP_IN,
	};
	int i, rc = -1;

	// Send NOP UPIU to check if the device is ready
	for (i = 0; i < MAX_UFS_NOP_PING_RETRY && rc; i++)
		rc = ufs_dev_op(ufs, &req, 0);

	return rc ? ufs_err("NOP OUT failed", rc) : 0;
}

// Set the fDeviceInit field in the flags and wait for the bit to clear
static int ufs_set_fDeviceInit(UfsCtlr *ufs)
{
	UfsQryReq req = {
		.idn = UFS_IDN_FDEVICEINIT,
	};
	uint64_t start;
	int rc;

	rc = ufs_dev_query_op(ufs, &req, UPIU_QUERY_OP_SET_FLAG);
	if (rc)
		return ufs_err("Failed to set fDeviceInit", rc);

	// Loop until flag is cleared
	start = timer_us(0);
	while (1) {
		bool timed_out = timer_us(start) > UFS_DEVICEINIT_TIMEOUT_US;

		rc = ufs_dev_query_op(ufs, &req, UPIU_QUERY_OP_READ_FLAG);
		if (rc)
			return ufs_err("Failed to read fDeviceInit", rc);
		// Check for success (including once after timed_out is true)
		if (req.val == 0)
			return 0;
		// Return timed out error only after checking for success
		if (timed_out)
			return ufs_err("DeviceInit timed out", UFS_ETIMEDOUT);
		// Avoid pummelling the device while it is initializing
		mdelay(1);
	};
}

static int ufs_scsi_process_sense(UfsCtlr *ufs, UfsCRespUPIU *r)
{
	uint16_t data_len = be16toh(r->data_segment_len);
	UfsSenseData *data = (UfsSenseData *)r->data;
	uint16_t sense_len = be16toh(data->len);
	UfsSense *s = &data->sense;

	// Check data segment length
	if (data_len < UFS_SENSE_DATA_SIZE)
		return ufs_err("Too little sense data (%u bytes)", UFS_EPROTO, data_len);

	// Check there is a valid sense data
	if (sense_len != UFS_SENSE_SIZE)
		return ufs_err("Invalid sense length (%u bytes)", UFS_EPROTO, sense_len);

	// Verify response code
	if (s->response_code != SCSI_SENSE_FIXED_FORMAT)
		return ufs_err("Invalid sense response code %#x", UFS_EPROTO, s->response_code);

	// UAC is a power on condition, not an error
	if (s->sense_key == SENSE_KEY_UNIT_ATTENTION)
		return UFS_EUAC;

	return ufs_err("Sense check failed, sense key %#x", UFS_EIO, s->sense_key);
}

static int ufs_build_prdt(UfsCtlr *ufs, UfsPRDT *prdt, uint64_t addr, uint32_t len)
{
	uint32_t byte_cnt;
	int cnt;

	for (cnt = 0; len; cnt++, prdt++) {
		byte_cnt = len > PRDT_DBC_MAX ? PRDT_DBC_MAX : len;
		prdt->base_addr = htole64(addr);
		prdt->rsrvd     = 0;
		prdt->byte_cnt  = htole32(byte_cnt - 1);
		addr += byte_cnt;
		len -= byte_cnt;
	}

	return cnt;
}

// Issue a SCSI command
static int ufs_do_scsi_command(UfsCtlr *ufs, UfsCmdReq *req)
{
	int tag = UFS_DFLT_TAG;
	UfsUTRD *utrd = ufs_utrd(ufs, tag);
	UfsCUPIU *c = ufs_ucd(ufs, tag);
	UfsCRespUPIU *r = (void *)c + UFS_RESP_UPIU_OFFS;
	UfsPRDT *prdt = (void *)c + UFS_PRDT_OFFS;
	uint16_t prdt_len;
	int rc;

	// Check the destination buffer for DWORD alignment
	if (!IS_ALIGNED(req->data_buf_phy, 4) || !IS_ALIGNED(req->expected_len, 4))
		return ufs_err("Data buffer not aligned to 4-byte boundary", UFS_EINVAL);

	memset(c, 0, UFS_CMD_UPIU_LEN);

	c->type		= UPIU_TYPE_COMMAND;
	c->flags	= req->flags;
	c->lun		= req->lun;
	c->task_tag	= tag;
	c->iid_cmd_set	= UPIU_CMDTYPE_SCSI;
	c->tfr_len	= htobe32(req->expected_len);

	memcpy(c->cdb, req->cdb, UFS_CDB_SZ);

	memset(r, 0, UFS_RESP_UPIU_LEN);

	if (req->flags & UFS_XFER_FLAGS_READ)
		utrd->ddir = UTRD_DIR_READ;
	else if (req->flags & UFS_XFER_FLAGS_WRITE)
		utrd->ddir = UTRD_DIR_WRITE;
	else
		utrd->ddir = UTRD_DIR_NODATA;

	// Build Physical Region Description Table
	prdt_len = ufs_build_prdt(ufs, prdt, req->data_buf_phy, req->expected_len);
	utrd->prdt_len = htole16(prdt_len);

	rc = ufs_process_request(ufs, tag);
	if (rc)
		return rc;

	// Check Overall Command Status
	if (utrd->ocs)
		return ufs_err("OCS error %#x", UFS_EIO, utrd->ocs);

	// Check to make sure the response is a query response type
	if (r->type != UPIU_TYPE_RESPONSE)
		return ufs_err("Invalid response type", UFS_EPROTO);

	// Check to see if there is a CHECK_CONDITION flag and valid payload
	if (r->status == SCSI_STATUS_CHK_COND && be16toh(r->data_segment_len) != 0)
		return ufs_scsi_process_sense(ufs, r);

	// Check to make sure the Response Code is SUCCESS
	if (r->response != UPIU_RESP_SUCCESS)
		return ufs_err("Error response %#x", UFS_EPROTO, r->response);

	// Verify SCSI transfer status
	if (r->status == SCSI_STATUS_BUSY)
		return ufs_err("SCSI_STATUS_BUSY", UFS_EBUSY);

	if (r->status != SCSI_STATUS_GOOD)
		return ufs_err("SCSI error status %#x", UFS_EIO, r->status);

	return 0;
}

static int ufs_scsi_command(UfsCtlr *ufs, UfsCmdReq *req)
{
	int busy_retries = 3; // Busy is not expected, but allow 3 retries
	int rc;

	do {
		rc = ufs_do_scsi_command(ufs, req);

		// Try again for unit attention condition
		if (rc == UFS_EUAC)
			rc = ufs_do_scsi_command(ufs, req);
	} while (rc == UFS_EBUSY && busy_retries-- > 0);

	if (rc)
		return ufs_err("SCSI command %#x failed", rc, req->cdb[0]);

	return 0;
}

// Test Unit Ready command
static int ufs_scsi_unit_rdy(UfsCtlr *ufs, uint32_t lun)
{
	UfsCmdReq req = {
		.lun = lun,
		.cdb = {
			[0] = SCSI_CMD_TEST_UNIT_RDY,
		},
	};

	return ufs_scsi_command(ufs, &req);
}

static int ufs_scsi_tfr(UfsDevice *ufs_dev, uint8_t *buf, uint32_t lba,
			uint32_t blocks, bool read)
{
	UfsCmdReq req = {
		.lun = ufs_dev->lun,
		.expected_len = blocks * ufs_dev->dev.block_size,
		.data_buf_phy = virt_to_phys(buf),
		.cdb = {
			[2] = lba >> 24,
			[3] = lba >> 16,
			[4] = lba >> 8,
			[5] = lba,
			[7] = blocks >> 8,
			[8] = blocks,
		},
	};

	// Maximum size of transfer supported by SCSI READ (10) / WRITE (10).
	// PRDT memory allocation size is also based on this limit.
	// UFS block size is at least 4096 so this limit means a maximum
	// transfer size of 262140 KiB (slightly under 256 MiB).
	if (blocks > 65535)
		return UFS_EINVAL;

	// Note SCSI READ (10) / WRITE (10) support is specified as mandatory
	// whereas SCSI READ(16) / WRITE (16) is optional.
	if (read) {
		req.cdb[0] = SCSI_CMD_READ10;
		req.cdb[1] = 0;
		req.flags  = UFS_XFER_FLAGS_READ;
	} else {
		req.cdb[0] = SCSI_CMD_WRITE10;
		// Use FUA to avoid write cache because we never flush the cache
		req.cdb[1] = SCSI_FLAG_FUA;
		req.flags  = UFS_XFER_FLAGS_WRITE;
	}

	return ufs_scsi_command(ufs_dev->ufs, &req);
}

static lba_t block_ufs_read(BlockDevOps *me, lba_t start, lba_t count,
			    void *buffer)
{
	UfsDevice *ufs_dev = container_of(me, UfsDevice, dev.ops);

	return ufs_scsi_tfr(ufs_dev, buffer, start, count, true) ? 0 : count;
}

static lba_t block_ufs_write(BlockDevOps *me, lba_t start, lba_t count,
			     const void *buffer)
{
	UfsDevice *ufs_dev = container_of(me, UfsDevice, dev.ops);
	uint8_t *buf = (uint8_t *)buffer;

	return ufs_scsi_tfr(ufs_dev, buf, start, count, false) ? 0 : count;
}

static inline bool ufs_fast(uint32_t pwr_mode)
{
	return pwr_mode == UFS_FAST_MODE || pwr_mode == UFS_FASTAUTO_MODE;
}

// Refer UFSHCI spec. JESD223D section 7.4 UIC Power Mode Change
static int ufs_utp_gear_sw(UfsCtlr *ufs)
{
	UfsTfrMode *tfr_mode = &ufs->tfr_mode;
	uint32_t phy_term;
	uint32_t pwr_mode;
	uint32_t upmcrs;
	int rc;

	rc = ufs_hook(ufs, UFS_OP_PRE_GEAR_SWITCH, NULL);
	if (rc)
		return ufs_err("UFS_OP_PRE_GEAR_SWITCH failed", rc);

	// Clear the UPMS bit in IS register
	// Note, UFSHCI_IS is RWC i.e. write 1's to clear bits
	ufs_write32(ufs, UFSHCI_IS, BMSK_UPMS);

	rc = ufs_dme_set(ufs, PA_ACTIVETXDATALANES, tfr_mode->tx.lanes);
	if (rc)
		return rc;

	rc = ufs_dme_set(ufs, PA_ACTIVERXDATALANES, tfr_mode->rx.lanes);
	if (rc)
		return rc;

	// Set the TX and RX gear in L1.5, Set the HOST PWM RX GEAR
	rc = ufs_dme_set(ufs, PA_RXGEAR, tfr_mode->rx.gear);
	if (rc)
		return rc;

	// Set the HOST PWM TX GEAR
	rc = ufs_dme_set(ufs, PA_TXGEAR, tfr_mode->tx.gear);
	if (rc)
		return rc;

	phy_term = ufs_fast(tfr_mode->tx.pwr_mode);
	rc = ufs_dme_set(ufs, PA_TXTERMINATION, phy_term);
	if (rc)
		return rc;

	phy_term = ufs_fast(tfr_mode->rx.pwr_mode);
	rc = ufs_dme_set(ufs, PA_RXTERMINATION, phy_term);
	if (rc)
		return rc;

	// Set the HS Series
	if (ufs_fast(tfr_mode->rx.pwr_mode) || ufs_fast(tfr_mode->tx.pwr_mode)) {
		rc = ufs_dme_set(ufs, PA_HSSERIES, ufs->tfr_mode.hs_series);
		if (rc)
			return rc;
	}

	// Set L2 Timer Data refer UniPro v1.61 Table 102 L2 Time Data Structure
	// and Table 148 Data Link Layer Attributes for Alternative Power Mode Change
	// Note, currently, UFS uses TC0 only. Therefore, setting the following values
	// are not needed:
	//	DME_Local_FC1ProtectionTimeOutVal
	//	DME_Local_TC1ReplayTimeOutVal
	//	DME_Local_ AFC1ReqTimeOutVal
	if ((rc = ufs_dme_set(ufs, PA_PWRMODEUSERDATA(0), DL_FC0PROTECTIONTIMEOUTVAL)) ||
	    (rc = ufs_dme_set(ufs, PA_PWRMODEUSERDATA(1), DL_TC0REPLAYTIMEOUTVAL)) ||
	    (rc = ufs_dme_set(ufs, PA_PWRMODEUSERDATA(2), DL_AFC0REQTIMEOUTVAL)) ||
	    (rc = ufs_dme_set(ufs, PA_PWRMODEUSERDATA(3), DL_FC1PROTECTIONTIMEOUTVAL)) ||
	    (rc = ufs_dme_set(ufs, PA_PWRMODEUSERDATA(4), DL_TC1REPLAYTIMEOUTVAL)) ||
	    (rc = ufs_dme_set(ufs, PA_PWRMODEUSERDATA(5), DL_AFC1REQTIMEOUTVAL)) ||
	    (rc = ufs_dme_set(ufs, DME_LOCALFC0PROTECTIONTIMEOUTVAL, DL_FC0PROTECTIONTIMEOUTVAL)) ||
	    (rc = ufs_dme_set(ufs, DME_LOCALTC0REPLAYTIMEOUTVAL    , DL_TC0REPLAYTIMEOUTVAL)) ||
	    (rc = ufs_dme_set(ufs, DME_LOCALAFC0REQTIMEOUTVAL      , DL_AFC0REQTIMEOUTVAL)))
		return rc;

	// Set the HOST Power Mode
	pwr_mode = tfr_mode->rx.pwr_mode << 4 | tfr_mode->tx.pwr_mode;
	rc = ufs_dme_set(ufs, PA_PWRMODE, pwr_mode);
	if (rc)
		return rc;

	rc = ufs_poll_completion(ufs, BMSK_UPMS, 0, UFS_HCI_UPMS_POLL_TIMEOUT_US);
	if (rc)
		return rc;

	// Check the result of the power mode change request
	upmcrs = (ufs_read32(ufs, UFSHCI_HCS) & BMSK_UPMCRS) >> UPMCRS_SHIFT;
	if (upmcrs != UPMCRS_PWR_LOCAL)
		return ufs_err("Power Mode Change failed, UPMCRS %d", UFS_EIO, upmcrs);

	rc = ufs_hook(ufs, UFS_OP_POST_GEAR_SWITCH, NULL);
	if (rc)
		return ufs_err("UFS_OP_POST_GEAR_SWITCH failed", rc);

	return rc;
}

static int ufs_get_tfr_mode(UfsCtlr *ufs)
{
	UfsTfrMode *tfr_mode = &ufs->tfr_mode;

	if (tfr_mode->initialized)
		return 0;

	ufs_dme_get(ufs, PA_CONNECTEDRXDATALANES, &tfr_mode->rx.lanes);
	ufs_dme_get(ufs, PA_CONNECTEDTXDATALANES, &tfr_mode->tx.lanes);

	if (!tfr_mode->rx.lanes || !tfr_mode->tx.lanes)
		return ufs_err("Failed to get max lanes rx %u, tx %u", UFS_EINVAL,
			       tfr_mode->rx.lanes, tfr_mode->tx.lanes);

	ufs_dme_get(ufs, PA_MAXRXHSGEAR, &tfr_mode->rx.gear);
	ufs_dme_peer_get(ufs, PA_MAXRXHSGEAR, &tfr_mode->tx.gear);

	if (tfr_mode->rx.gear) {
		tfr_mode->rx.pwr_mode = UFS_FAST_MODE;
	} else {
		tfr_mode->rx.pwr_mode = UFS_SLOW_MODE;
		ufs_dme_get(ufs, PA_MAXRXPWMGEAR, &tfr_mode->rx.gear);
	}

	if (tfr_mode->tx.gear) {
		tfr_mode->tx.pwr_mode = UFS_FAST_MODE;
	} else {
		tfr_mode->tx.pwr_mode = UFS_SLOW_MODE;
		ufs_dme_peer_get(ufs, PA_MAXRXPWMGEAR, &tfr_mode->tx.gear);
	}

	if (!tfr_mode->rx.gear || !tfr_mode->tx.gear)
		return ufs_err("Failed to get max gear rx %u, tx %u", UFS_EINVAL,
			       tfr_mode->rx.gear, tfr_mode->tx.gear);

	tfr_mode->hs_series = UFS_HS_SERIES_B;

	tfr_mode->initialized = true;

	return 0;
}

// Set the gear speed to operational speed
static int ufs_set_gear(UfsCtlr *ufs)
{
	int rc;

	rc = ufs_get_tfr_mode(ufs);
	if (rc)
		return rc;

	rc = ufs_utp_gear_sw(ufs);
	if (rc)
		return ufs_err("Gear switch failed", rc);

	return 0;
}

// Read a UFS descriptor
static int ufs_read_descriptor(UfsCtlr *ufs, uint8_t idn, uint8_t idx,
			       uint8_t *buf, uint8_t len, uint8_t *resp_len)
{
	UfsQryReq req = {
		.idn = idn,
		.idx = idx,
		.resp_data_len = len,
		.resp_data_buf = buf,
	};
	int rc;

	memset(buf, 0, len);

	rc = ufs_dev_query_op(ufs, &req, UPIU_QUERY_OP_READ_DESCRIPTOR);
	if (rc)
		return ufs_err("Failed to read descriptor IDN %u", rc, idn);

	// Descriptor byte 1 is always IDN
	if (buf[UFS_DESC_IDN_BYTE] != idn)
		return ufs_err("Invalid response IDN", UFS_EIO);

	*resp_len = req.val;

	return 0;
}

static int ufs_get_descriptor(UfsCtlr *ufs, UfsDesc *desc, uint8_t idn, uint8_t idx)
{
	int rc;

	if (desc->read_already)
		return 0;

	rc = ufs_read_descriptor(ufs, idn, idx, desc->raw, UFS_DESCRIPTOR_MAX_SIZE, &desc->len);
	if (rc)
		return rc;

	desc->read_already = true;

	return 0;
}

static int ufs_populate_device_descriptor(UfsCtlr *ufs)
{
	int rc;

	rc = ufs_get_descriptor(ufs, &ufs->dev_desc, UFS_DESC_IDN_DEVICE, 0);
	if (rc)
		return ufs_err("Failed get device descriptor", rc);
	return 0;
}

static int ufs_populate_unit_descriptor(UfsDevice *ufs_dev)
{
	int lun = ufs_dev->lun;
	int rc;

	rc = ufs_get_descriptor(ufs_dev->ufs, &ufs_dev->unit_desc, UFS_DESC_IDN_UNIT, lun);
	if (rc)
		return ufs_err("Failed get unit descriptor for LUN %u", rc, lun);
	return 0;
}

static int ufs_setup_device(UfsCtlr *ufs, UfsDevice *ufs_dev, int lun)
{
	int rc;

	if (UFS_DEBUG)
		printf("Setting up logical unit %02x\n", lun);

	ufs_dev->ufs = ufs;
	ufs_dev->lun = lun;

	// Get unit descriptor
	rc = ufs_populate_unit_descriptor(ufs_dev);
	if (rc)
		return rc;

	// Check LUN is enabled
	if (!ufs_ud(ufs_dev)->bLUEnable)
		return UFS_ENOLUN;

	// Send TEST UNIT READY
	rc = ufs_scsi_unit_rdy(ufs, lun);
	if (rc)
		return ufs_err("Failed get unit ready", rc);

	return 0;
}

static int ufs_new_device(UfsCtlr *ufs, int lun, UfsDevice **ufs_dev_ptr)
{
	UfsDevice *ufs_dev = xzalloc(sizeof(UfsDevice));
	int rc;

	rc = ufs_setup_device(ufs, ufs_dev, lun);
	if (rc) {
		free(ufs_dev);
		return rc;
	}

	*ufs_dev_ptr = ufs_dev;

	return 0;
}

static int ufs_utp_init(UfsCtlr *ufs)
{
	uint64_t phys_addr;
	int i, rc;

	// Reset the controller by setting HCE register to 0
	ufs_write32(ufs, UFSHCI_HCE, 0);
	rc = ufs_wait(ufs, UFSHCI_HCE, BMSK_HCE, 0, HCI_DISABLE_TIMEOUT_US);
	if (rc)
		return ufs_err("HCE failed to clear", rc);

	rc = ufs_hook(ufs, UFS_OP_PRE_HCE, NULL);
	if (rc)
		return ufs_err("UFS_OP_PRE_HCE failed", rc);

	// Enable the host controller
	ufs_write32(ufs, UFSHCI_HCE, BMSK_HCE);
	// For some host controllers, if HCE is read back immediately, it reads as 1
	// even though the controller is not yet enabled, so the transition is then
	// 1 -> 0 -> 1. To avoid seeing 1 prematurely, a delay is needed,
	// The maximum delay used in the kernel is 1000us.
	mdelay(1);
	rc = ufs_wait(ufs, UFSHCI_HCE, BMSK_HCE, BMSK_HCE, HCI_ENABLE_TIMEOUT_US);
	if (rc)
		return ufs_err("HCE failed to set", rc);

	rc = ufs_hook(ufs, UFS_OP_PRE_LINK_STARTUP, NULL);
	if (rc)
		return ufs_err("UFS_OP_PRE_LINK_STARTUP failed", rc);

	// Retry number of times for link startup
	for (i = 0; i < UFS_LINK_STARTUP_RETRY; i++) {
		// Send DME_LINKSTARTUP command
		rc = ufs_utp_uic_getset(ufs, UICCMDR_DME_LINKSTARTUP, 0, 0, NULL);
		if (rc) {
			ufs_err("DME_LINKSTARTUP attempt %i failed", rc, i + 1);
			continue;
		}
		// Check to make sure the device is detected, wait until it is
		rc = ufs_wait(ufs, UFSHCI_HCS, BMSK_DP, BMSK_DP, HCI_LINK_STARTUP_TIMEOUT_US);
		if (!rc)
			break;
		// As per spec, wait for ULSS, but retry anyway if it does not set
		ufs_wait(ufs, UFSHCI_IS, BMSK_ULSS, BMSK_ULSS, HCI_LINK_STARTUP_TIMEOUT_US);
		// UFSHCI_IS is RWC, write 1 to clear ULSS
		ufs_write32(ufs, UFSHCI_IS, BMSK_ULSS);
		ufs_err("No device found after %i attempts", rc, i + 1);
	}
	if (rc)
		return ufs_err("Link startup failed or no device", rc);

	rc = ufs_hook(ufs, UFS_OP_POST_LINK_STARTUP, NULL);
	if (rc)
		return ufs_err("UFS_OP_POST_LINK_STARTUP failed", rc);

	if (!ufs->ufs_req_list)
		ufs->ufs_req_list = dma_memalign(UFS_DMA_ALIGN, UFS_MEM_SZ);

	memset(ufs->ufs_req_list, 0, UFS_MEM_SZ);

	// Program the physical address into the register
	phys_addr = virt_to_phys(ufs->ufs_req_list);
	ufs_write32(ufs, UFSHCI_UTRLBA, phys_addr);
	ufs_write32(ufs, UFSHCI_UTRLBAU, phys_addr >> 32);

	// Enable the UTP transfer request list
	ufs_write32(ufs, UFSHCI_UTRLRSR, BMSK_RSR);

	return 0;
}

static int ufs_ctrlr_setup(UfsCtlr *ufs)
{
	int rc;

	if (ufs->ctlr_initialized)
		return 0;

	// Force re-read of tfr_mode and device descriptor when retrying
	ufs->tfr_mode.initialized = false;
	ufs->dev_desc.read_already = false;

	rc = ufs_utp_init(ufs);
	if (rc)
		return rc;

	rc = ufs_wait_device_awake(ufs);
	if (rc)
		return rc;

	rc = ufs_set_fDeviceInit(ufs);
	if (rc)
		return rc;

	rc = ufs_populate_device_descriptor(ufs);
	if (rc)
		return rc;

	// Only change gear if there are logical units
	if (ufs_dd(ufs)->bNumberLU) {
		rc = ufs_set_gear(ufs);
		if (rc)
			return rc;
	}

	ufs->ctlr_initialized = true;

	return 0;
}

static int ufs_ctrlr_setup_retry(UfsCtlr *ufs)
{
	int i, rc = -1;

	for (i = 0; i < UFS_INIT_RETRY && rc; i++) {
		if (i)
			ufs_err("Retrying", rc);
		rc = ufs_ctrlr_setup(ufs);
	}
	return rc;
}

static int ufs_add_device(UfsCtlr *ufs, uint32_t lun)
{
	UfsDevice *ufs_dev;
	char name[20];
	int rc;

	rc = ufs_new_device(ufs, lun, &ufs_dev);
	if (rc)
		return rc;

	snprintf(name, sizeof(name), "UFS LUN %u", lun);

	ufs_dev->dev.name		= strdup(name);
	ufs_dev->dev.removable		= 0;
	ufs_dev->dev.block_size		= 1 << ufs_ud(ufs_dev)->bLogicalBlockSize;
	ufs_dev->dev.block_count	= be64toh(ufs_ud(ufs_dev)->qLogicalBlockCount);
	ufs_dev->dev.stream_block_count	= ufs_dev->dev.block_count;
	ufs_dev->dev.ops.read		= &block_ufs_read;
	ufs_dev->dev.ops.write		= &block_ufs_write;
	ufs_dev->dev.ops.new_stream	= &new_simple_stream;
	printf("Adding UFS block device LUN %02x block size %u block count %llu\n",
		lun, ufs_dev->dev.block_size, (unsigned long long)ufs_dev->dev.block_count);
	list_insert_after(&ufs_dev->dev.list_node, &fixed_block_devices);

	ufs->ufs_dev[lun] = ufs_dev;

	return 0;
}

int ufs_update(BlockDevCtrlrOps *bdev_ops)
{
	UfsCtlr *ufs = container_of(bdev_ops, UfsCtlr, bctlr.ops);
	int lun, cnt;
	int rc;

	rc = ufs_ctrlr_setup_retry(ufs);
	if (rc)
		return rc;

	for (lun = 0, cnt = 0; lun < MAX_LUN && cnt < ufs_dd(ufs)->bNumberLU; lun++) {
		if (!ufs->ufs_dev[lun]) {
			rc = ufs_add_device(ufs, lun);
			if (rc == UFS_ENOLUN)
				continue;
			if (rc)
				 return ufs_err("Failed to set up LUN %d", rc, lun);
		}
		cnt += 1;
	}

	ufs->bctlr.need_update = 0;

	return 0;
}
