/* SPDX-License-Identifier: GPL-2.0-only */

#include "cse_internal.h"
#include "cse.h"
#include "drivers/soc/common/iomap.h"
#include "me.h"

#define MAX_HECI_MESSAGE_RETRY_COUNT 5

/* Wait up to 15 sec for HECI to get ready */
#define HECI_DELAY_READY_MS	(15 * 1000)
/* Wait up to 100 usec between circular buffer polls */
#define HECI_DELAY_US		100
/* Wait up to 5 sec for CSE to chew something we sent */
#define HECI_SEND_TIMEOUT_MS	(5 * 1000)
/* Wait up to 5 sec for CSE to blurp a reply */
#define HECI_READ_TIMEOUT_MS	(5 * 1000)
/* Wait up to 1 ms for CSE CIP */
#define HECI_CIP_TIMEOUT_US	1000
/* Wait up to 5 seconds for CSE to boot from RO(BP1) */
#define CSE_DELAY_BOOT_TO_RO_MS	(5 * 1000)

#define SLOT_SIZE		sizeof(uint32_t)

#define MMIO_CSE_CB_WW		0x00
#define MMIO_HOST_CSR		0x04
#define MMIO_CSE_CB_RW		0x08
#define MMIO_CSE_CSR		0x0c
#define MMIO_CSE_DEVIDLE	0x800
#define  CSE_DEV_IDLE		(1 << 2)
#define  CSE_DEV_CIP		(1 << 0)

#define CSR_IE			(1 << 0)
#define CSR_IS			(1 << 1)
#define CSR_IG			(1 << 2)
#define CSR_READY		(1 << 3)
#define CSR_RESET		(1 << 4)
#define CSR_RP_START		8
#define CSR_RP			(((1 << 8) - 1) << CSR_RP_START)
#define CSR_WP_START		16
#define CSR_WP			(((1 << 8) - 1) << CSR_WP_START)
#define CSR_CBD_START		24
#define CSR_CBD			(((1 << 8) - 1) << CSR_CBD_START)

#define MEI_HDR_IS_COMPLETE	(1 << 31)
#define MEI_HDR_LENGTH_START	16
#define MEI_HDR_LENGTH_SIZE	9
#define MEI_HDR_LENGTH		(((1 << MEI_HDR_LENGTH_SIZE) - 1) \
					<< MEI_HDR_LENGTH_START)
#define MEI_HDR_HOST_ADDR_START	8
#define MEI_HDR_HOST_ADDR	(((1 << 8) - 1) << MEI_HDR_HOST_ADDR_START)
#define MEI_HDR_CSE_ADDR_START	0
#define MEI_HDR_CSE_ADDR	(((1 << 8) - 1) << MEI_HDR_CSE_ADDR_START)


#define PCH_PWRM_BASE_ADDRESS		0xfe000000

#define ETR				0x1048
#define   CF9_LOCK				(1 << 31)
#define   CF9_GLB_RST			(1 << 20)

#define PM1_CNT         0x04
#define   SLP_EN        (1 << 13)
#define   SLP_TYP       (7 << 10)
#define   SLP_TYP_S0    (0 << 10)
#define   SLP_TYP_S1    (1 << 10)
#define   SLP_TYP_S3    (5 << 10)
#define   SLP_TYP_S4    (6 << 10)
#define   SLP_TYP_S5    (7 << 10)

bool acpi_get_sleep_type_s3(void)
{
	uint32_t reg32 = inl(ACPI_BASE_ADDRESS + PM1_CNT);
	if ((reg32 & SLP_TYP) == SLP_TYP_S3)
		return true;
	return false;
}

/* Get HECI BAR 0 from PCI configuration space */
static uintptr_t get_cse_bar(pcidev_t dev)
{
	uintptr_t bar;

	bar = pci_read_config32(dev, PCI_BASE_ADDRESS_0);
	assert(bar != 0);
	/*
	 * Bits 31-12 are the base address as per EDS for SPI,
	 * Don't care about 0-11 bit
	 */
	return bar & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK;
}

static uint32_t read_bar(pcidev_t dev, uint32_t offset)
{
	return read32((void *)get_cse_bar(dev) + offset);
}

static void write_bar(pcidev_t dev, uint32_t offset, uint32_t val)
{
	return write32((void *)get_cse_bar(dev) + offset, val);
}

static uint32_t read_cse_csr(void)
{
	return read_bar(PCH_DEV_CSE, MMIO_CSE_CSR);
}

