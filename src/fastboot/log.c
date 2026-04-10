// SPDX-License-Identifier: GPL-2.0
/* Copyright 2026 Google LLC. */

#include <libpayload.h>
#include <stdlib.h>

#include "fastboot/fastboot.h"
#include "fastboot/log.h"
#include "vboot/ui.h"

/* Log instance that should collect printed messages */
static struct fastboot_log *active_fastboot_log_instance;

static void fastboot_logger_write(const void *buf, size_t count)
{
	struct fastboot_log *log;
	const char *str = buf;

	if (!active_fastboot_log_instance)
		return;

	log = active_fastboot_log_instance;
	log->total_len += count;

	if (count > FASTBOOT_LOG_BUF_SIZE) {
		/* We will not fit everything to the buffer, copy last part */
		str += count - FASTBOOT_LOG_BUF_SIZE;
		count = FASTBOOT_LOG_BUF_SIZE;
	}

	size_t to_copy = MIN(FASTBOOT_LOG_BUF_SIZE - log->idx, count);
	memcpy(log->buf + log->idx, str, to_copy);
	log->idx += to_copy;
	if (log->idx >= FASTBOOT_LOG_BUF_SIZE) {
		str += to_copy;
		count -= to_copy;
		memcpy(log->buf, str, count);
		log->idx = count;
	}
}

uint64_t fastboot_log_get_total_bytes(const struct fastboot_log *log)
{
	return log->total_len;
}

uint64_t fastboot_log_get_oldest_available_byte(const struct fastboot_log *log)
{
	if (log->total_len < FASTBOOT_LOG_BUF_SIZE)
		return 0;

	return log->total_len - FASTBOOT_LOG_BUF_SIZE;
}

const char *fastboot_log_get_buf(const struct fastboot_log *log, uint64_t start, size_t *num)
{
	const uint64_t oldest = fastboot_log_get_oldest_available_byte(log);

	if (start < oldest || start >= log->total_len)
		return NULL;

	if (log->idx == log->total_len) {
		/* We have only part of buf up to the idx */
		*num = MIN(log->total_len - start, *num);
		return log->buf + start;
	}

	uint64_t offset = start - oldest;
	if (log->idx + offset >= FASTBOOT_LOG_BUF_SIZE) {
		/* We need data only from part of buf before idx */
		*num = MIN(log->total_len - start, *num);
		return log->buf + (log->idx + offset) % FASTBOOT_LOG_BUF_SIZE;
	}

	const size_t right_part_len = FASTBOOT_LOG_BUF_SIZE - log->idx - offset;
	if (right_part_len >= *num)
		/* We need data only from part of buf after idx */
		return log->buf + log->idx + offset;

	/* We need to combine both parts of buf */
	*num = MIN(right_part_len + log->idx, *num);

	char *buf = malloc(*num);
	if (!buf)
		return NULL;

	memcpy(buf, log->buf + log->idx + offset, right_part_len);
	memcpy(buf + right_part_len, log->buf, *num - right_part_len);

	return buf;
}

void fastboot_log_drop_buf(const struct fastboot_log *log, const char *buf)
{
	if (!buf || (buf >= log->buf && buf < log->buf + FASTBOOT_LOG_BUF_SIZE))
		/* NULL or inside our log->buf */
		return;

	/* It should be the buffer created in fastboot_log_get_buf, drop const */
	free((void *)buf);
}

uint64_t fastboot_log_get_iter(const struct fastboot_log *log, uint64_t byte)
{
	const uint64_t oldest = fastboot_log_get_oldest_available_byte(log);

	if (byte < oldest || byte >= log->total_len)
		return UINT64_MAX;

	if (log->idx == log->total_len)
		return byte;

	return (log->idx + byte - oldest) % FASTBOOT_LOG_BUF_SIZE;
}

bool fastboot_log_inc_iter(const struct fastboot_log *log, uint64_t *iter)
{
	uint64_t next = (*iter + 1) % FASTBOOT_LOG_BUF_SIZE;

	if (next == log->idx)
		return false;

	*iter = next;
	return true;
}

bool fastboot_log_dec_iter(const struct fastboot_log *log, uint64_t *iter)
{
	uint64_t next = (*iter + FASTBOOT_LOG_BUF_SIZE - 1) % FASTBOOT_LOG_BUF_SIZE;

	if (*iter == log->idx || next >= log->total_len)
		return false;

	*iter = next;
	return true;
}

char fastboot_log_get_byte_at_iter(const struct fastboot_log *log, uint64_t iter)
{
	return log->buf[iter];
}

static struct console_output_driver fastboot_logger = {
	.write = fastboot_logger_write,
};

void fastboot_log_set_active(struct fastboot_log *log)
{
	active_fastboot_log_instance = log;
}

void fastboot_log_init(struct FastbootOps *fb)
{
	fb->log = xmalloc(sizeof(struct fastboot_log));
	fb->log->idx = 0;
	fb->log->total_len = 0;

	fastboot_log_set_active(NULL);
	console_add_output_driver(&fastboot_logger);
}

void fastboot_log_release(struct FastbootOps *fb)
{
	fastboot_log_set_active(NULL);
	free(fb->log);
	fb->log = NULL;
}
