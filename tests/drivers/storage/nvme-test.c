// SPDX-License-Identifier: GPL-2.0

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cmocka.h>
#include <libpayload.h>

#include "drivers/storage/blockdev.h"
#include "drivers/storage/bouncebuf.h"
#include "drivers/storage/nvme.h"
#include "tests/test.h"

// Undefine potential system macros that conflict with structure member names
#undef ds

// Target address and page for the arbitrary write
#define TARGET_ADDR 0x00007fff00000001ULL
#define TARGET_PAGE 0x00007fff00000000ULL

// We will include nvme.c directly to test static functions and ease mocking
#undef read16
#undef read32
#undef write32
#define read16(addr) mock_read16(addr)
#define read32(addr) mock_read32(addr)
#define write32(addr, val) mock_write32(addr, val)

// Forward declarations of our mocks with correct volatile qualifiers
uint16_t mock_read16(volatile const void *addr);
uint32_t mock_read32(volatile const void *addr);
void mock_write32(volatile void *addr, uint32_t val);

#include "drivers/storage/nvme.c"

// Global state for our fake device
static NvmeCtrlr *current_ctrlr = NULL;
static uint8_t fake_mmio[0x2000];

struct {
	uint64_t cap;
	uint32_t cc;
	uint32_t csts;
	uint32_t aqa;
	uint64_t asq;
	uint64_t acq;
	uint32_t sqtdbl[NVME_NUM_QUEUES];
	uint32_t cqhdbl[NVME_NUM_QUEUES];
} fake_regs;

struct {
	bool malicious_sqhd;
	uint8_t mdts;
	uint32_t last_sq_tail[NVME_NUM_QUEUES];
} fake_device_behavior;

// Define the global cleanup_funcs list to resolve link errors
struct list_node cleanup_funcs;

// Stub timing functions
void ndelay(uint64_t n) {}

// Mock bounce buffer functions to avoid linking bouncebuf.c (which has cache dependencies)
int bounce_buffer_start(struct bounce_buffer *state, void *data,
			size_t len, unsigned int flags)
{
	state->user_buffer = data;
	state->bounce_buffer = data;
	state->len = len;
	state->len_aligned = len;
	state->flags = flags;
	return 0;
}

int bounce_buffer_stop(struct bounce_buffer *state)
{
	return 0;
}

// PCI Mocks with correct signatures
uint8_t pci_read_config8(pcidev_t dev, uint16_t reg)
{
	if (reg == REG_PROG_IF)
		return PCI_IF_NVMHCI;
	if (reg == REG_SUBCLASS)
		return PCI_CLASS_MASS_STORAGE_NVM;
	if (reg == REG_CLASS)
		return PCI_CLASS_MASS_STORAGE;
	if (reg == REG_HEADER_TYPE)
		return 0;
	return 0;
}

uint16_t pci_read_config16(pcidev_t dev, uint16_t reg)
{
	if (reg == REG_VENDOR_ID)
		return 0x1234;
	if (reg == REG_DEVICE_ID)
		return 0x5678;
	return 0;
}

uint32_t pci_read_config32(pcidev_t dev, uint16_t reg)
{
	if (reg == REG_PRIMARY_BUS)
		return 0;
	return 0;
}

void pci_set_bus_master(pcidev_t dev) {}

uint32_t pci_read_resource(pcidev_t dev, int bar)
{
	return (uint32_t)(uintptr_t)fake_mmio;
}

// Initialize fake registers
void init_fake_regs(void)
{
	memset(&fake_regs, 0, sizeof(fake_regs));
	// CAP: MQES=64 (0x3f), CQR=1, TO=1, DSTRD=0 (stride=4), CSS=1 (NVM), MPSMIN=0 (4KB)
	fake_regs.cap = 0x200101003fULL;
}