static uint32_t read_host_csr(void)
{
	return read_bar(PCH_DEV_CSE, MMIO_HOST_CSR);
}

static void write_host_csr(uint32_t data)
{
	write_bar(PCH_DEV_CSE, MMIO_HOST_CSR, data);
}

static size_t filled_slots(uint32_t data)
{
	uint8_t wp, rp;
	rp = data >> CSR_RP_START;
	wp = data >> CSR_WP_START;
	return (uint8_t)(wp - rp);
}

static size_t cse_filled_slots(void)
{
	return filled_slots(read_cse_csr());
}

static size_t host_empty_slots(void)
{
	uint32_t csr;
	csr = read_host_csr();

	return ((csr & CSR_CBD) >> CSR_CBD_START) - filled_slots(csr);
}

static void clear_int(void)
{
	uint32_t csr;
	csr = read_host_csr();
	csr |= CSR_IS;
	write_host_csr(csr);
}

static uint32_t read_slot(void)
{
	return read_bar(PCH_DEV_CSE, MMIO_CSE_CB_RW);
}

static void write_slot(uint32_t val)
{
	write_bar(PCH_DEV_CSE, MMIO_CSE_CB_WW, val);
}

static int wait_write_slots(size_t cnt)
{
	struct stopwatch sw;

	stopwatch_init_msecs_expire(&sw, HECI_SEND_TIMEOUT_MS);
	while (host_empty_slots() < cnt) {
		udelay(HECI_DELAY_US);
		if (stopwatch_expired(&sw)) {
			printk(BIOS_ERR, "HECI: timeout, buffer not drained\n");
			return 0;
		}
	}
	return 1;
}

static int wait_read_slots(size_t cnt)
{
	struct stopwatch sw;

	stopwatch_init_msecs_expire(&sw, HECI_READ_TIMEOUT_MS);
	while (cse_filled_slots() < cnt) {
		udelay(HECI_DELAY_US);
		if (stopwatch_expired(&sw)) {
			printk(BIOS_ERR, "HECI: timed out reading answer!\n");
			return 0;
		}
	}
	return 1;
}

/* get number of full 4-byte slots */
static size_t bytes_to_slots(size_t bytes)
{
	return ALIGN_UP(bytes, SLOT_SIZE) / SLOT_SIZE;
}

static int cse_ready(void)
{
	uint32_t csr;
	csr = read_cse_csr();
	return csr & CSR_READY;
}

static bool cse_check_hfs1_com(int mode)
{
	union me_hfsts1 hfs1;
	hfs1.data = me_read_config32(PCI_ME_HFSTS1);
	return hfs1.fields.operation_mode == mode;
}

bool cse_is_hfs1_cws_normal(void)
{
	union me_hfsts1 hfs1;
	hfs1.data = me_read_config32(PCI_ME_HFSTS1);
	if (hfs1.fields.working_state == ME_HFS1_CWS_NORMAL)
		return true;
	return false;
}

bool cse_is_hfs1_com_normal(void)
{
	return cse_check_hfs1_com(ME_HFS1_COM_NORMAL);
}

bool cse_is_hfs1_com_secover_mei_msg(void)
{
	return cse_check_hfs1_com(ME_HFS1_COM_SECOVER_MEI_MSG);
}

bool cse_is_hfs1_com_soft_temp_disable(void)
{
	return cse_check_hfs1_com(ME_HFS1_COM_SOFT_TEMP_DISABLE);
}

/*
 * Starting from TGL platform, HFSTS1.spi_protection_mode replaces mfg_mode to indicate
 * SPI protection status as well as end-of-manufacturing(EOM) status where EOM flow is
 * triggered in single staged operation (either through first boot with required MFIT
 * configuratin or FPT /CLOSEMANUF).
 * In staged manufacturing flow, spi_protection_mode alone doesn't indicate the EOM status.
 *
 * HFSTS1.spi_protection_mode description:
 * mfg_mode = 0 means SPI protection is on.
 * mfg_mode = 1 means SPI is unprotected.
 */
bool cse_is_hfs1_spi_protected(void)
{
	union me_hfsts1 hfs1;
	hfs1.data = me_read_config32(PCI_ME_HFSTS1);
	return !hfs1.fields.mfg_mode;
}

bool cse_is_hfs3_fw_sku_lite(void)
{
	union me_hfsts3 hfs3;
	hfs3.data = me_read_config32(PCI_ME_HFSTS3);
	return hfs3.fields.fw_sku == ME_HFS3_FW_SKU_LITE;
}

