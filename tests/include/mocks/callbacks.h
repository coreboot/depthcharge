/* SPDX-License-Identifier: GPL-2.0 */

#define FIRMWARE_LOG_BUFFER_LEN 16

extern uint32_t mock_time_ms;

/*
 * Variables for vb2ex_get_firmware_log. See comments in vb2ex_get_firmware_log
 * for more information.
 */
extern char firmware_log_buf[FIRMWARE_LOG_BUFFER_LEN];
extern int firmware_log_snapshots_count;
