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

/* Documentation:
 * This driver implements a minimal subset of the NVMe 1.0e specification.
 * (nvmexpress.org) It is designed to balance simplicity and performance.
 * Therefore it operates by polling the NVMe Completion Queue (CQ) for phase
 * changes rather than utilizing interrupts. The initialization functions are
 * processed one at a time, therefore the Admin Queue pair only supports depth
 * 2.
 * This driver is limited to a single IO queue pair (in addition to the
 * mandatory Admin queue pair). The IO queue depth is configurable, but has
 * shallow defaults to minimize host memory consumption. This driver only
 * supports a maximum of one PRP List, limiting the maximum transfer size to
 * 2MB (assuming 4KB memory pages).
 *
 * Operation:
 * At initialization this driver allocates a pool of host memory and overlays
 * the queue pair structures. It also statically allocates a block of memory
 * for a PRP List, avoiding the need to allocate/free memory at IO time.
 * Each identified NVMe namespace has a corresponding depthcharge BlockDev
 * structure, effectively creating a new "drive" visible to higher levels.
 *
 * The depthcharge read/write callbacks split host requests into chunks
 * satisfying the NVMe device's maximum transfer size limitations. Then they
 * call the corresponding _internal_ functions to facilitate formatting of the
 * NVMe structures in host memory. After all of the commands have been created
 * in host memory the Submission Queue tail pointer is updated allowing the
 * drive to process the newly submitted commands. Queuing commands allows the
 * drive to internally optimize accesses, increasing performance. Finally, the
 * Completion Queue phase bit is polled until it inverts, indicating that the
 * command has completed. If the SQ is full, outstanding commands will be
 * completed before the _internal_ function proceeds. This situation reduces
 * effective performance and should be avoided by increasing SQ/CQ depth.
 */

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>
#include <stdio.h>

#include "base/cleanup_funcs.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/info.h"
#include "drivers/storage/nvme.h"

/* Read 64bits from register space */
static uint64_t readll(uintptr_t _a)
{
	uint64_t _v;

	_v = readl(_a);
	_v |= ((uint64_t)readl(_a + sizeof(uint32_t)) << 32);
	return le64toh(_v);
}

/* Write 64bits to register space */
static void writell(uint64_t _v, volatile const uintptr_t _a)
{
	_v = htole64(_v);
	writel(_v & 0xffffffff, _a);
	writel(_v >> 32, _a + sizeof(uint32_t));
}

DEBUG(
static void nvme_dump_status(NVME_CQ volatile *cq) {
	printf("Dump NVMe Completion Entry Status from [%p]:\n", cq);

	printf("  SQ ID : [0x%x], Phase Tag : [%d], Cmd ID : [0x%x] Flags : [0x%x]\n",
			cq->sqid, cq->flags & NVME_CQ_FLAGS_PHASE, cq->cid, cq->flags);

	if (NVME_CQ_FLAGS_SCT(cq->flags) == 0) {
		if (NVME_CQ_FLAGS_SC(cq->flags) == 0)
			printf("  NVMe Cmd Execution Result - Successful\n");
		else
			printf("  NVMe Cmd Execution Result - error sc=%u\n",NVME_CQ_FLAGS_SC(cq->flags));
	} else
		printf("   NVMe Cmd Execution Result - error sct=%u\n",NVME_CQ_FLAGS_SCT(cq->flags));
}
) //DEBUG

/* Disables and resets the NVMe controller */
static NVME_STATUS nvme_disable_controller(NvmeCtrlr *ctrlr) {
	NVME_CC cc;
	uint32_t timeout;

	/* Read controller configuration */
	cc = readl(ctrlr->ctrlr_regs + NVME_CC_OFFSET);
	CLR(cc, NVME_CC_EN);
	/* Write controller configuration */
	writel_with_flush(cc, ctrlr->ctrlr_regs + NVME_CC_OFFSET);
	/* Delay up to CAP.TO ms for CSTS.RDY to clear*/
	if (NVME_CAP_TO(ctrlr->cap) == 0)
		timeout = 1;
	else
		timeout = NVME_CAP_TO(ctrlr->cap);

	if (WAIT_WHILE(
		((readl(ctrlr->ctrlr_regs + NVME_CSTS_OFFSET) & NVME_CSTS_RDY) == 1),
		timeout)) {
		return NVME_TIMEOUT;
	}

	return NVME_SUCCESS;
}

/* Enables controller and verifies that it's ready */
static NVME_STATUS nvme_enable_controller(NvmeCtrlr *ctrlr) {
	NVME_CC cc = 0;
	uint32_t timeout;

	SET(cc, NVME_CC_EN);
	cc |= NVME_CC_IOSQES(6); /* Spec. recommended values */
	cc |= NVME_CC_IOCQES(4); /* Spec. recommended values */
	/* Write controller configuration. */
	writel_with_flush(cc, ctrlr->ctrlr_regs + NVME_CC_OFFSET);

	/* Delay up to CAP.TO ms for CSTS.RDY to set*/
	if (NVME_CAP_TO(ctrlr->cap) == 0)
		timeout = 1;
	else
		timeout = NVME_CAP_TO(ctrlr->cap);

	if (WAIT_WHILE(
		((readl(ctrlr->ctrlr_regs + NVME_CSTS_OFFSET) & NVME_CSTS_RDY) == 0),
		timeout)) {
		return NVME_TIMEOUT;
	}

	return NVME_SUCCESS;
}

/* Shutdown controller before power cycle */
static NVME_STATUS nvme_shutdown_controller(NvmeCtrlr *ctrlr) {
	NVME_CC cc;
	uint32_t timeout;

	/* Read controller configuration */
	cc = readl(ctrlr->ctrlr_regs + NVME_CC_OFFSET);

	/* Indicate normal shutdown */
	CLR(cc, NVME_CC_SHN_MASK);
	SET(cc, NVME_CC_SHN_NORMAL);

	/* Write controller configuration */
	writel_with_flush(cc, ctrlr->ctrlr_regs + NVME_CC_OFFSET);

	/* Delay up to CAP.TO ms for CSTS.SHST to indicate complete */
	if (NVME_CAP_TO(ctrlr->cap) == 0)
		timeout = 1;
	else
		timeout = NVME_CAP_TO(ctrlr->cap);

	if (WAIT_WHILE(
		((readl(ctrlr->ctrlr_regs + NVME_CSTS_OFFSET) &
		  NVME_CSTS_SHST_MASK) != NVME_CSTS_SHST_COMPLETE),
		timeout)) {

		/* Send abrupt shutdown notification */
		cc = readl(ctrlr->ctrlr_regs + NVME_CC_OFFSET);
		CLR(cc, NVME_CC_SHN_MASK);
		SET(cc, NVME_CC_SHN_ABRUPT);
		writel_with_flush(cc, ctrlr->ctrlr_regs + NVME_CC_OFFSET);

		printf("NVMe: shutdown timeout, sent abrupt notification\n");
		return NVME_TIMEOUT;
	}

	printf("NVMe: shutdown succeeded\n");
	return NVME_SUCCESS;
}