/* Makes the host ready to communicate with CSE */
void cse_set_host_ready(void)
{
	uint32_t csr;
	csr = read_host_csr();
	csr &= ~CSR_RESET;
	csr |= (CSR_IG | CSR_READY);
	write_host_csr(csr);
}

/* Polls for ME mode ME_HFS1_COM_SECOVER_MEI_MSG for 15 seconds */
uint8_t cse_wait_sec_override_mode(void)
{
	struct stopwatch sw;
	stopwatch_init_msecs_expire(&sw, HECI_DELAY_READY_MS);
	while (!cse_is_hfs1_com_secover_mei_msg()) {
		udelay(HECI_DELAY_US);
		if (stopwatch_expired(&sw)) {
			printk(BIOS_ERR, "HECI: Timed out waiting for SEC_OVERRIDE mode!\n");
			return 0;
		}
	}
	printk(BIOS_DEBUG, "HECI: CSE took < %d ms to enter security override mode\n",
			HECI_DELAY_READY_MS);
	return 1;
}

/*
 * Polls for CSE's current operation mode 'Soft Temporary Disable'.
 * The CSE enters the current operation mode when it boots from RO(BP1).
 */
uint8_t cse_wait_com_soft_temp_disable(void)
{
	struct stopwatch sw;
	stopwatch_init_msecs_expire(&sw, CSE_DELAY_BOOT_TO_RO_MS);
	while (!cse_is_hfs1_com_soft_temp_disable()) {
		udelay(HECI_DELAY_US);
		if (stopwatch_expired(&sw)) {
			printk(BIOS_ERR, "HECI: Timed out waiting for CSE to boot from RO!\n");
			return 0;
		}
	}
	printk(BIOS_SPEW, "HECI: CSE took < %d ms to boot from RO\n",
			CSE_DELAY_BOOT_TO_RO_MS);
	return 1;
}

static int wait_heci_ready(void)
{
	struct stopwatch sw;

	stopwatch_init_msecs_expire(&sw, HECI_DELAY_READY_MS);
	while (!cse_ready()) {
		udelay(HECI_DELAY_US);
		if (stopwatch_expired(&sw))
			return 0;
	}

	return 1;
}

static void host_gen_interrupt(void)
{
	uint32_t csr;
	csr = read_host_csr();
	csr |= CSR_IG;
	write_host_csr(csr);
}

static size_t hdr_get_length(uint32_t hdr)
{
	return (hdr & MEI_HDR_LENGTH) >> MEI_HDR_LENGTH_START;
}

static int
send_one_message(uint32_t hdr, const void *buff)
{
	size_t pend_len, pend_slots, remainder, i;
	uint32_t tmp;
	const uint32_t *p = buff;

	/* Get space for the header */
	if (!wait_write_slots(1))
		return 0;

	/* First, write header */
	write_slot(hdr);

	pend_len = hdr_get_length(hdr);
	pend_slots = bytes_to_slots(pend_len);

	if (!wait_write_slots(pend_slots))
		return 0;

	/* Write the body in whole slots */
	i = 0;
	while (i < ALIGN_DOWN(pend_len, SLOT_SIZE)) {
		write_slot(*p++);
		i += SLOT_SIZE;
	}

	remainder = pend_len % SLOT_SIZE;
	/* Pad to 4 bytes not touching caller's buffer */
	if (remainder) {
		memcpy(&tmp, p, remainder);
		write_slot(tmp);
	}

	host_gen_interrupt();

	/* Make sure nothing bad happened during transmission */
	if (!cse_ready())
		return 0;

	return pend_len;
}

