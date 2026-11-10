// SPDX-License-Identifier: GPL-2.0
/* Copyright 2026 Google LLC. */

#include <coreboot_tables.h>
#include <libpayload.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <tests/test.h>

#include "drivers/bus/usb/intel_tcss.c"

#define TEST_USB3_PORT		1
#define TEST_USB2_PORT		2

struct sysinfo_t lib_sysinfo;

static struct {
	struct type_c_info info;
	struct type_c_port_info ports[1];
} test_type_c = {
	.info = { .port_count = 1 },
	.ports = {
		[0] = {
			.usb2_port_number = TEST_USB2_PORT,
			.usb3_port_number = TEST_USB3_PORT,
			.sbu_orientation = TYPEC_ORIENTATION_NONE,
			.data_orientation = TYPEC_ORIENTATION_NONE,
		},
	},
};

_Static_assert(offsetof(__typeof__(test_type_c), ports) ==
	       offsetof(struct type_c_info, port_info),
	       "test_type_c layout must match struct type_c_info");

static uint8_t mock_mux_state;
static int mock_ufp;
static int mock_dbg_acc;
static bool mock_port_connected;
static int mock_ipc_count;
static uint32_t mock_last_req;

void usb_register_init_callback(UsbCallbackData *to_register)
{
}

void usb_register_poll_callback(UsbCallbackData *to_register)
{
}

int cros_ec_get_usb_pd_mux_info(int port, uint8_t *mux_state)
{
	assert_int_equal(port, 0);
	*mux_state = mock_mux_state;
	return 0;
}

int cros_ec_get_usb_pd_control(int port, int *ufp, int *dbg_acc)
{
	assert_int_equal(port, 0);
	*ufp = mock_ufp;
	*dbg_acc = mock_dbg_acc;
	return 0;
}

bool is_port_connected(int port, int usb2)
{
	assert_int_equal(port, TEST_USB3_PORT - 1);
	assert_int_equal(usb2, TEST_USB2_PORT - 1);
	return mock_port_connected;
}

int pmc_send_cmd(uint32_t cmd, const struct pmc_ipc_buffer *wbuf,
		 struct pmc_ipc_buffer *rbuf)
{
	assert_int_equal(cmd, pmc_make_ipc_cmd(PMC_IPC_USBC_CMD_ID,
					       PMC_IPC_USBC_SUBCMD_ID,
					       PMC_IPC_CONN_DISC_REQ_SIZE));
	assert_non_null(wbuf);
	assert_non_null(rbuf);
	mock_ipc_count++;
	mock_last_req = wbuf->buf[0];
	rbuf->buf[0] = 0;
	return 0;
}

static void poll_once(void)
{
	mock_ipc_count = 0;
	mock_last_req = 0;
	assert_int_equal(update_all_tcss_ports_states(), 0);
}

#define ASSERT_CONN_IPC(expected_ufp) do { \
	assert_int_equal(mock_ipc_count, 1); \
	assert_int_equal(GET_TCSS_CD_FIELD(USAGE, mock_last_req), \
			 TCSS_CONN_REQ_RES); \
	assert_int_equal(GET_TCSS_CD_FIELD(USB3, mock_last_req), \
			 TEST_USB3_PORT); \
	assert_int_equal(GET_TCSS_CD_FIELD(USB2, mock_last_req), \
			 TEST_USB2_PORT); \
	assert_int_equal(GET_TCSS_CD_FIELD(UFP, mock_last_req), \
			 (expected_ufp)); \
} while (0)

#define ASSERT_DISC_IPC() do { \
	assert_int_equal(mock_ipc_count, 1); \
	assert_int_equal(GET_TCSS_CD_FIELD(USAGE, mock_last_req), \
			 TCSS_DISC_REQ_RES); \
	assert_int_equal(GET_TCSS_CD_FIELD(USB3, mock_last_req), \
			 TEST_USB3_PORT); \
	assert_int_equal(GET_TCSS_CD_FIELD(USB2, mock_last_req), \
			 TEST_USB2_PORT); \
} while (0)

#define ASSERT_NO_IPC() assert_int_equal(mock_ipc_count, 0)

static int setup(void **state)
{
	lib_sysinfo.type_c_info = (uintptr_t)&test_type_c.info;

	/* Force a clean disconnect so static port state resets between tests. */
	mock_mux_state = 0;
	mock_port_connected = true;
	poll_once();
	ASSERT_DISC_IPC();

	mock_port_connected = false;
	mock_ufp = 0;
	mock_dbg_acc = 0;
	mock_ipc_count = 0;
	mock_last_req = 0;
	return 0;
}

static void test_dr_swap_iom_disconnect(void **state)
{
	/* 1. Connected with ufp=1 -> CONN(ufp=1). */
	mock_mux_state = USB_PD_MUX_USB_ENABLED;
	mock_ufp = 1;
	mock_port_connected = false;
	poll_once();
	ASSERT_CONN_IPC(1);

	/* Avoid back-to-back CONN before IOM status reflects connection. */
	poll_once();
	ASSERT_NO_IPC();

	/* 2. DR_Swap changes ufp to 0 -> DISC first. */
	mock_port_connected = true;
	mock_ufp = 0;
	poll_once();
	ASSERT_DISC_IPC();

	/* 3. IOM reports port disconnected -> CONN(ufp=0). */
	mock_port_connected = false;
	poll_once();
	ASSERT_CONN_IPC(0);

	/* 4. Steady state -> no redundant IPCs. */
	poll_once();
	ASSERT_NO_IPC();
	mock_port_connected = true;
	poll_once();
	ASSERT_NO_IPC();
}

static void test_dr_swap_port_stays_connected_after_disc(void **state)
{
	/* 1. Connected with ufp=1 -> CONN(ufp=1). */
	mock_mux_state = USB_PD_MUX_USB_ENABLED;
	mock_ufp = 1;
	mock_port_connected = false;
	poll_once();
	ASSERT_CONN_IPC(1);

	/* 2. DR_Swap changes ufp to 0 -> DISC. */
	mock_port_connected = true;
	mock_ufp = 0;
	poll_once();
	ASSERT_DISC_IPC();

	/*
	 * 3. On Meteor Lake, PCH USB2 PORTSC CCS stays set across TCSS DISC,
	 * so is_port_connected() stays true. Next poll must send CONN(ufp=0)
	 * rather than looping on DISC or skipping reconnect.
	 */
	poll_once();
	ASSERT_CONN_IPC(0);

	/* 4. Nothing changes -> no IPC. */
	poll_once();
	ASSERT_NO_IPC();
}

static void test_dr_swap_from_boot_connected_port(void **state)
{
	/* 1. Port already connected by coreboot with ufp=1 -> no IPC. */
	mock_mux_state = USB_PD_MUX_USB_ENABLED;
	mock_ufp = 1;
	mock_port_connected = true;
	poll_once();
	ASSERT_NO_IPC();

	/* 2. DR_Swap flips ufp to 0 -> DISC, then CONN(ufp=0), then idle. */
	mock_ufp = 0;
	poll_once();
	ASSERT_DISC_IPC();

	poll_once();
	ASSERT_CONN_IPC(0);

	poll_once();
	ASSERT_NO_IPC();
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_dr_swap_iom_disconnect, setup),
		cmocka_unit_test_setup(
			test_dr_swap_port_stays_connected_after_disc, setup),
		cmocka_unit_test_setup(
			test_dr_swap_from_boot_connected_port, setup),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