/* Add command to host SQ, don't write to HW SQ yet
 *
 * ctrlr: NVMe controller handle
 * qid: Queue Identifier for the SQ/CQ containing the new command
 * sqsize: Size of the submission queue
 */
static NVME_STATUS nvme_submit_cmd(NvmeCtrlr *ctrlr, uint16_t qid, uint32_t sqsize) {
	if (NULL == ctrlr)
		return NVME_INVALID_PARAMETER;
	if (qid > NVME_NUM_IO_QUEUES)
		return NVME_INVALID_PARAMETER;

	/* Update the submission queue tail in host memory */
	if (++(ctrlr->sq_t_dbl[qid]) > (sqsize-1))
		ctrlr->sq_t_dbl[qid] = 0;

	return NVME_SUCCESS;
}

/* Ring SQ doorbell register, submitting all outstanding command to HW
 *
 * ctrlr: NVMe controller handle
 * qid: Queue Identifier for the SQ/CQ containing the new command
 */
static NVME_STATUS nvme_ring_sq_doorbell(NvmeCtrlr *ctrlr, uint16_t qid) {
	if (NULL == ctrlr)
		return NVME_INVALID_PARAMETER;
	if (qid > NVME_NUM_IO_QUEUES)
		return NVME_INVALID_PARAMETER;

	/* Ring SQ doorbell by writing SQ tail index to controller */
	writel_with_flush(ctrlr->sq_t_dbl[qid],
				ctrlr->ctrlr_regs +
				NVME_SQTDBL_OFFSET(qid, NVME_CAP_DSTRD(ctrlr->cap)));

	return NVME_SUCCESS;
}

/* Poll for completion of all commands from HW
 *
 * ctrlr: NVMe controller handle
 * qid: Queue Identifier for the SQ/CQ containing the new command
 * cqsize: Size of the completion queue
 * timeout_ms: How long in milliseconds to wait for command completion
 */
static NVME_STATUS nvme_complete_cmds_polled(NvmeCtrlr *ctrlr,
			uint16_t qid,
			uint32_t cqsize,
			uint32_t timeout_ms) {
	NVME_CQ *cq;
	uint32_t ncmds;

	if (NULL == ctrlr)
		return NVME_INVALID_PARAMETER;
	if (qid > NVME_NUM_IO_QUEUES)
		return NVME_INVALID_PARAMETER;
	if (timeout_ms == 0)
		timeout_ms = 1;

	/* We will complete all outstanding commands */
	if (ctrlr->cq_h_dbl[qid] < ctrlr->sq_t_dbl[qid])
		ncmds = ctrlr->sq_t_dbl[qid] - ctrlr->cq_h_dbl[qid];
	else
		ncmds = (cqsize - ctrlr->cq_h_dbl[qid]) + ctrlr->sq_t_dbl[qid];
	DEBUG(printf("nvme_complete_cmds_polled: completing %u commands\n",(unsigned)ncmds);)

	while (ncmds--) {
		cq  = ctrlr->cq_buffer[qid] + ctrlr->cq_h_dbl[qid];
		/* Wait for phase to change (or timeout) */
		if (WAIT_WHILE(
			((readw(&(cq->flags)) & NVME_CQ_FLAGS_PHASE) == ctrlr->pt[qid]),
			timeout_ms)) {
				printf("nvme_complete_cmds_polled: ERROR - timeout\n");
				return NVME_TIMEOUT;
		}

		/* Dump completion entry status for debugging. */
		DEBUG(nvme_dump_status(cq);)

		/* Update the doorbell, queue phase, and queue command id if necessary */
		if (++(ctrlr->cq_h_dbl[qid]) > (cqsize-1)) {
			ctrlr->cq_h_dbl[qid] = 0;
			ctrlr->pt[qid] ^= 1;
		}
		/* Update SQ head pointer */
		ctrlr->sqhd[qid] = cq->sqhd;
	}

	/* Ring the completion queue doorbell register*/
	writel_with_flush(ctrlr->cq_h_dbl[qid], ctrlr->ctrlr_regs + NVME_CQHDBL_OFFSET(qid, NVME_CAP_DSTRD(ctrlr->cap)));

	/* If the SQ is empty, reset cid to zero */
	if (ctrlr->sq_t_dbl[qid] == ctrlr->sqhd[qid])
		ctrlr->cid[qid] = 0;

	return NVME_SUCCESS;
}

/* Submit and complete 1 command by polling CQ for phase change
 * Rings SQ doorbell, polls waiting for completion, rings CQ doorbell
 *
 * ctrlr: NVMe controller handle
 * qid: Queue Identifier for the SQ/CQ containing the new command
 * sqsize: Number of commands (size) of the submission queue
 * cqsize: Number of commands (size) of the completion queue
 * timeout_ms: How long in milliseconds to wait for command completion
 */
static NVME_STATUS nvme_do_one_cmd_synchronous(NvmeCtrlr *ctrlr,
			uint16_t qid,
			uint32_t sqsize,
			uint32_t cqsize,
			uint32_t timeout_ms) {
	NVME_STATUS status = NVME_SUCCESS;

	if (NULL == ctrlr)
		return NVME_INVALID_PARAMETER;
	if (qid > NVME_NUM_IO_QUEUES)
		return NVME_INVALID_PARAMETER;
	if (timeout_ms == 0)
		timeout_ms = 1;

	/* This function should only be called when no commands are pending
	 * because it will complete all outstanding commands. */
	if (ctrlr->sq_t_dbl[qid] != ctrlr->sqhd[qid])
		printf("nvme_do_one_cmd_synchronous: warning, SQ not empty. All commands will be completed.\n");

	status = nvme_submit_cmd(ctrlr, qid, sqsize);
	if (NVME_ERROR(status)) {
		DEBUG(printf("nvme_do_one_cmd_synchronous: error %d submitting command\n",status);)
		return status;
	}

	status = nvme_ring_sq_doorbell(ctrlr, qid);
	if (NVME_ERROR(status)) {
		DEBUG(printf("nvme_do_one_cmd_synchronous: error %d ringing doorbell\n",status);)
		return status;
	}

	status = nvme_complete_cmds_polled(ctrlr, qid, cqsize, NVME_GENERIC_TIMEOUT);
	if (NVME_ERROR(status)) {
		DEBUG(printf("nvme_do_one_cmd_synchronous: error %d completing command\n",status);)
		return status;
	}

	return NVME_SUCCESS;
}

/* Sends Set Feature 07h to allocate count number of IO queues */
static NVME_STATUS nvme_set_queue_count(NvmeCtrlr *ctrlr, uint16_t count) {
	NVME_SQ *sq;
	int status = NVME_SUCCESS;

	if (count == 0)
		return NVME_INVALID_PARAMETER;

	sq  = ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX] + ctrlr->sq_t_dbl[NVME_ADMIN_QUEUE_INDEX];

	memset(sq, 0, sizeof(NVME_SQ));

	sq->opc = NVME_ADMIN_SETFEATURES_OPC;
	sq->cid = ctrlr->cid[NVME_ADMIN_QUEUE_INDEX]++;

	sq->cdw10 = NVME_ADMIN_SETFEATURES_NUMQUEUES;

	/* Count is a 0's based value, so subtract one */
	count--;
	/* Set count number of IO SQs and CQs */
	sq->cdw11 = count;
	sq->cdw11 |= (count << 16);

	status = nvme_do_one_cmd_synchronous(ctrlr,
				NVME_ADMIN_QUEUE_INDEX,
				NVME_ASQ_SIZE,
				NVME_ACQ_SIZE,
				NVME_GENERIC_TIMEOUT);

	return status;
}