enum cse_tx_rx_status
heci_send(const void *msg, size_t len, uint8_t host_addr, uint8_t client_addr)
{
	uint8_t retry;
	uint32_t csr, hdr;
	size_t sent, remaining, cb_size, max_length;
	const uint8_t *p;

	if (!msg || !len)
		return CSE_TX_ERR_INPUT;

	clear_int();

	for (retry = 0; retry < MAX_HECI_MESSAGE_RETRY_COUNT; retry++) {
		p = msg;

		if (!wait_heci_ready()) {
			printk(BIOS_ERR, "HECI: not ready\n");
			continue;
		}

		csr = read_host_csr();
		cb_size = ((csr & CSR_CBD) >> CSR_CBD_START) * SLOT_SIZE;
		/*
		 * Reserve one slot for the header. Limit max message
		 * length by 9 bits that are available in the header.
		 */
		max_length = MIN(cb_size, (1 << MEI_HDR_LENGTH_SIZE) - 1)
				- SLOT_SIZE;
		remaining = len;

		/*
		 * Fragment the message into smaller messages not exceeding
		 * useful circular buffer length. Mark last message complete.
		 */
		do {
			hdr = MIN(max_length, remaining)
				<< MEI_HDR_LENGTH_START;
			hdr |= client_addr << MEI_HDR_CSE_ADDR_START;
			hdr |= host_addr << MEI_HDR_HOST_ADDR_START;
			hdr |= (MIN(max_length, remaining) == remaining) ?
						MEI_HDR_IS_COMPLETE : 0;
			sent = send_one_message(hdr, p);
			p += sent;
			remaining -= sent;
		} while (remaining > 0 && sent != 0);

		if (!remaining)
			return CSE_TX_RX_SUCCESS;
	}

	printk(BIOS_DEBUG, "HECI: Trigger HECI reset\n");
	heci_reset();
	return CSE_TX_ERR_CSE_NOT_READY;
}

static enum cse_tx_rx_status
recv_one_message(uint32_t *hdr, void *buff, size_t maxlen, size_t *recv_len)
{
	uint32_t reg, *p = buff;
	size_t recv_slots, remainder, i;

	/* first get the header */
	if (!wait_read_slots(1))
		return CSE_RX_ERR_TIMEOUT;

	*hdr = read_slot();
	*recv_len = hdr_get_length(*hdr);

	if (!*recv_len)
		printk(BIOS_WARNING, "HECI: message is zero-sized\n");

	recv_slots = bytes_to_slots(*recv_len);

	i = 0;
	if (*recv_len > maxlen) {
		printk(BIOS_ERR, "HECI: response is too big\n");
		return CSE_RX_ERR_RESP_LEN_MISMATCH;
	}

	/* wait for the rest of messages to arrive */
	wait_read_slots(recv_slots);

	/* fetch whole slots first */
	while (i < ALIGN_DOWN(*recv_len, SLOT_SIZE)) {
		*p++ = read_slot();
		i += SLOT_SIZE;
	}

	/*
	 * If ME is not ready, something went wrong and
	 * we received junk
	 */
	if (!cse_ready())
		return CSE_RX_ERR_CSE_NOT_READY;

	remainder = *recv_len % SLOT_SIZE;

	if (remainder) {
		reg = read_slot();
		memcpy(p, &reg, remainder);
	}
	return CSE_TX_RX_SUCCESS;
}

enum cse_tx_rx_status heci_receive(void *buff, size_t *maxlen)
{
	uint8_t retry;
	size_t left, received;
	uint32_t hdr = 0;
	uint8_t *p;
	enum cse_tx_rx_status ret = CSE_RX_ERR_TIMEOUT;

	if (!buff || !maxlen || !*maxlen)
		return CSE_RX_ERR_INPUT;

	clear_int();

	for (retry = 0; retry < MAX_HECI_MESSAGE_RETRY_COUNT; retry++) {
		p = buff;
		left = *maxlen;

		if (!wait_heci_ready()) {
			printk(BIOS_ERR, "HECI: not ready\n");
			continue;
		}

		/*
		 * Receive multiple packets until we meet one marked
		 * complete or we run out of space in caller-provided buffer.
		 */
		do {
			ret = recv_one_message(&hdr, p, left, &received);
			if (ret) {
				printk(BIOS_ERR, "HECI: Failed to receive!\n");
				goto CSE_RX_ERR_HANDLE;
			}
			left -= received;
			p += received;
			/* If we read out everything ping to send more */
			if (!(hdr & MEI_HDR_IS_COMPLETE) && !cse_filled_slots())
				host_gen_interrupt();
		} while (received && !(hdr & MEI_HDR_IS_COMPLETE) && left > 0);

		if ((hdr & MEI_HDR_IS_COMPLETE) && received) {
			*maxlen = p - (uint8_t *)buff;
			return CSE_TX_RX_SUCCESS;
		}
	}

CSE_RX_ERR_HANDLE:
	printk(BIOS_DEBUG, "HECI: Trigger HECI Reset\n");
	heci_reset();
	return CSE_RX_ERR_CSE_NOT_READY;
}