// Process single fake command and write completion (accepts expected host phase tag)
static void handle_fake_device_command(int qid, uint32_t sq_idx, NVME_SQ *sq, uint16_t expected_host_pt)
{
	if (qid == NVME_ADMIN_QUEUE_INDEX) {
		if (sq->opc == NVME_ADMIN_IDENTIFY_OPC) {
			uint32_t cns = sq->cdw10 & 0xff;
			void *dma_dest = (void*)(uintptr_t)sq->prp[0];
			if (cns == 1) {
				// Identify Controller
				NVME_ADMIN_CONTROLLER_DATA *id_ctrlr = dma_dest;
				memset(id_ctrlr, 0, sizeof(*id_ctrlr));
				id_ctrlr->mdts = fake_device_behavior.mdts;
				id_ctrlr->nn = 1;
				strcpy((char*)id_ctrlr->mn, "Fake NVMe Device");
				strcpy((char*)id_ctrlr->sn, "123456");
			} else if (cns == 2) {
				// Active Namespace List
				uint32_t *ns_list = dma_dest;
				ns_list[0] = 1;
				ns_list[1] = 0;
			} else if (cns == 0) {
				// Identify Namespace
				NVME_ADMIN_NAMESPACE_DATA *id_ns = dma_dest;
				memset(id_ns, 0, sizeof(*id_ns));
				id_ns->nsze = 1000000;
				id_ns->ncap = 1000000;
				id_ns->flbas = 0;
				id_ns->lba_format[0].lbads = 9; // LBAF0: 512 bytes (2^9)
			}
		}
	}

	// Prepare completion entry
	size_t cq_size = (qid == NVME_ADMIN_QUEUE_INDEX) ? 2 : current_ctrlr->iocq_sz;
	NVME_CQ *cq_entry = &current_ctrlr->cq_buffer[qid][sq_idx];
	memset(cq_entry, 0, sizeof(*cq_entry));

	uint16_t sqhd = sq_idx + 1;
	if (sqhd >= cq_size)
		sqhd = 0;

	if (qid == NVME_IO_QUEUE_INDEX && fake_device_behavior.malicious_sqhd) {
		// Malicious behavior: return sqhd = 0x7fff to target specific address bits
		// and prevent cid reset (since 0x7fff != any valid sq_t_dbl)
		sqhd = 0x7fff;
	}

	cq_entry->sqhd = sqhd;
	cq_entry->sqid = qid;
	cq_entry->cid = sq->cid;

	// Set phase tag to opposite of the expected host pt
	cq_entry->flags = ((expected_host_pt ^ 1) & NVME_CQ_FLAGS_PHASE);
}

// MMIO Read Mock with correct volatile signatures
uint32_t mock_read32(volatile const void *addr)
{
	if (current_ctrlr && addr >= (volatile const void*)fake_mmio && addr < (volatile const void*)(fake_mmio + 0x2000)) {
		uintptr_t offset = (uintptr_t)addr - (uintptr_t)fake_mmio;
		if (offset == NVME_CAP_OFFSET)
			return fake_regs.cap & 0xffffffff;
		if (offset == NVME_CAP_OFFSET + 4)
			return fake_regs.cap >> 32;
		if (offset == NVME_CC_OFFSET)
			return fake_regs.cc;
		if (offset == NVME_CSTS_OFFSET) {
			if (fake_regs.cc & 1)
				fake_regs.csts |= 1; // CSTS.RDY = 1
			else
				fake_regs.csts &= ~1; // CSTS.RDY = 0
			return fake_regs.csts;
		}
		uint32_t dstrd = NVME_CAP_DSTRD(fake_regs.cap);
		for (int qid = 0; qid < NVME_NUM_QUEUES; qid++) {
			if (offset == NVME_SQTDBL_OFFSET(qid, dstrd))
				return fake_regs.sqtdbl[qid];
			if (offset == NVME_CQHDBL_OFFSET(qid, dstrd))
				return fake_regs.cqhdbl[qid];
		}
		return 0;
	}
	return *(volatile const uint32_t*)(uintptr_t)(addr);
}

