// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/elog.h"
#include "drivers/flash/flash.h"

#define ELOG_ERR(fmt, rc, args...) ({					\
	printf("%s: " fmt ", error %d\n", __func__, ## args, rc);	\
	rc;								\
})

enum elog_init_state {
	ELOG_INIT_UNINITIALIZED = 0,
	ELOG_INIT_INITIALIZED,
	ELOG_INIT_BROKEN,
};

struct elog_state {
	/* The pointer to nv_last_write will be introduced after we have elog
	   event add functions. */
	size_t last_write;

	FmapArea nv_area;
	/* Mirror the eventlog in memory. */
	void *data;

	/* Minimum of nv_area.size and ELOG_SIZE. All the elog related
	   operations should check if they exceed this elog end. */
	size_t size;

	enum elog_init_state elog_initialized;
};

static struct elog_state elog_state;

#define ELOG_SIZE (4 * KiB)
static uint8_t elog_mirror_buf[ELOG_SIZE];

static size_t elog_events_start(void)
{
	/* Events are added directly after the header. */
	return sizeof(struct elog_header);
}

/*
 * Check if mirrored buffer is filled with ELOG_TYPE_EOL byte from the
 * provided offset to the end of the mirrored buffer.
 */
static bool elog_is_buffer_clear(size_t offset)
{
	int i;
	for (i = offset; i < elog_state.size; i++)
		if (elog_mirror_buf[i] != ELOG_TYPE_EOL)
			return false;
	return true;
}

/*
 * Validate the event header and data.
 */
static bool elog_is_event_valid(const struct event_header *event,
				size_t size_limit)
{
	uint8_t len;

	if (!event)
		return false;
	len = event->length;

	/* Event length must be at least header size + checksum (uint8_t) */
	if (len < (sizeof(*event) + 1))
		return false;

	if (len > ELOG_MAX_EVENT_SIZE || len > size_limit)
		return false;

	/* If event checksum is invalid the area is corrupt */
	if (elog_checksum_event(event) != 0)
		return false;

	/* Event is valid */
	return true;
}

/*
 * Scan the event area and validate each entry and update the ELOG state.
 * It is guaranteed that the elog_mirror_buf is prepared.
 */
static int elog_update_event_buffer_state(size_t start_offset)
{
	size_t offset = start_offset;
	elog_state.last_write = start_offset;

	const struct event_header *event;
	/* Point to the current event. */
	event = elog_state.data + offset;
	while (offset + sizeof(*event) < elog_state.size) {
		if (event->type == ELOG_TYPE_EOL)
			break;
		if (!elog_is_event_valid(event, elog_state.size - offset))
			return ELOG_ERR("Bad event at offset %#x",
					ELOG_ERR_CONTENT, (unsigned int)offset);
		event = elog_get_next_event(event);
		offset = (void *)event - elog_state.data;
		elog_state.last_write = offset;
	}

	if (!elog_is_buffer_clear(elog_state.last_write))
		return ELOG_ERR("Buffer not cleared from %#x", ELOG_ERR_CONTENT,
				(unsigned int)elog_state.last_write);
	return 0;
}

elog_error_t elog_init(void)
{
	FmapArea *area = &elog_state.nv_area;
	struct elog_header *header;
	size_t elog_size;

	switch (elog_state.elog_initialized) {
	case ELOG_INIT_UNINITIALIZED:
		break;
	case ELOG_INIT_INITIALIZED:
		return ELOG_SUCCESS;
	case ELOG_INIT_BROKEN:
		return ELOG_ERR_BROKEN;
	}
	elog_state.elog_initialized = ELOG_INIT_BROKEN;

	elog_state.data = &elog_mirror_buf;

	/* Find RW region. */
	if (fmap_find_area(ELOG_RW_REGION_NAME, area))
		return ELOG_ERR("Couldn't find ELOG region", ELOG_ERR_EX);

	/* Read elog from flash. */
	elog_size = MIN(area->size, ELOG_SIZE);
	elog_state.size = elog_size;
	if (flash_read(elog_state.data, area->offset, elog_size) != elog_size)
		return ELOG_ERR("Failed to read from ELOG region with size %zu",
				ELOG_ERR_EX, elog_size);

	/* Verify header. */
	header = (struct elog_header *)elog_state.data;
	if (elog_verify_header(header) != CB_SUCCESS)
		return ELOG_ERR("Invalid ELOG header", ELOG_ERR_CONTENT);

	if (elog_update_event_buffer_state(elog_events_start()))
		return ELOG_ERR("Invalid ELOG buffer", ELOG_ERR_CONTENT);

	elog_state.elog_initialized = ELOG_INIT_INITIALIZED;

	return ELOG_ERR("ELOG context successfully initialized", ELOG_SUCCESS);
}