/* Creates a single IO completion queue */
static NVME_STATUS nvme_create_cq(NvmeCtrlr *ctrlr, uint16_t qid, uint16_t qsize) {
	NVME_SQ *sq;
	int status = NVME_SUCCESS;

	sq  = ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX] + ctrlr->sq_t_dbl[NVME_ADMIN_QUEUE_INDEX];

	memset(sq, 0, sizeof(NVME_SQ));

	sq->opc = NVME_ADMIN_CRIOCQ_OPC;
	sq->cid = ctrlr->cid[NVME_ADMIN_QUEUE_INDEX]++;

	/* Only physically contiguous address supported */
	sq->prp[0] = (uintptr_t)virt_to_phys(ctrlr->cq_buffer[qid]);
	/* Set physically contiguous (PC) bit */
	sq->cdw11 = 1;

	sq->cdw10 |= NVME_ADMIN_CRIOCQ_QID(qid);
	sq->cdw10 |= NVME_ADMIN_CRIOCQ_QSIZE(qsize);

	status = nvme_do_one_cmd_synchronous(ctrlr,
				NVME_ADMIN_QUEUE_INDEX,
				NVME_ASQ_SIZE,
				NVME_ACQ_SIZE,
				NVME_GENERIC_TIMEOUT);

	return status;
}

/* Creates a single IO submission queue
 * NOTE: Assumes that completion queue ID == submission queue ID
 */
static NVME_STATUS nvme_create_sq(NvmeCtrlr *ctrlr, uint16_t qid, uint16_t qsize) {
	NVME_SQ *sq;
	int status = NVME_SUCCESS;

	sq  = ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX] + ctrlr->sq_t_dbl[NVME_ADMIN_QUEUE_INDEX];

	memset(sq, 0, sizeof(NVME_SQ));

	sq->opc = NVME_ADMIN_CRIOSQ_OPC;
	sq->cid = ctrlr->cid[NVME_ADMIN_QUEUE_INDEX]++;

	/* Only physically contiguous address supported */
	sq->prp[0] = (uintptr_t)virt_to_phys(ctrlr->sq_buffer[qid]);
	/* Set physically contiguous (PC) bit */
	sq->cdw11 = 1;
	sq->cdw11 |= NVME_ADMIN_CRIOSQ_CQID(qid);

	sq->cdw10 |= NVME_ADMIN_CRIOSQ_QID(qid);
	sq->cdw10 |= NVME_ADMIN_CRIOSQ_QSIZE(qsize);

	status = nvme_do_one_cmd_synchronous(ctrlr,
				NVME_ADMIN_QUEUE_INDEX,
				NVME_ASQ_SIZE,
				NVME_ACQ_SIZE,
				NVME_GENERIC_TIMEOUT);

	return status;
}

/* Generate PRPs for a single virtual memory buffer
 * prp_list: pre-allocated prp list buffer
 * prp: pointer to SQ PRP array
 * buffer: host buffer for request
 * size: number of bytes in request
 */
static NVME_STATUS nvme_fill_prp(PrpList *prp_list, uint64_t *prp, void *buffer, uint64_t size)
{
	uint64_t offset = (uintptr_t)buffer & (NVME_PAGE_SIZE - 1);
	uint64_t xfer_pages;
	uintptr_t buffer_phys = virt_to_phys(buffer);

	/* PRP0 is always the (potentially unaligned) start of the buffer */
	prp[0] = buffer_phys;
	/* Increment buffer to the next aligned page */
	if (ALIGN(buffer_phys,NVME_PAGE_SIZE) == buffer_phys)
		buffer_phys += NVME_PAGE_SIZE;
	else
		buffer_phys = ALIGN_UP(buffer_phys,NVME_PAGE_SIZE);

	/* Case 1: all data will fit in 2 PRP entries (accounting for buffer offset) */
	if ((size + offset) <= (2 * NVME_PAGE_SIZE)) {
		prp[1] = buffer_phys;
		return NVME_SUCCESS;
	}

	/* Case 2: Need to build up to one PRP List */
	xfer_pages = (ALIGN((size + offset), NVME_PAGE_SIZE) >> NVME_PAGE_SHIFT);
	/* Don't count first prp entry as it is the beginning of buffer */
	xfer_pages--;
	/* Make sure this transfer fits into one PRP list */
	if (xfer_pages > (NVME_MAX_XFER_BYTES/NVME_PAGE_SIZE))
		return NVME_INVALID_PARAMETER;

	/* Fill the PRP List */
	prp[1] = (uintptr_t)virt_to_phys(prp_list);
	for (uint32_t entry_index = 0; entry_index < xfer_pages; entry_index++) {
		prp_list->prp_entry[entry_index] = buffer_phys;
		buffer_phys += NVME_PAGE_SIZE;
	}
	return NVME_SUCCESS;
}

/* Sets up read operation for up to max_transfer blocks */
static NVME_STATUS nvme_internal_read(NvmeDrive *drive, void *buffer, lba_t start, lba_t count)
{
	NvmeCtrlr *ctrlr = drive->ctrlr;
	NVME_SQ *sq;
	int status = NVME_SUCCESS;

	if (count == 0)
		return NVME_INVALID_PARAMETER;

	/* If queue is full, need to complete inflight commands before submitting more */
	if ((ctrlr->sq_t_dbl[NVME_IO_QUEUE_INDEX] + 1) % ctrlr->iosq_sz == ctrlr->sqhd[NVME_IO_QUEUE_INDEX]) {
		DEBUG(printf("nvme_internal_read: Too many outstanding commands. Completing in-flights\n");)
		/* Submit commands to controller */
		nvme_ring_sq_doorbell(ctrlr, NVME_IO_QUEUE_INDEX);
		/* Complete submitted command(s) */
		status = nvme_complete_cmds_polled(ctrlr,
				NVME_IO_QUEUE_INDEX,
				NVME_CCQ_SIZE,
				NVME_GENERIC_TIMEOUT);
		if (NVME_ERROR(status)) {
			printf("nvme_internal_read: error %d completing outstanding commands\n",status);
			return status;
		}
	}

	sq  = ctrlr->sq_buffer[NVME_IO_QUEUE_INDEX] + ctrlr->sq_t_dbl[NVME_IO_QUEUE_INDEX];

	memset(sq, 0, sizeof(NVME_SQ));

	sq->opc = NVME_IO_READ_OPC;
	sq->cid = ctrlr->cid[NVME_IO_QUEUE_INDEX]++;
	sq->nsid = drive->namespace_id;

	status = nvme_fill_prp(ctrlr->prp_list[sq->cid], sq->prp, buffer, count * drive->dev.block_size);
	if (NVME_ERROR(status)) {
		printf("nvme_internal_read: error %d generating PRP(s)\n",status);
		return status;
	}

	sq->cdw10 = start;
	sq->cdw11 = (start >> 32);
	sq->cdw12 = (count - 1) & 0xFFFF;

	status = nvme_submit_cmd(ctrlr, NVME_IO_QUEUE_INDEX, ctrlr->iosq_sz);

	return status;
}

