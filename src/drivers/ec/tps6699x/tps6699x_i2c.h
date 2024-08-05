/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2024 Google LLC.  */

#ifndef __DRIVERS_EC_TPS6699X_TPS6699X_I2C_H_
#define __DRIVERS_EC_TPS6699X_TPS6699X_I2C_H_

#include "arch/types.h"

#include "tps6699x.h"

#define TPS6699X_CMD_SIZE 4
#define TPS6699X_MAX_NUM_PORTS 2
#define TPS6699X_METADATA_OFFSET 0x4
#define TPS6699X_METADATA_LENGTH 0x8
#define TPS6699X_DEFAULT_PORT 1
#define TPS6699X_DATA_REGION_OFFSET 0x80C
#define TPS6699X_DATA_BLOCK_SIZE 0x4000
#define TPS6699X_HEADER_BLOCK_OFFSET 0xC
#define TPS6699X_HEADER_BLOCK_LENGTH 0x800
#define TPS6699X_FW_SIZE_OFFSET 0x4F8
#define TPS6699X_DO_COPY 0xAC
#define TPS6699X_DO_SWITCH_BANK 0xAC
#define TPS6699X_HASH_FW_VERSION_INDX 1
#define TPS6699X_MAX_I2C_BUFFER_LEN                                            \
	256 // max len for host cmd (used for I2C tunneling)
#define TPS6699X_DATA_CHUNK_LEN                                                \
	64 // break up into chunks of multiples of 64 bytes
#define TPS6699X_GAID_MAGIC_VALUE 0xAC

enum tps6699x_4cc_tasks {
	/* Invalid command (!CMD) */
	COMMAND_TASK_NO_COMMAND = 0x444d4321,
	/* Cold reset request */
	COMMAND_TASK_GAID = 0x44494147,
	/* TFU commands */
	COMMAND_TASK_TFUS = 0x73554654,
	COMMAND_TASK_TFUC = 0x63554654,
	COMMAND_TASK_TFUD = 0x64554654,
	COMMAND_TASK_TFUE = 0x65554654,
	COMMAND_TASK_TFUI = 0x69554654,
	COMMAND_TASK_TFUQ = 0x71554654,
};

enum tps6699x_i2c_registers {
	TPSREG_VENDOR_ID = 0x00,
	TPSREG_DEVICE_ID = 0x01,
	TPSREG_MODE = 0x03,
	TPSREG_CMD1 = 0x08,
	TPSREG_DATA1 = 0x09,
	TPSREG_VERSION = 0x0F,
	TPSREG_CMD2 = 0x10,
	TPSREG_DATA2 = 0x11,
	TPSREG_IRQ_EVENT1 = 0x14,
	TPSREG_IRQ_EVENT2 = 0x15,
	TPSREG_IRQ_MASK1 = 0x16,
	TPSREG_IRQ_MASK2 = 0x17,
	TPSREG_IRQ_CLEAR1 = 0x18,
	TPSREG_IRQ_CLEAR2 = 0x19,
	TPSREG_BOOT_FLAGS = 0x2D,
	TPSREG_DEVICE_INFO = 0x2F,
};

struct tfu_initiate {
	uint16_t num_blocks;
	uint16_t data_block_size;
	uint16_t timeout_secs;
	uint16_t broadcast_address;
} __attribute__((__packed__));

struct tfu_download {
	uint16_t num_blocks;
	uint16_t data_block_size;
	uint16_t timeout_secs;
	uint16_t broadcast_address;
} __attribute__((__packed__));

struct tfu_query {
	uint8_t bank;
	uint8_t cmd;
} __attribute__((__packed__));

struct tfu_complete {
	uint8_t do_switch;
	uint8_t do_copy;
} __attribute__((__packed__));

struct tps6699x_tfu_query_output {
	uint8_t result;
	uint8_t tfu_state;
	uint8_t complete_image;
	uint16_t blocks_written;
	uint8_t header_block_status;
	uint8_t per_block_status[12];
	uint8_t num_header_bytes_written;
	uint8_t num_data_bytes_written;
	uint8_t num_appconfig_bytes_written;
} __attribute__((__packed__));

/**
 * @brief 4.9 Data Register for CMD1 (Offset = 0x09)
 * @brief 4.13 Data Register for CMD2 (Offset = 0x11)
 *
 * Data register for the primary command interface.
 */
union reg_data {
	struct {
		uint8_t _intern_reg : 8;
		uint8_t _intern_len : 8;
		uint8_t data[64];
	} __packed;
	uint8_t raw_value[66];
};

union reg_mode {
	struct {
		uint8_t _intern_reg : 8;
		uint8_t _intern_len : 8;

		uint8_t data[4];
	} __packed;
	uint8_t raw_value[6];
};

union reg_command {
	struct {
		uint8_t _intern_reg : 8;
		uint8_t _intern_len : 8;

		uint32_t command : 32;
	} __packed;
	uint8_t raw_value[6];
};

union reg_version {
	struct {
		uint8_t _intern_reg : 8;
		uint8_t _intern_len : 8;

		uint32_t version : 32;
	} __packed;
	uint8_t raw_value[6];
};

union gaid_params_t {
	struct {
		uint8_t switch_banks;
		uint8_t copy_banks;
	} __packed;
	uint8_t raw[2];
};

/**
 * @brief Write to the PDC command register to initiate the given 4CC command
 *
 * @param me Chip object
 * @param cmd 4CC command
 * @return int 0 on success
 */
int tps6699x_command_reg_write(Tps6699x *me, union reg_command *cmd);

/**
 * @brief Query the PDC command register to check command progress
 *
 * @param me Chip object
 * @param cmd Result output buffer
 * @return int 0 on success
 */
int tps6699x_command_reg_read(Tps6699x *me, union reg_command *cmd);

/**
 * @brief Write input data for a 4CC command
 *
 * @param me Chip object
 * @param data Input data buffer
 * @return int 0 on success
 */
int tps6699x_data_reg_write(Tps6699x *me, union reg_data *data);

/**
 * @brief Read response data from a 4CC command
 *
 * @param me Chip object
 * @param data Output buffer for response
 * @return int 0 on success
 */
int tps6699x_data_reg_read(Tps6699x *me, union reg_data *data);

/**
 * @brief Stream data payload to the PDCs
 *
 * @param me Chip object
 * @param broadcast_address Broadcast I2C target addresses device(s) are
 * listening on
 * @param buf Buffer to send
 * @param buf_len Number of bytes to send from \p buf
 * @return int 0 on success
 */
int tps6699x_stream_data(Tps6699x *me, const uint8_t broadcast_address,
			 const uint8_t *buf, size_t buf_len);

/**
 * @brief Query the PDC mode register
 *
 * @param me Chip object
 * @param mode Result output buffer
 * @return int 0 on success
 */
int tps6699x_mode_read(Tps6699x *me, union reg_mode *mode);

/**
 * @brief Query the running firmware version
 *
 * @param me Chip object
 * @param ver Output buffer for version info
 * @return int 0 on success
 */
int tps6699x_version_read(Tps6699x *me, union reg_version *ver);

#endif /* __DRIVERS_EC_TPS6699X_TPS6699X_I2C_H_ */