// MMIO Write Mock (handles expected phase tag state machine logic)
void mock_write32(volatile void *addr, uint32_t val)
{
	if (current_ctrlr && addr >= (volatile void*)fake_mmio && addr < (volatile void*)(fake_mmio + 0x2000)) {
		uintptr_t offset = (uintptr_t)addr - (uintptr_t)fake_mmio;
		if (offset == NVME_CC_OFFSET) {
			fake_regs.cc = val;
			if (!(val & 1))
				fake_regs.csts &= ~1;
			return;
		}
		if (offset == NVME_AQA_OFFSET) {
			fake_regs.aqa = val;
			return;
		}
		if (offset == NVME_ASQ_OFFSET) {
			fake_regs.asq = (fake_regs.asq & 0xffffffff00000000ULL) | val;
			return;
		}
		if (offset == NVME_ASQ_OFFSET + 4) {
			fake_regs.asq = (fake_regs.asq & 0xffffffffULL) | ((uint64_t)val << 32);
			return;
		}
		if (offset == NVME_ACQ_OFFSET) {
			fake_regs.acq = (fake_regs.acq & 0xffffffff00000000ULL) | val;
			return;
		}
		if (offset == NVME_ACQ_OFFSET + 4) {
			fake_regs.acq = (fake_regs.acq & 0xffffffffULL) | ((uint64_t)val << 32);
			return;
		}

		uint32_t dstrd = NVME_CAP_DSTRD(fake_regs.cap);
		for (int qid = 0; qid < NVME_NUM_QUEUES; qid++) {
			if (offset == NVME_SQTDBL_OFFSET(qid, dstrd)) {
				fake_regs.sqtdbl[qid] = val;
				print_message("mock_write32: SQTDBL write for qid %d, val %d (prev_tail %d)\n", qid, val, fake_device_behavior.last_sq_tail[qid]);
				// Process new commands!
				uint32_t prev_tail = fake_device_behavior.last_sq_tail[qid];
				uint32_t new_tail = val;
				size_t sq_size = (qid == NVME_ADMIN_QUEUE_INDEX) ? 2 : current_ctrlr->iosq_sz;
				size_t cq_size = sq_size;
				uint32_t idx = prev_tail;
				uint16_t expected_host_pt = current_ctrlr->pt[qid];
				while (idx != new_tail) {
					NVME_SQ *sq_entry = &current_ctrlr->sq_buffer[qid][idx];
					handle_fake_device_command(qid, idx, sq_entry, expected_host_pt);
					// Track host-side phase tag flip on queue wrap-around
					if (idx == cq_size - 1) {
						expected_host_pt ^= 1;
					}
					idx = (idx + 1) % sq_size;
				}
				fake_device_behavior.last_sq_tail[qid] = new_tail;
				return;
			}
			if (offset == NVME_CQHDBL_OFFSET(qid, dstrd)) {
				fake_regs.cqhdbl[qid] = val;
				return;
			}
		}
		return;
	}
	*(volatile uint32_t*)(uintptr_t)(addr) = val;
}

// Memory read mock with correct volatile signature
uint16_t mock_read16(volatile const void *addr)
{
	return *(volatile const uint16_t*)(uintptr_t)(addr);
}