/* Read operation entrypoint
 * Cut operation into max_transfer chunks and do it
 */
static lba_t nvme_read(BlockDevOps *me, lba_t start, lba_t count, void *buffer)
{
	NvmeDrive *drive = container_of(me, NvmeDrive, dev.ops);
	NvmeCtrlr *ctrlr = drive->ctrlr;
	uint64_t max_transfer_blocks = 0;
	uint32_t block_size = drive->dev.block_size;
	lba_t orig_count = count;
	int status = NVME_SUCCESS;

	DEBUG(printf("nvme_read: Reading from namespace %d\n",drive->namespace_id);)

	if (ctrlr->controller_data->mdts != 0)
		max_transfer_blocks = ((1 << (ctrlr->controller_data->mdts)) * (1 << NVME_CAP_MPSMIN(ctrlr->cap))) / block_size;
	/* Artificially limit max_transfer_blocks to 1 PRP List */
	if ((max_transfer_blocks == 0) ||
	    (max_transfer_blocks > NVME_MAX_XFER_BYTES / block_size))
		max_transfer_blocks = NVME_MAX_XFER_BYTES / block_size;

	while (count > 0) {
		if (count > max_transfer_blocks) {
			DEBUG(printf("nvme_read: partial read of %llu blocks\n",(unsigned long long)max_transfer_blocks);)
			status = nvme_internal_read(drive, buffer, start, max_transfer_blocks);
			count -= max_transfer_blocks;
			buffer += max_transfer_blocks*block_size;
			start += max_transfer_blocks;
		} else {
			DEBUG(printf("nvme_read: final read of %llu blocks\n",(unsigned long long)count);)
			status = nvme_internal_read(drive, buffer, start, count);
			count = 0;
		}
		if (NVME_ERROR(status))
			break;
	}

	if (NVME_ERROR(status)) {
		printf("nvme_read: error %d\n",status);
		return -1;
	}

	/* Submit commands to controller */
	nvme_ring_sq_doorbell(ctrlr, NVME_IO_QUEUE_INDEX);
	/* Complete submitted command(s) */
	nvme_complete_cmds_polled(ctrlr,
			NVME_IO_QUEUE_INDEX,
			NVME_CCQ_SIZE,
			NVME_GENERIC_TIMEOUT);

	DEBUG(printf("nvme_read: lba = 0x%08x, Original = 0x%08x, Remaining = 0x%08x, BlockSize = 0x%x Status = %d\n", (uint32_t)start, (uint32_t)orig_count, (uint32_t)count, block_size, status);)

	return orig_count - count;
}

/* Sets up write operation for up to max_transfer blocks */
static NVME_STATUS nvme_internal_write(NvmeDrive *drive, void *buffer, lba_t start, lba_t count)
{
	NvmeCtrlr *ctrlr = drive->ctrlr;
	NVME_SQ *sq;
	int status = NVME_SUCCESS;

	if (count == 0)
		return NVME_INVALID_PARAMETER;

	/* If queue is full, need to complete inflight commands before submitting more */
	if ((ctrlr->sq_t_dbl[NVME_IO_QUEUE_INDEX] + 1) % ctrlr->iosq_sz == ctrlr->sqhd[NVME_IO_QUEUE_INDEX]) {
		DEBUG(printf("nvme_internal_write: Too many outstanding commands. Completing in-flights\n");)
		/* Submit commands to controller */
		nvme_ring_sq_doorbell(ctrlr, NVME_IO_QUEUE_INDEX);
		/* Complete submitted command(s) */
		status = nvme_complete_cmds_polled(ctrlr,
				NVME_IO_QUEUE_INDEX,
				NVME_CCQ_SIZE,
				NVME_GENERIC_TIMEOUT);
		if (NVME_ERROR(status)) {
			printf("nvme_internal_read: error %d completing outstanding commands\n",status);
			return status;
		}
	}

	sq  = ctrlr->sq_buffer[NVME_IO_QUEUE_INDEX] + ctrlr->sq_t_dbl[NVME_IO_QUEUE_INDEX];

	memset(sq, 0, sizeof(NVME_SQ));

	sq->opc = NVME_IO_WRITE_OPC;
	sq->cid = ctrlr->cid[NVME_IO_QUEUE_INDEX]++;
	sq->nsid = drive->namespace_id;

	status = nvme_fill_prp(ctrlr->prp_list[sq->cid], sq->prp, buffer, count * drive->dev.block_size);
	if (NVME_ERROR(status)) {
		printf("nvme_internal_write: error %d generating PRP(s)\n",status);
		return status;
	}

	sq->cdw10 = start;
	sq->cdw11 = (start >> 32);
	sq->cdw12 = (count - 1) & 0xFFFF;

	status = nvme_submit_cmd(ctrlr, NVME_IO_QUEUE_INDEX, ctrlr->iosq_sz);

	return status;
}

/* Write operation entrypoint
 * Cut operation into max_transfer chunks and do it
 */
static lba_t nvme_write(BlockDevOps *me, lba_t start, lba_t count,
						const void *buffer)
{
	NvmeDrive *drive = container_of(me, NvmeDrive, dev.ops);
	NvmeCtrlr *ctrlr = drive->ctrlr;
	uint64_t max_transfer_blocks = 0;
	uint32_t block_size = drive->dev.block_size;
	lba_t orig_count = count;
	int status = NVME_SUCCESS;

	DEBUG(printf("nvme_write: Writing to namespace %d\n",drive->namespace_id);)

	if (ctrlr->controller_data->mdts != 0)
		max_transfer_blocks = ((1 << (ctrlr->controller_data->mdts)) * (1 << NVME_CAP_MPSMIN(ctrlr->cap))) / block_size;
	/* Artificially limit max_transfer_blocks to 1 PRP List */
	if ((max_transfer_blocks == 0) ||
	    (max_transfer_blocks > NVME_MAX_XFER_BYTES / block_size))
		max_transfer_blocks = NVME_MAX_XFER_BYTES / block_size;

	while (count > 0) {
		if (count > max_transfer_blocks) {
			DEBUG(printf("nvme_write: partial write of %llu blocks\n",(unsigned long long)max_transfer_blocks);)
			status = nvme_internal_write(drive, (void *)buffer, start, max_transfer_blocks);
			count -= max_transfer_blocks;
			buffer += max_transfer_blocks*block_size;
			start += max_transfer_blocks;
		} else {
			DEBUG(printf("nvme_write final write of %llu blocks\n",(unsigned long long)count);)
			status = nvme_internal_write(drive, (void *)buffer, start, count);
			count = 0;
		}
		if (NVME_ERROR(status))
			break;
	}

	if (NVME_ERROR(status)) {
		printf("nvme_write: error %d\n",status);
		return -1;
	}

	/* Submit commands to controller */
	nvme_ring_sq_doorbell(ctrlr, NVME_IO_QUEUE_INDEX);
	/* Complete submitted command(s) */
	nvme_complete_cmds_polled(ctrlr,
			NVME_IO_QUEUE_INDEX,
			NVME_CCQ_SIZE,
			NVME_GENERIC_TIMEOUT);

	DEBUG(printf("nvme_write: lba = 0x%08x, Original = 0x%08x, Remaining = 0x%08x, BlockSize = 0x%x Status = %d\n", (uint32_t)start, (uint32_t)orig_count, (uint32_t)count, block_size, status);)

	return orig_count - count;
}