enum cse_tx_rx_status heci_send_receive(const void *snd_msg, size_t snd_sz, void *rcv_msg,
					size_t *rcv_sz, uint8_t cse_addr)
{
	enum cse_tx_rx_status ret;

	ret = heci_send(snd_msg, snd_sz, BIOS_HOST_ADDR, cse_addr);
	if (ret) {
		printk(BIOS_ERR, "HECI: send Failed\n");
		return ret;
	}

	if (rcv_msg != NULL) {
		ret = heci_receive(rcv_msg, rcv_sz);
		if (ret) {
			printk(BIOS_ERR, "HECI: receive Failed\n");
			return ret;
		}
	}
	return ret;
}

/*
 * Attempt to reset the device. This is useful when host and ME are out
 * of sync during transmission or ME didn't understand the message.
 */
int heci_reset(void)
{
	uint32_t csr;

	/* Clear post code to prevent eventlog entry from unknown code. */
	post_code(POST_CODE_ZERO);

	/* Send reset request */
	csr = read_host_csr();
	csr |= (CSR_RESET | CSR_IG);
	write_host_csr(csr);

	if (wait_heci_ready()) {
		/* Device is back on its imaginary feet, clear reset */
		cse_set_host_ready();
		return 1;
	}

	printk(BIOS_CRIT, "HECI: reset failed\n");

	return 0;
}

bool is_cse_devfn_visible(pcidev_t dev)
{
	int slot = PCI_SLOT(dev);
	int func = PCI_FUNC(dev);

	if (pci_read_config16(dev, REG_VENDOR_ID) == 0xFFFF) {
		printk(BIOS_WARNING, "HECI: CSE device %02x.%01x is hidden\n", slot, func);
		return false;
	}

	return true;
}

bool is_cse_enabled(void)
{
	return is_cse_devfn_visible(PCH_DEVFN_CSE);
}

uint32_t me_read_config32(int offset)
{
	return pci_read_config32(PCH_DEV_CSE, offset);
}

static bool cse_is_global_reset_allowed(void)
{
	/*
	 * Allow sending GLOBAL_RESET command only if:
	 *  - CSE's current working state is Normal and current operation mode is Normal.
	 *  - (or) CSE's current working state is normal and current operation mode can
	 *    be Soft Temp Disable or Security Override Mode if CSE's Firmware SKU is
	 *    Lite.
	 */
	if (!cse_is_hfs1_cws_normal())
		return false;

	if (cse_is_hfs1_com_normal())
		return true;

	if (cse_is_hfs3_fw_sku_lite()) {
		if (cse_is_hfs1_com_soft_temp_disable() || cse_is_hfs1_com_secover_mei_msg())
			return true;
	}
	return false;
}

/*
 * Sends GLOBAL_RESET_REQ cmd to CSE with reset type GLOBAL_RESET.
 * Returns 0 on failure and 1 on success.
 */
static int cse_request_reset(enum rst_req_type rst_type)
{
	int status;
	struct mkhi_hdr reply;
	struct reset_message {
		struct mkhi_hdr hdr;
		uint8_t req_origin;
		uint8_t reset_type;
	} __packed;
	struct reset_message msg = {
		.hdr = {
			.group_id = MKHI_GROUP_ID_CBM,
			.command = MKHI_CBM_GLOBAL_RESET_REQ,
		},
		.req_origin = GR_ORIGIN_BIOS_POST,
		.reset_type = rst_type
	};
	size_t reply_size;

	printk(BIOS_DEBUG, "HECI: Global Reset(Type:%d) Command\n", rst_type);

	if (!(rst_type == GLOBAL_RESET || rst_type == CSE_RESET_ONLY)) {
		printk(BIOS_ERR, "HECI: Unsupported reset type is requested\n");
		return 0;
	}

	if (!cse_is_global_reset_allowed() || !is_cse_enabled()) {
		printk(BIOS_ERR, "HECI: CSE does not meet required prerequisites\n");
		return 0;
	}

	heci_reset();

	reply_size = sizeof(reply);
	memset(&reply, 0, reply_size);

	if (rst_type == CSE_RESET_ONLY)
		status = heci_send(&msg, sizeof(msg), BIOS_HOST_ADDR, HECI_MKHI_ADDR);
	else
		status = heci_send_receive(&msg, sizeof(msg), &reply, &reply_size,
									HECI_MKHI_ADDR);

	printk(BIOS_DEBUG, "HECI: Global Reset %s!\n", !status ? "success" : "failure");
	return status;
}

int cse_request_global_reset(void)
{
	return cse_request_reset(GLOBAL_RESET);
}