// Test case for the vulnerability
static void test_nvme_vulnerability(void **state)
{
	init_fake_regs();
	memset(&fake_device_behavior, 0, sizeof(fake_device_behavior));
	fake_device_behavior.mdts = 3; // 32KB max transfer
	fake_device_behavior.malicious_sqhd = true; // Trigger!

	pcidev_t fake_dev = 0x100;
	NvmeCtrlr *ctrlr = new_nvme_ctrlr(fake_dev);
	assert_non_null(ctrlr);
	current_ctrlr = ctrlr;

	// Initialize controller (will perform Admin commands, handled by fake device)
	int status = ctrlr->ctrlr.ops.update(&ctrlr->ctrlr.ops);
	assert_int_equal(status, 0);

	// Verify drive registration
	assert_false(list_is_empty(&fixed_block_devices));
	BlockDev *bdev = container_of(list_first(&fixed_block_devices), BlockDev, list_node);
	assert_non_null(bdev);

	print_message("DEBUG LAYOUT:\n");
	print_message("  ctrlr->prp_list start: %p\n", (void*)ctrlr->prp_list);
	print_message("  ctrlr->buffer pointer addr: %p\n", (void*)&ctrlr->buffer);
	print_message("  ctrlr->buffer pointer value: %p\n", (void*)ctrlr->buffer);
	print_message("  ctrlr->prp_list[10] value: %p\n", (void*)ctrlr->prp_list[10]);
	PrpList **oob_prp_list_11 = (PrpList**)&ctrlr->prp_list[11];
	print_message("  ctrlr->prp_list[11] (OOB) value: %p\n", (void*)*oob_prp_list_11);
	PrpList **oob_prp_list_18 = (PrpList**)&ctrlr->prp_list[18];
	print_message("  ctrlr->prp_list[18] (OOB) value: %p\n", (void*)*oob_prp_list_18);

	// Map intermediate pages to prevent segfaults during OOB progression
	void *dummy_map1 = mmap((void*)0x0000000500000000ULL, 4096, PROT_READ|PROT_WRITE,
				MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
	assert_ptr_not_equal(dummy_map1, MAP_FAILED);

	void *dummy_map2 = mmap((void*)0x0000000a00000000ULL, 4096, PROT_READ|PROT_WRITE,
				MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
	assert_ptr_not_equal(dummy_map2, MAP_FAILED);

	// Map target page to intercept arbitrary write
	void *target_map = mmap((void*)TARGET_PAGE, 4096, PROT_READ|PROT_WRITE,
				MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
	assert_ptr_not_equal(target_map, MAP_FAILED);
	assert_ptr_equal(target_map, (void*)TARGET_PAGE);
	memset(target_map, 0, 4096);

	// Prepare transfer buffer: 608KB (19 commands of 32KB)
	size_t transfer_size = 608 * 1024;
	uint8_t *real_buffer = malloc(transfer_size + 4096);
	assert_non_null(real_buffer);
	uint8_t *transfer_buffer = (uint8_t*)ALIGN_UP((uintptr_t)real_buffer, 4096);
	memset(transfer_buffer, 0xAA, transfer_size);

	// Trigger read operation: 1216 blocks (1216 * 512 = 608KB)
	lba_t blocks_read = bdev->ops.read(&bdev->ops, 0, 1216, transfer_buffer);

	// REGRESSION DEVIATION CHECK:
	// In a fixed/safe driver, the read MUST detect the OOB submission and return 0 (error).
	// In the vulnerable driver, it completes successfully (returns 1216) but corrupts memory.
	assert_int_equal(blocks_read, 0);

	// Verify that NO arbitrary write occurred at TARGET_ADDR!
	uint64_t expected_value = (uint64_t)(uintptr_t)(transfer_buffer + 593920);
	uint64_t actual_value = *(volatile uint64_t*)TARGET_ADDR;

	print_message("Target Address (%p) Content: %p\n", (void*)TARGET_ADDR, (void*)(uintptr_t)actual_value);
	print_message("Expected Address (if OOB write occurred):  %p\n", (void*)(uintptr_t)expected_value);

	// TARGET_ADDR content must remain 0
	assert_int_equal(actual_value, 0);

	// Verify that intermediate dummy spaces also remained clean (no writes occurred)
	uint64_t dummy1_val = *(volatile uint64_t*)0x0000000500000000ULL;
	uint64_t dummy2_val = *(volatile uint64_t*)0x0000000a00000000ULL;
	assert_int_equal(dummy1_val, 0);
	assert_int_equal(dummy2_val, 0);

	// Cleanup
	munmap(dummy_map1, 4096);
	munmap(dummy_map2, 4096);
	munmap(target_map, 4096);
	free(real_buffer);
}

static int setup(void **state)
{
	_list_init(&fixed_block_devices);
	_list_init(&removable_block_devices);
	_list_init(&cleanup_funcs);
	current_ctrlr = NULL;
	return 0;
}

static int teardown(void **state)
{
	if (current_ctrlr) {
		// Basic cleanup of allocations in nvme_ctrlr_init
		for (int i = 0; i < current_ctrlr->iosq_sz; i++) {
			if (current_ctrlr->prp_list[i])
				free(current_ctrlr->prp_list[i]);
		}
		if (current_ctrlr->buffer)
			free(current_ctrlr->buffer);
		if (current_ctrlr->controller_data)
			free(current_ctrlr->controller_data);
		free(current_ctrlr);
	}
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_nvme_vulnerability, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}