static NVME_STATUS nvme_read_log_page(NvmeDrive *drive, int log_page_id,
				      void *data, size_t size)
{
	NVME_SQ *sq;
	NvmeCtrlr *ctrlr = drive->ctrlr;

	if (data == NULL)
		return NVME_INVALID_PARAMETER;

	// Read log page command only support read Dwords, and it is 0-based
	// value, that is we have to read at least 1 Dword.
	if ((size == 0) || (size % sizeof(uint32_t) != 0))
		return NVME_INVALID_PARAMETER;

	sq = ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX] +
	     ctrlr->sq_t_dbl[NVME_ADMIN_QUEUE_INDEX];

	memset(sq, 0, sizeof(*sq));

	sq->opc = NVME_ADMIN_GET_LOG_PAGE;

	sq->cid = ctrlr->cid[NVME_ADMIN_QUEUE_INDEX]++;
	sq->nsid = NVME_NSID_ALL; // NSID_ALL for controller.

	// Number of Dwords is 0-based.
	uint32_t dword_len = size / sizeof(uint32_t) - 1;

	// 31:16  Number of Dwords Lower (NUMDL)
	// 07:00  Log Page Identifier (LID)
	uint32_t dword_len_l = dword_len & (BIT(16) - 1);
	sq->cdw10 = (dword_len_l << 16) | log_page_id;

	// 15:00 Number of Dwords Upper (NUMDU)
	uint32_t dword_len_u = dword_len >> 16;
	sq->cdw11 = dword_len_u;

	NVME_STATUS status =
		nvme_fill_prp(ctrlr->prp_list[sq->cid], sq->prp, data, size);
	if (NVME_ERROR(status)) {
		printf("%s: error %d generating PRP(s)\n", __func__, status);
		return status;
	}

	status = nvme_do_one_cmd_synchronous(ctrlr, NVME_ADMIN_QUEUE_INDEX,
					     NVME_ASQ_SIZE, NVME_ACQ_SIZE,
					     NVME_GENERIC_TIMEOUT);

	if (NVME_ERROR(status)) {
		printf("%s: error %d of nvme_do_one_cmd_synchronous\n",
		       __func__, status);
		return status;
	}

	return status;
}

/* Sends the Identify command, saves result in ctrlr->controller_data*/
static NVME_STATUS nvme_identify(NvmeCtrlr *ctrlr) {
	NVME_SQ *sq;
	int status = NVME_SUCCESS;

	ctrlr->controller_data = dma_memalign(NVME_PAGE_SIZE, sizeof(NVME_ADMIN_CONTROLLER_DATA));
	if (ctrlr->controller_data == NULL) {
		printf("nvme_identify: ERROR - out of memory\n");
		return NVME_OUT_OF_RESOURCES;
	}

	sq  = ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX] + ctrlr->sq_t_dbl[NVME_ADMIN_QUEUE_INDEX];

	memset(sq, 0, sizeof(NVME_SQ));

	sq->opc = NVME_ADMIN_IDENTIFY_OPC;
	sq->cid = ctrlr->cid[NVME_ADMIN_QUEUE_INDEX]++;

	/* Identify structure is 4Kb in size. Fits in aligned 1 PAGE */
	sq->prp[0] = (uintptr_t)virt_to_phys(ctrlr->controller_data);
	/* Set bit 0 (Cns bit) to 1 to identify a controller */
	sq->cdw10 = 1;

	status = nvme_do_one_cmd_synchronous(ctrlr,
				NVME_ADMIN_QUEUE_INDEX,
				NVME_ASQ_SIZE,
				NVME_ACQ_SIZE,
				NVME_GENERIC_TIMEOUT);
	if (NVME_ERROR(status))
		return status;

	ctrlr->controller_data->sn[19] = 0;
	ctrlr->controller_data->mn[39] = 0;
	DEBUG(printf(" == NVME IDENTIFY CONTROLLER DATA ==\n");)
	DEBUG(printf("    PCI VID   : 0x%x\n", ctrlr->controller_data->vid);)
	DEBUG(printf("    PCI SSVID : 0x%x\n", ctrlr->controller_data->ssvid);)
	DEBUG(printf("    SN        : %s\n",   (char *)(ctrlr->controller_data->sn));)
	DEBUG(printf("    MN        : %s\n",   (char *)(ctrlr->controller_data->mn));)
	DEBUG(printf("    RAB       : 0x%x\n", ctrlr->controller_data->rab);)
	DEBUG(printf("    AERL      : 0x%x\n", ctrlr->controller_data->aerl);)
	DEBUG(printf("    SQES      : 0x%x\n", ctrlr->controller_data->sqes);)
	DEBUG(printf("    CQES      : 0x%x\n", ctrlr->controller_data->cqes);)
	DEBUG(printf("    NN        : 0x%x\n", ctrlr->controller_data->nn);)

	printf("Identified NVMe model %s\n",
	       (char *)ctrlr->controller_data->mn);

	return status;
}

static int nvme_device_self_test(BlockDevOps *me, uint32_t cmd)
{
	NvmeDrive *drive = container_of(me, NvmeDrive, dev.ops);
	NVME_SQ *sq;
	NvmeCtrlr *ctrlr = drive->ctrlr;

	sq = ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX] +
	     ctrlr->sq_t_dbl[NVME_ADMIN_QUEUE_INDEX];

	memset(sq, 0, sizeof(*sq));

	sq->opc = NVME_ADMIN_DEV_SELF_TEST;

	sq->cid = ctrlr->cid[NVME_ADMIN_QUEUE_INDEX]++;
	sq->nsid = NVME_NSID_ALL; // NSID_ALL for controller.
	sq->cdw10 = cmd;

	NVME_STATUS status = nvme_do_one_cmd_synchronous(
		ctrlr, NVME_ADMIN_QUEUE_INDEX, NVME_ASQ_SIZE, NVME_ACQ_SIZE,
		NVME_GENERIC_TIMEOUT);

	if (NVME_ERROR(status)) {
		printf("%s: error %d of nvme_do_one_cmd_synchronous\n",
		       __func__, status);
		return status;
	}

	return status;
}

static int nvme_read_smart_log(BlockDevOps *me, HealthInfo *smart)
{
	NvmeDrive *drive = container_of(me, NvmeDrive, dev.ops);

	assert(sizeof(NvmeSmartLogData) == 512);

	smart->type = STORAGE_INFO_TYPE_NVME;
	NVME_STATUS status = nvme_read_log_page(drive, NVME_LOG_SMART,
						&smart->data.nvme_data,
						sizeof(smart->data.nvme_data));
	if (NVME_ERROR(status)) {
		printf("%s: fail to read log page: %d\n", __func__, status);
		return status;
	}

	return NVME_SUCCESS;
}