static bool cse_is_hmrfpo_enable_allowed(void)
{
	/*
	 * Allow sending HMRFPO ENABLE command only if:
	 *  - CSE's current working state is Normal and current operation mode is Normal
	 *  - (or) cse's current working state is normal and current operation mode is
	 *    Soft Temp Disable if CSE's Firmware SKU is Lite
	 */
	if (!cse_is_hfs1_cws_normal())
		return false;

	if (cse_is_hfs1_com_normal())
		return true;

	if (cse_is_hfs3_fw_sku_lite() && cse_is_hfs1_com_soft_temp_disable())
		return true;

	return false;
}

/* Sends HMRFPO Enable command to CSE */
enum cb_err cse_hmrfpo_enable(void)
{
	struct hmrfpo_enable_msg {
		struct mkhi_hdr hdr;
		uint32_t nonce[2];
	} __packed;

	/* HMRFPO Enable message */
	struct hmrfpo_enable_msg msg = {
		.hdr = {
			.group_id = MKHI_GROUP_ID_HMRFPO,
			.command = MKHI_HMRFPO_ENABLE,
		},
		.nonce = {0},
	};

	/* HMRFPO Enable response */
	struct hmrfpo_enable_resp {
		struct mkhi_hdr hdr;
		/* Base addr for factory data area, not relevant for client SKUs */
		uint32_t fct_base;
		/* Length of factory data area, not relevant for client SKUs */
		uint32_t fct_limit;
		uint8_t status;
		uint8_t reserved[3];
	} __packed;

	struct hmrfpo_enable_resp resp;
	size_t resp_size = sizeof(struct hmrfpo_enable_resp);

	if (cse_is_hfs1_com_secover_mei_msg()) {
		printk(BIOS_DEBUG, "HECI: CSE is already in security override mode, "
			       "skip sending HMRFPO_ENABLE command to CSE\n");
		return CB_SUCCESS;
	}

	printk(BIOS_DEBUG, "HECI: Send HMRFPO Enable Command\n");

	if (!cse_is_hmrfpo_enable_allowed()) {
		printk(BIOS_ERR, "HECI: CSE does not meet required prerequisites\n");
		return CB_ERR;
	}

	if (heci_send_receive(&msg, sizeof(struct hmrfpo_enable_msg),
				&resp, &resp_size, HECI_MKHI_ADDR))
		return CB_ERR;

	if (resp.hdr.result) {
		printk(BIOS_ERR, "HECI: Resp Failed:%d\n", resp.hdr.result);
		return CB_ERR;
	}

	if (resp.status) {
		printk(BIOS_ERR, "HECI: HMRFPO_Enable Failed (resp status: %d)\n", resp.status);
		return CB_ERR;
	}

	return CB_SUCCESS;
}

/*
 * Sends HMRFPO Get Status command to CSE to get the HMRFPO status.
 * The status can be DISABLED/LOCKED/ENABLED
 */
int cse_hmrfpo_get_status(void)
{
	struct hmrfpo_get_status_msg {
		struct mkhi_hdr hdr;
	} __packed;

	struct hmrfpo_get_status_resp {
		struct mkhi_hdr hdr;
		uint8_t status;
		uint8_t reserved[3];
	} __packed;

	struct hmrfpo_get_status_msg msg = {
		.hdr = {
			.group_id = MKHI_GROUP_ID_HMRFPO,
			.command = MKHI_HMRFPO_GET_STATUS,
		},
	};
	struct hmrfpo_get_status_resp resp;
	size_t resp_size = sizeof(struct hmrfpo_get_status_resp);

	printk(BIOS_INFO, "HECI: Sending Get HMRFPO Status Command\n");

	if (!cse_is_hfs1_cws_normal()) {
		printk(BIOS_ERR, "HECI: CSE's current working state is not Normal\n");
		return -1;
	}

	if (heci_send_receive(&msg, sizeof(struct hmrfpo_get_status_msg),
				&resp, &resp_size, HECI_MKHI_ADDR)) {
		printk(BIOS_ERR, "HECI: HMRFPO send/receive fail\n");
		return -1;
	}

	if (resp.hdr.result) {
		printk(BIOS_ERR, "HECI: HMRFPO Resp Failed:%d\n",
				resp.hdr.result);
		return -1;
	}

	return resp.status;
}

void cse_trigger_vboot_recovery(enum csme_failure_reason reason)
{
	printk(BIOS_DEBUG, "cse: CSE status registers: HFSTS1: 0x%x, HFSTS2: 0x%x "
	       "HFSTS3: 0x%x\n", me_read_config32(PCI_ME_HFSTS1),
	       me_read_config32(PCI_ME_HFSTS2), me_read_config32(PCI_ME_HFSTS3));

	{
		struct vb2_context *ctx = vboot_get_context();
		vb2api_fail(ctx, VB2_RECOVERY_INTEL_CSE_LITE_SKU, reason);
		vb2ex_commit_data(ctx);
		reboot();
	}

	die("cse: Failed to trigger recovery mode(recovery subcode:%d)\n", reason);
}

