// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Google LLC.  */

#include <stdio.h>

#include "arch/types.h"
#include "drivers/ec/cros/ec.h"

#include "tps6699x.h"
#include "tps6699x_i2c.h"

/**
 * @brief Low-level I2C handler for PDC access
 *
 * @param bus Pointer to CrosECTunnelI2c driver to use
 * @param addr I2C target address of the PDC
 * @param reg Register to access
 * @param buf Buffer of data to read/write
 * @param len Length of \p buf
 * @param read True to read, false to write
 * @return int 0 on success
 */
static int tps6699x_xfer_reg(CrosECTunnelI2c *bus, uint8_t addr, uint8_t reg, uint8_t *buf,
			     uint8_t len, bool read)
{
	if (bus == NULL || buf == NULL || len < 2) {
		debug("Invalid params to %s: bus=%p, buf=%p, len=%u", __func__, bus, buf, len);
		return -1;
	}

	if (read) {
		/* Read from register */

		/* Skip reg value in buf[0], and decrement
		 * length by 1. The PDC returns the number of bytes
		 * read in buf[1].
		 */
		return i2c_readblock(&bus->ops, addr, reg, &buf[1], len - 1);
	}

	/* Write to register */

	/* Number of bytes in message (excluding the register and this size field) */
	buf[1] = len - 2;

	/* Start with buf[1] and decrement the length because i2c_writeblock
	 * automatically pre-pends the register.
	 */
	return i2c_writeblock(&bus->ops, addr, reg, &buf[1], len - 1);
}

int tps6699x_command_reg_write(Tps6699x *me, union reg_command *cmd)
{
	return tps6699x_xfer_reg(me->bus, me->chip_info.i2c_addr, TPSREG_CMD1, cmd->raw_value,
				 sizeof(union reg_command), /*read=*/false);
}

int tps6699x_command_reg_read(Tps6699x *me, union reg_command *cmd)
{
	return tps6699x_xfer_reg(me->bus, me->chip_info.i2c_addr, TPSREG_CMD1, cmd->raw_value,
				 sizeof(union reg_command), /*read=*/true);
}

int tps6699x_data_reg_write(Tps6699x *me, union reg_data *data)
{
	return tps6699x_xfer_reg(me->bus, me->chip_info.i2c_addr, TPSREG_DATA1, data->raw_value,
				 sizeof(union reg_data), /*read=*/false);
}

int tps6699x_data_reg_read(Tps6699x *me, union reg_data *data)
{
	return tps6699x_xfer_reg(me->bus, me->chip_info.i2c_addr, TPSREG_DATA1, data->raw_value,
				 sizeof(union reg_data), /*read=*/true);
}

int tps6699x_mode_read(Tps6699x *me, union reg_mode *mode)
{
	return tps6699x_xfer_reg(me->bus, me->chip_info.i2c_addr, TPSREG_MODE, mode->raw_value,
				 sizeof(union reg_mode), /*read=*/true);
}

int tps6699x_version_read(Tps6699x *me, union reg_version *ver)
{
	return tps6699x_xfer_reg(me->bus, me->chip_info.i2c_addr, TPSREG_VERSION,
				 ver->raw_value, sizeof(union reg_version), /*read=*/true);
}

int tps6699x_stream_data(Tps6699x *me, const uint8_t broadcast_address, const uint8_t *buf,
			 size_t buf_len)
{
	int rv;

	if (buf == NULL)
		return -1;

	/* Perform the transfer in chunks */
	for (int chunk_offset = 0; chunk_offset < buf_len;
	     chunk_offset += TPS6699X_DATA_CHUNK_LEN) {
		const uint8_t *write_start_pos = buf + chunk_offset;
		int write_length = MIN(TPS6699X_DATA_CHUNK_LEN, buf_len - chunk_offset);

		rv = i2c_write_raw(&me->bus->ops, broadcast_address, (uint8_t *)write_start_pos,
				   write_length);
		if (rv) {
			printf("%s: Streaming data block failed (ret=%d, "
			       "offset_into_block=%d, total_block_size=%zu,"
			       "chunk_size=%d)",
			       me->chip_name, rv, chunk_offset, buf_len,
			       TPS6699X_DATA_CHUNK_LEN);
			return rv;
		}

		/* Periodically print a progress log message */
		if ((chunk_offset / TPS6699X_DATA_CHUNK_LEN) % 32 == 0) {
			printf("%s:  Block progress %u / %zu\n", me->chip_name, chunk_offset,
			       buf_len);
		}
	}

	printf("%s:  Block complete (%zu)\n", me->chip_name, buf_len);
	return 0;
}