static int nvme_read_test_log(BlockDevOps *me, StorageTestLog *result)
{
	NvmeDrive *drive = container_of(me, NvmeDrive, dev.ops);

	_Static_assert(sizeof(NvmeTestLogData) == 32, "NvmeTestLogData");

	result->type = STORAGE_INFO_TYPE_NVME;
	NVME_STATUS status = nvme_read_log_page(drive, NVME_LOG_SELF_TEST,
						&result->data.nvme_data,
						sizeof(result->data.nvme_data));
	if (NVME_ERROR(status)) {
		printf("%s: fail to read log page: %d\n", __func__, status);
		return status;
	}

	return NVME_SUCCESS;
}

static int nvme_test_control(BlockDevOps *me, BlockDevTestOpsType ops)
{
	uint32_t cmd;
	switch (ops) {
	case BLOCKDEV_TEST_OPS_TYPE_SHORT:
		cmd = NVME_SELF_TEST_SHORT;
		break;
	case BLOCKDEV_TEST_OPS_TYPE_EXTENDED:
		cmd = NVME_SELF_TEST_EXTENDED;
		break;
	case BLOCKDEV_TEST_OPS_TYPE_STOP:
		cmd = NVME_SELF_TEST_ABORT;
		break;
	default:
		return NVME_UNSUPPORTED;
	}
	NVME_STATUS status = nvme_device_self_test(me, cmd);
	if (NVME_ERROR(status)) {
		printf("%s: cmd: %x, fail: %d\n", __func__, cmd, status);
		return status;
	}
	return NVME_SUCCESS;
}

static NVME_STATUS nvme_create_drive(NvmeCtrlr *ctrlr, uint32_t namespace_id,
				     unsigned int block_size, lba_t block_count)
{
	/* Create drive node. */
	NvmeDrive *nvme_drive = xzalloc(sizeof(*nvme_drive));
	static const int name_size = 21;
	char *name = xmalloc(name_size);
	snprintf(name, name_size, "NVMe Namespace %d", namespace_id);
	nvme_drive->dev.ops.read = &nvme_read;
	nvme_drive->dev.ops.write = &nvme_write;
	nvme_drive->dev.ops.new_stream = &new_simple_stream;
	nvme_drive->dev.ops.get_health_info = &nvme_read_smart_log;
	nvme_drive->dev.ops.get_test_log = &nvme_read_test_log;
	nvme_drive->dev.ops.test_control = &nvme_test_control;
	nvme_drive->dev.name = name;
	nvme_drive->dev.removable = 0;
	nvme_drive->dev.block_size = block_size;
	nvme_drive->dev.block_count = block_count;
	nvme_drive->ctrlr = ctrlr;
	nvme_drive->namespace_id = namespace_id;

	list_insert_after(&nvme_drive->dev.list_node, &fixed_block_devices);
	list_insert_after(&nvme_drive->list_node, &ctrlr->drives);

	printf("Added NVMe drive \"%s\" lbasize:%d, count:0x%llx\n",
	       nvme_drive->dev.name, nvme_drive->dev.block_size,
	       (uint64_t)nvme_drive->dev.block_count);

	return NVME_SUCCESS;
}

/* Sends the Identify Namespace command, creates NvmeDrives for each namespace */
static NVME_STATUS nvme_identify_namespaces(NvmeCtrlr *ctrlr) {
	NVME_SQ *sq;
	uint32_t *active_namespaces = NULL;
	NVME_ADMIN_NAMESPACE_DATA *namespace_data = NULL;
	int status = NVME_SUCCESS;

	if (ctrlr->controller_data == NULL) {
		printf("nvme_identify_namespaces: ERROR - must complete Identify command first\n");
		return NVME_INVALID_PARAMETER;
	}

	active_namespaces = dma_memalign(NVME_PAGE_SIZE, NVME_PAGE_SIZE);
	if (active_namespaces == NULL) {
		printf("nvme_identify_namespaces: ERROR - out of memory\n");
		return NVME_OUT_OF_RESOURCES;
	}

	namespace_data = dma_memalign(NVME_PAGE_SIZE, sizeof(NVME_ADMIN_NAMESPACE_DATA));
	if (namespace_data == NULL) {
		free(active_namespaces);
		printf("nvme_identify_namespaces: ERROR - out of memory\n");
		return NVME_OUT_OF_RESOURCES;
	}

	DEBUG(printf("nvme_identify_namespaces: Querying active namespaces");)
	sq  = ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX] + ctrlr->sq_t_dbl[NVME_ADMIN_QUEUE_INDEX];
	memset(sq, 0, sizeof(NVME_SQ));
	sq->opc = NVME_ADMIN_IDENTIFY_OPC;
	sq->cid = ctrlr->cid[NVME_ADMIN_QUEUE_INDEX]++;
	sq->nsid = 0;
	sq->cdw10 = NVME_ID_CNS_NS_ACTIVE_LIST;
	/* Active namespaces list is 4Kb in size. Fits in 1 aligned PAGE */
	sq->prp[0] = (uintptr_t)virt_to_phys(active_namespaces);
	status = nvme_do_one_cmd_synchronous(ctrlr,
			NVME_ADMIN_QUEUE_INDEX,
			NVME_ASQ_SIZE,
			NVME_ACQ_SIZE,
			NVME_GENERIC_TIMEOUT);
	if (NVME_ERROR(status))
		goto exit;

	for (uint32_t *ptr = active_namespaces;
	     ptr < active_namespaces + (NVME_PAGE_SIZE / sizeof(uint32_t));
	     ptr++) {
		uint32_t index = *ptr;
		/* The list contains active namespaces IDs and zeroes to fill up the rest.
		 * The standard doesn't guarantee the list is contiguous, only that valid
		 * IDs are in an increasing order, thus just ignore zeroes, the same way
		 * as nvme-cli does. */
		if (!index)
			continue;
		if (index > ctrlr->controller_data->nn) {
			printf("nvme_identify_namespaces: ERROR - index [%d] exceeds the maximum [%d]\n",
				index, ctrlr->controller_data->nn);
			status = NVME_DEVICE_ERROR;
			goto exit;
		}
		DEBUG(printf("nvme_identify_namespaces: Working on namespace %d\n",index);)

		sq  = ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX] + ctrlr->sq_t_dbl[NVME_ADMIN_QUEUE_INDEX];

		memset(sq, 0, sizeof(NVME_SQ));

		sq->opc = NVME_ADMIN_IDENTIFY_OPC;
		sq->cid = ctrlr->cid[NVME_ADMIN_QUEUE_INDEX]++;
		sq->nsid = index;

		/* Identify structure is 4Kb in size. Fits in 1 aligned PAGE */
		sq->prp[0] = (uintptr_t)virt_to_phys(namespace_data);
		/* Clear bit 0 (Cns bit) to identify a namespace */

		status = nvme_do_one_cmd_synchronous(ctrlr,
				NVME_ADMIN_QUEUE_INDEX,
				NVME_ASQ_SIZE,
				NVME_ACQ_SIZE,
				NVME_GENERIC_TIMEOUT);
		if (NVME_ERROR(status))
			goto exit;

		DEBUG(printf(" == NVME IDENTIFY NAMESPACE [%d] DATA ==\n", index);)
		DEBUG(printf("    NSZE        : 0x%llx\n", namespace_data->nsze);)
		DEBUG(printf("    NCAP        : 0x%llx\n", namespace_data->ncap);)
		DEBUG(printf("    NUSE        : 0x%llx\n", namespace_data->nuse);)
		DEBUG(printf("    LBAF0.LBADS : 0x%x\n", (namespace_data->lba_format[0].lbads));)

		if (namespace_data->nsze == 0) {
			printf("nvme_identify_namespaces: ERROR - namespace %d has zero size\n", index);
			status = NVME_DEVICE_ERROR;
			goto exit;
		} else {
			unsigned int block_size =
				2 << (namespace_data->lba_format[
				      namespace_data->flbas & 0xF].lbads - 1);
			status = nvme_create_drive(ctrlr, index, block_size,
						   namespace_data->nsze);
			if (NVME_ERROR(status))
				goto exit;
		}
	}

 exit:
	if (active_namespaces != NULL)
		free(active_namespaces);
	if (namespace_data != NULL)
		free(namespace_data);

	return status;
}