static bool disable_cse_idle(pcidev_t dev)
{
	struct stopwatch sw;
	uint32_t dev_idle_ctrl = read_bar(dev, MMIO_CSE_DEVIDLE);
	dev_idle_ctrl &= ~CSE_DEV_IDLE;
	write_bar(dev, MMIO_CSE_DEVIDLE, dev_idle_ctrl);

	stopwatch_init_usecs_expire(&sw, HECI_CIP_TIMEOUT_US);
	do {
		dev_idle_ctrl = read_bar(dev, MMIO_CSE_DEVIDLE);
		if ((dev_idle_ctrl & CSE_DEV_CIP) == CSE_DEV_CIP)
			return true;
		udelay(HECI_DELAY_US);
	} while (!stopwatch_expired(&sw));

	return false;
}

static void enable_cse_idle(pcidev_t dev)
{
	uint32_t dev_idle_ctrl = read_bar(dev, MMIO_CSE_DEVIDLE);
	dev_idle_ctrl |= CSE_DEV_IDLE;
	write_bar(dev, MMIO_CSE_DEVIDLE, dev_idle_ctrl);
}

enum cse_device_state get_cse_device_state(pcidev_t dev)
{
	uint32_t dev_idle_ctrl = read_bar(dev, MMIO_CSE_DEVIDLE);
	if ((dev_idle_ctrl & CSE_DEV_IDLE) == CSE_DEV_IDLE)
		return DEV_IDLE;

	return DEV_ACTIVE;
}

static enum cse_device_state ensure_cse_active(pcidev_t dev)
{
	uint32_t cmd;
	if (!disable_cse_idle(dev))
		return DEV_IDLE;
	cmd = pci_read_config32(dev, REG_COMMAND);
	cmd |= (REG_COMMAND_MEM | REG_COMMAND_BM);
	pci_write_config32(dev, REG_COMMAND, cmd);

	return DEV_ACTIVE;
}

static void ensure_cse_idle(pcidev_t dev)
{
	uint32_t cmd;
	enable_cse_idle(dev);

	cmd = pci_read_config32(dev, REG_COMMAND);
	cmd &= ~(REG_COMMAND_MEM | REG_COMMAND_BM);
	pci_write_config32(dev, REG_COMMAND, cmd);
}

bool set_cse_device_state(pcidev_t dev, enum cse_device_state requested_state)
{
	enum cse_device_state current_state = get_cse_device_state(dev);

	if (current_state == requested_state)
		return true;

	if (requested_state == DEV_ACTIVE)
		return ensure_cse_active(dev) == requested_state;
	else
		ensure_cse_idle(dev);

	return true;
}

void cse_set_to_d0i3(void)
{
	if (!is_cse_devfn_visible(PCH_DEVFN_CSE))
		return;

	set_cse_device_state(PCH_DEVFN_CSE, DEV_IDLE);
}

/* Function to set D0I3 for all HECI devices */
void heci_set_to_d0i3(void)
{
	for (int i = 0; i < CONFIG_MAX_HECI_DEVICES; i++) {
		pcidev_t dev = PCI_DEV(0, PCH_DEV_SLOT_CSE, i);
		if (!is_cse_devfn_visible(dev))
			continue;

		set_cse_device_state(dev, DEV_IDLE);
	}
}

void cse_control_global_reset_lock(void)
{
	/*
	 * As per ME BWG recommendation the BIOS should not lock down CF9GR bit during
	 * manufacturing and re-manufacturing environment if HFSTS1 [4] is set. Note:
	 * this recommendation is not applicable for CSE-Lite SKUs where BIOS should set
	 * CF9LOCK bit irrespectively.
	 *
	 * Other than that, make sure payload/OS can't trigger global reset.
	 *
	 * BIOS must also ensure that CF9GR is cleared and locked (Bit31 of ETR3)
	 * prior to transferring control to the OS.
	 */
	if (CONFIG(SOC_INTEL_CSE_LITE_SKU) || cse_is_hfs1_spi_protected())
		pmc_global_reset_disable_and_lock();
	else
		pmc_global_reset_enable(false);
}

