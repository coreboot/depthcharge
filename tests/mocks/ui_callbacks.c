// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/ui/common.h>
#include <mocks/callbacks.h>
#include <vboot_api.h>
#include <vb2_api.h>

uint32_t mock_time_ms;

char firmware_log_buf[FIRMWARE_LOG_BUFFER_LEN];
int firmware_log_snapshots_count;

uint32_t vb2ex_mtime(void)
{
	return mock_time_ms;
}

void vb2ex_msleep(uint32_t msec)
{
	mock_time_ms += msec;
}

uint32_t vb2ex_get_locale_count(void)
{
	return mock_type(uint32_t);
}

const char *vb2ex_get_debug_info(struct vb2_context *ctx)
{
	return "mock debug info";
}

int vb2ex_physical_presence_pressed(void)
{
	return mock();
}

/*
 * This mock function uses firmware_log_snapshots_count to decide what will be
 * written into the buffer and returned. Each time the reset parameter is
 * non-zero, the snapshot count will increase by one.
 */
const char *vb2ex_get_firmware_log(int reset)
{
	if (reset)
		firmware_log_snapshots_count++;
	snprintf(firmware_log_buf, sizeof(firmware_log_buf), "%d",
		 firmware_log_snapshots_count);
	return firmware_log_buf;
}

uint32_t vb2ex_prepare_log_screen(enum vb2_screen screen, uint32_t locale_id,
				  const char *str)
{
	check_expected(str);
	return mock_type(uint32_t);
}

vb2_error_t vb2ex_run_altfw(uint32_t altfw_id)
{
	vb2_error_t rv;
	check_expected(altfw_id);
	rv = mock_type(vb2_error_t);
	if (rv == VB2_SUCCESS) {
		mock_assert(0, __func__, __FILE__, __LINE__);
		return VB2_SUCCESS;
	} else {
		return rv;
	}
}

uint32_t vb2ex_get_altfw_count(void)
{
	return mock_type(uint32_t);
}

/*
 * This mock function requires a mock value as expected called time, and has two
 * check_expected calls for the two parameters. The expected_time checking is
 * done by ASSERT_TIME_RANGE.
 */
void vb2ex_beep(uint32_t msec, uint32_t frequency)
{
	uint32_t expected_time = mock_type(uint32_t);

	check_expected(msec);
	check_expected(frequency);
	if (expected_time != MOCK_IGNORE)
		ASSERT_TIME_RANGE(mock_time_ms, expected_time);

	mock_time_ms += msec;
}

vb2_error_t vb2ex_diag_get_storage_test_log(const char **log)
{
	*log = "mock";
	return mock_type(vb2_error_t);
}