static int is_nvme_ctrlr(pcidev_t dev)
{
	if (pci_read_config8(dev, REG_PROG_IF) != PCI_IF_NVMHCI)
		return 0;
	if (pci_read_config8(dev, REG_SUBCLASS) != PCI_CLASS_MASS_STORAGE_NVM)
		return 0;
	if (pci_read_config8(dev, REG_CLASS) != PCI_CLASS_MASS_STORAGE)
		return 0;
	return 1;
}

static NvmeModelData *nvme_match_static_model(NvmeCtrlr *ctrlr)
{
	NvmeModelData *model;
	list_for_each(model, ctrlr->static_model_data, list_node)
		if (strstr((const char *)ctrlr->controller_data->mn,
			   model->model_id))
			return model;
	return NULL;
}

static int nvme_shutdown(struct CleanupFunc *cleanup, CleanupType type)
{
	NvmeCtrlr *ctrlr = (NvmeCtrlr *)cleanup->data;
	NvmeDrive *drive, *prev = NULL;
	int status = NVME_SUCCESS;

	if (ctrlr == NULL)
		return 1;

	/* Only disable controller if initialized */
	if (ctrlr->enabled) {
		switch (type) {
		case CleanupOnReboot:
		case CleanupOnPowerOff:
			printf("Shutting down NVMe controller.\n");
			nvme_shutdown_controller(ctrlr);
		case CleanupOnLegacy:
		case CleanupOnHandoff:
		default:
			printf("Disabling NVMe controller.\n");
			status = nvme_disable_controller(ctrlr);
			if (NVME_ERROR(status))
				return 1;
		}
		ctrlr->enabled = 0;
	}

	list_for_each(drive, ctrlr->drives, list_node) {
		free(prev);
		prev = drive;
	}
	free(prev);
	free(ctrlr->controller_data);
	free(ctrlr->prp_list);
	free(ctrlr->buffer);
	free(ctrlr);
	return 0;
}