enum cb_err cse_get_fw_feature_state(uint32_t *feature_state)
{
	struct fw_feature_state_msg {
		struct mkhi_hdr hdr;
		uint32_t rule_id;
	} __packed;

	/* Get Firmware Feature State message */
	struct fw_feature_state_msg msg = {
		.hdr = {
			.group_id = MKHI_GROUP_ID_FWCAPS,
			.command = MKHI_FWCAPS_GET_FW_FEATURE_STATE,
		},
		.rule_id = ME_FEATURE_STATE_RULE_ID
	};

	/* Get Firmware Feature State response */
	struct fw_feature_state_resp {
		struct mkhi_hdr hdr;
		uint32_t rule_id;
		uint8_t rule_len;
		uint32_t fw_runtime_status;
	} __packed;

	struct fw_feature_state_resp resp;
	size_t resp_size = sizeof(struct fw_feature_state_resp);

	/* Ignore if CSE is disabled or input buffer is invalid */
	if (!is_cse_enabled() || !feature_state)
		return CB_ERR;

	/*
	 * Prerequisites:
	 * 1) HFSTS1 Current Working State is Normal
	 * 2) HFSTS1 Current Operation Mode is Normal
	 * 3) It's after DRAM INIT DONE message (taken care of by calling it
	 *    during ramstage)
	 */
	if (!cse_is_hfs1_cws_normal() || !cse_is_hfs1_com_normal() /*|| !ENV_RAMSTAGE */)
		return CB_ERR;

	printk(BIOS_DEBUG, "HECI: Send GET FW FEATURE STATE Command\n");

	if (heci_send_receive(&msg, sizeof(struct fw_feature_state_msg),
				&resp, &resp_size, HECI_MKHI_ADDR))
		return CB_ERR;

	if (resp.hdr.result) {
		printk(BIOS_ERR, "HECI: Resp Failed:%d\n", resp.hdr.result);
		return CB_ERR;
	}

	if (resp.rule_len != sizeof(resp.fw_runtime_status)) {
		printk(BIOS_ERR, "HECI: GET FW FEATURE STATE has invalid rule data length\n");
		return CB_ERR;
	}

	*feature_state = resp.fw_runtime_status;

	return CB_SUCCESS;
}

/*
 * `cse_final_ready_to_boot` function is native implementation of equivalent events
 * performed by FSP NotifyPhase(Ready To Boot) API invocations.
 *
 * Operations are:
 * 1. Perform global reset lock.
 * 2. Put HECI1 to D0i3 and disable the HECI1 if the user selects
 *      SOC_INTEL_CSE_DISABLE_HECI1_AT_PRE_BOOT config or CSE HFSTS1 Operation Mode is
 *      `Software Temporary Disable`.
 */
static void cse_final_ready_to_boot(void)
{
	cse_control_global_reset_lock();

	if (CONFIG(SOC_INTEL_CSE_DISABLE_HECI1_AT_PRE_BOOT) || cse_is_hfs1_com_soft_temp_disable()) {
		cse_set_to_d0i3();
		heci1_disable();
	}
}

/*
 * `cse_final_end_of_firmware` function is native implementation of equivalent events
 * performed by FSP NotifyPhase(End of Firmware) API invocations.
 *
 * Operations are:
 * 1. Set D0I3 for all HECI devices.
 */
static void cse_final_end_of_firmware(void)
{
	heci_set_to_d0i3();
}

/* This function to perform essential post EOP cse related operations.*/
void cse_late_finalize(void)
{
	cse_final_ready_to_boot();
	cse_final_end_of_firmware();
}

void pmc_global_reset_disable_and_lock(void)
{
	uint32_t *etr = (void *)(PCH_PWRM_BASE_ADDRESS + ETR);
	uint32_t reg;

	reg = read32(etr);
	reg = (reg & ~CF9_GLB_RST) | CF9_LOCK;
	write32(etr, reg);
}

void pmc_global_reset_enable(bool enable)
{
	uint32_t *etr = (void *)(PCH_PWRM_BASE_ADDRESS + ETR);
	uint32_t reg;

	reg = read32(etr);
	reg = enable ? reg | CF9_GLB_RST : reg & ~CF9_GLB_RST;
	write32(etr, reg);
}

static void do_full_reset(void)
{
	// TODO dcache_clean_all();
	reboot();
}

void do_global_reset(void)
{
	/* Ask CSE to do the global reset */
	if (cse_request_global_reset())
		return;

	/* global reset if CSE fail to reset */
	pmc_global_reset_enable(1);
	do_full_reset();
}