/* Initialization entrypoint */
static int nvme_ctrlr_init(BlockDevCtrlrOps *me)
{
	NvmeCtrlr *ctrlr = container_of(me, NvmeCtrlr, ctrlr.ops);
	pcidev_t dev = ctrlr->dev;
	int status = NVME_SUCCESS;

	/* If this is not an NVMe device, check if it is a root port */
	if (!is_nvme_ctrlr(dev)) {
		uint8_t header_type = pci_read_config8(dev, REG_HEADER_TYPE);
		header_type &= 0x7f;
		if (header_type != HEADER_TYPE_BRIDGE) {
			status = NVME_NOT_FOUND;
			printf("PCIe bridge not found @ %02x:%02x:%02x\n",
			       PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));
			goto exit;
		}

		/* Look for NVMe device on this root port */
		uint32_t bus = pci_read_config32(dev, REG_PRIMARY_BUS);
		bus = (bus >> 8) & 0xff;
		dev = PCI_DEV(bus, 0, 0);
		if (!is_nvme_ctrlr(dev)) {
			status = NVME_NOT_FOUND;
			printf("NVMe device not found @ %02x:%02x:%02x\n",
			       PCI_BUS(dev), PCI_SLOT(dev), PCI_FUNC(dev));
			goto exit;
		}

		/* Update the device pointer */
		ctrlr->dev = dev;
	}

	printf("Initializing NVMe controller %04x:%04x\n",
		pci_read_config16(ctrlr->dev, REG_VENDOR_ID),
		pci_read_config16(ctrlr->dev, REG_DEVICE_ID));

	pci_set_bus_master(dev);

	/* Read the Controller Capabilities register */
	ctrlr->ctrlr_regs = pci_read_resource(dev,0);
	ctrlr->ctrlr_regs = ctrlr->ctrlr_regs & ~0x7;
	ctrlr->cap = readll(ctrlr->ctrlr_regs + NVME_CAP_OFFSET);

	/* Verify that the NVM command set is supported */
	if (NVME_CAP_CSS(ctrlr->cap) != NVME_CAP_CSS_NVM) {
		printf("NVMe Cap CSS not NVMe (CSS=%01x.\n",(uint8_t)NVME_CAP_CSS(ctrlr->cap));
		status = NVME_UNSUPPORTED;
		goto exit;
	}

	/* Driver only supports 4k page size */
	if (NVME_CAP_MPSMIN(ctrlr->cap) > NVME_PAGE_SHIFT) {
		printf("NVMe driver only supports 4k page size.\n");
		status = NVME_UNSUPPORTED;
		goto exit;
	}

	/* Calculate max io sq/cq sizes based on MQES */
	ctrlr->iosq_sz = (NVME_CSQ_SIZE > NVME_CAP_MQES(ctrlr->cap)) ? NVME_CAP_MQES(ctrlr->cap) : NVME_CSQ_SIZE;
	ctrlr->iocq_sz = (NVME_CCQ_SIZE > NVME_CAP_MQES(ctrlr->cap)) ? NVME_CAP_MQES(ctrlr->cap) : NVME_CCQ_SIZE;
	DEBUG(printf("iosq_sz = %u, iocq_sz = %u\n",ctrlr->iosq_sz,ctrlr->iocq_sz);)

	/* Allocate enough PRP List memory for max queue depth commands */
	for (unsigned int list_index = 0; list_index < ctrlr->iosq_sz; list_index++) {
		ctrlr->prp_list[list_index] = dma_memalign(NVME_PAGE_SIZE, NVME_PAGE_SIZE);
		if (!(ctrlr->prp_list[list_index])) {
			printf("NVMe driver failed to allocate prp list %u memory\n",list_index);
			status = NVME_OUT_OF_RESOURCES;
			goto exit;
		}
		memset(ctrlr->prp_list[list_index], 0, NVME_PAGE_SIZE);
	}

	/* Allocate queue memory block */
	ctrlr->buffer = dma_memalign(NVME_PAGE_SIZE, (NVME_NUM_QUEUES * 2) * NVME_PAGE_SIZE);
	if (!(ctrlr->buffer)) {
		printf("NVMe driver failed to allocate queue buffer\n");
		status = NVME_OUT_OF_RESOURCES;
		goto exit;
	}
	memset(ctrlr->buffer, 0, (NVME_NUM_QUEUES * 2) * NVME_PAGE_SIZE);

	/* Disable controller */
	status = nvme_disable_controller(ctrlr);
	if (NVME_ERROR(status))
		goto exit;

	/* Create Admin queue pair */
	NVME_AQA aqa = 0;
	NVME_ASQ asq = 0;
	NVME_ACQ acq = 0;

	/* Verify defined queue sizes are within NVME_PAGE_SIZE limits */
	#if NVME_ASQ_SIZE != 2
	#error "Unsupported Admin SQ size defined"
	#endif
	#if NVME_ACQ_SIZE != 2
	#error "Unsupported Admin CQ size defined"
	#endif
	#if (NVME_CSQ_SIZE < 2) || (NVME_CSQ_SIZE > (NVME_PAGE_SIZE / 64))
	#error "Unsupported IO SQ size defined"
	#endif
	#if (NVME_CCQ_SIZE < 2) || (NVME_CCQ_SIZE > (NVME_PAGE_SIZE / 64))
	#error "Unsupported IO CQ size defined"
	#endif

	/* Set number of entries Admin submission & completion queues. */
	aqa |= NVME_AQA_ASQS(NVME_ASQ_SIZE);
	aqa |= NVME_AQA_ACQS(NVME_ACQ_SIZE);
	/* Address of Admin submission queue. */
	asq  = (uintptr_t)virt_to_phys(ctrlr->buffer);
	ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX] = (NVME_SQ *)ctrlr->buffer;
	/* Address of Admin completion queue. */
	acq  = (uintptr_t)virt_to_phys(ctrlr->buffer + NVME_PAGE_SIZE);
	ctrlr->cq_buffer[NVME_ADMIN_QUEUE_INDEX] = (NVME_CQ *)(ctrlr->buffer + NVME_PAGE_SIZE);
	/* Address of I/O submission & completion queues */
	ctrlr->sq_buffer[NVME_IO_QUEUE_INDEX] =
		(NVME_SQ *)(ctrlr->buffer + 2 * NVME_PAGE_SIZE);
	ctrlr->cq_buffer[NVME_IO_QUEUE_INDEX] =
		(NVME_CQ *)(ctrlr->buffer + 3 * NVME_PAGE_SIZE);

	DEBUG(printf("Private->Buffer = [%p]\n", (void *)virt_to_phys(ctrlr->buffer));)
	DEBUG(printf("Admin Queue Attributes = [%X]\n", aqa);)
	DEBUG(printf("Admin Submission Queue (sq_buffer[ADMIN]) = [%p]\n", (void *)virt_to_phys(ctrlr->sq_buffer[NVME_ADMIN_QUEUE_INDEX]));)
	DEBUG(printf("Admin Completion Queue (cq_buffer[ADMIN]) = [%p]\n", (void *)virt_to_phys(ctrlr->cq_buffer[NVME_ADMIN_QUEUE_INDEX]));)
	DEBUG(printf("I/O   Submission Queue (sq_buffer[NVME_IO_QUEUE]) = [%p]\n", (void *)virt_to_phys(ctrlr->sq_buffer[NVME_IO_QUEUE_INDEX]));)
	DEBUG(printf("I/O   Completion Queue (cq_buffer[NVME_IO_QUEUE]) = [%p]\n", (void *)virt_to_phys(ctrlr->cq_buffer[NVME_IO_QUEUE_INDEX]));)

	/* Write AQA */
	writel(aqa, ctrlr->ctrlr_regs + NVME_AQA_OFFSET);
	/* Write ASQ */
	writell(asq, ctrlr->ctrlr_regs + NVME_ASQ_OFFSET);
	/* Write ACQ */
	writell(acq, ctrlr->ctrlr_regs + NVME_ACQ_OFFSET);

	/* Enable controller */
	status = nvme_enable_controller(ctrlr);
	if (NVME_ERROR(status))
		goto exit;
	ctrlr->enabled = 1;

	/* Set IO queue count */
	status = nvme_set_queue_count(ctrlr, NVME_NUM_IO_QUEUES);
	if (NVME_ERROR(status))
		goto exit;

	/* Create IO queue pair */
	status = nvme_create_cq(ctrlr, NVME_IO_QUEUE_INDEX, ctrlr->iocq_sz);
	if (NVME_ERROR(status))
		goto exit;

	status = nvme_create_sq(ctrlr, NVME_IO_QUEUE_INDEX, ctrlr->iosq_sz);
	if (NVME_ERROR(status))
		goto exit;

	/* Identify */
	status = nvme_identify(ctrlr);
	if (NVME_ERROR(status))
		goto exit;

	NvmeModelData *model = nvme_match_static_model(ctrlr);
	if (model) {
		/* Create drive based on static namespace data */
		DEBUG(printf("Skip Identify Namespace and use static data\n");)
		status = nvme_create_drive(ctrlr, model->namespace_id,
				   model->block_size, model->block_count);
	} else {
		/* Identify Namespace and create drive nodes */
		status = nvme_identify_namespaces(ctrlr);
	}

	CleanupFunc *cleanup = xzalloc(sizeof(*cleanup));
	cleanup->cleanup = &nvme_shutdown;
	cleanup->types = CleanupOnHandoff | CleanupOnLegacy |
		CleanupOnReboot | CleanupOnPowerOff;
	cleanup->data = ctrlr;
	list_insert_after(&cleanup->list_node, &cleanup_funcs);

 exit:
	ctrlr->ctrlr.need_update = 0;

	if (status == NVME_UNSUPPORTED)
		printf("Unsupported NVMe controller found\n");
	return NVME_ERROR(status);
}

void nvme_add_static_namespace(NvmeCtrlr *ctrlr, uint32_t namespace_id,
			       unsigned int block_size, lba_t block_count,
			       const char *model_id)
{
	NvmeModelData *data = xzalloc(sizeof(*data));
	data->namespace_id = namespace_id;
	data->block_size = block_size;
	data->block_count = block_count;
	strncpy(data->model_id, model_id,
		MIN(strlen(model_id), NVME_MODEL_NUMBER_LEN));
	list_insert_after(&data->list_node, &ctrlr->static_model_data);
}

/* Setup controller initialization/shutdown callbacks.
 * Used in board.c to get handle to new ctrlr.
 */
NvmeCtrlr *new_nvme_ctrlr(pcidev_t dev)
{
	NvmeCtrlr *ctrlr = xzalloc(sizeof(*ctrlr));

	printf("Looking for NVMe Controller %p @ %02x:%02x:%02x\n",
		ctrlr, PCI_BUS(dev),PCI_SLOT(dev),PCI_FUNC(dev));

	ctrlr->ctrlr.ops.update = &nvme_ctrlr_init;
	ctrlr->ctrlr.need_update = 1;
	ctrlr->dev = dev;

	return ctrlr;
}
