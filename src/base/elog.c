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
	/*
	 * The non-volatile storage chases the mirrored copy.
	 * When nv_last_write is less than the mirrored last write the
	 * non-volatile storage needs to be updated.
	 */
	size_t last_write;
	size_t nv_last_write;

	FmapArea nv_area;
	/* Mirror the eventlog in memory. */
	void *data;

	/* Minimum of nv_area.size and ELOG_SIZE. All the elog related
	   operations should check if they exceed this elog end. */
	size_t size;

	/* Depthcharge does not support RTC now, use the RTC of the last event
	   while adding new events. */
	const struct event_header *last_rtc_event;

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
	elog_state.last_rtc_event = NULL;

	const struct event_header *event;
	/* Point to the current event. */
	event = elog_state.data + offset;
	while (offset + sizeof(*event) < elog_state.size) {
		if (event->type == ELOG_TYPE_EOL)
			break;
		if (!elog_is_event_valid(event, elog_state.size - offset))
			return ELOG_ERR("Bad event at offset %#x",
					ELOG_ERR_CONTENT, (unsigned int)offset);

		/* Point to the latest header with a valid timestamp. */
		if (event->day != 0)
			elog_state.last_rtc_event = event;

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
	elog_state.nv_last_write = elog_state.last_write;

	elog_state.elog_initialized = ELOG_INIT_INITIALIZED;

	return ELOG_ERR("ELOG context successfully initialized", ELOG_SUCCESS);
}

static elog_error_t elog_sync_to_flash(void)
{
	int rv;
	/* Nothing to write */
	if (elog_state.nv_last_write >= elog_state.last_write)
		return ELOG_SUCCESS;

	size_t write_size = elog_state.last_write - elog_state.nv_last_write;
	rv = flash_write(elog_state.data + elog_state.nv_last_write,
			 elog_state.nv_area.offset + elog_state.nv_last_write,
			 write_size);
	if (rv != write_size)
		return ELOG_ERR("Failed to write to ELOG region", ELOG_ERR_EX);
	elog_state.nv_last_write = elog_state.last_write;
	return ELOG_SUCCESS;
}

elog_error_t elog_add_event_raw(uint8_t event_type, void *data,
				uint8_t data_size)
{
	struct event_header *event;
	uint8_t event_size;

	/* Make sure ELOG structures are initialized */
	int rv = elog_init();
	if (rv)
		return ELOG_ERR("ELOG broken", rv);

	/* Header + Data + Checksum */
	event_size = sizeof(struct event_header) + data_size + 1;
	if (event_size > ELOG_MAX_EVENT_SIZE)
		return ELOG_ERR("ELOG: Event(%X) data size too big (%u)",
				ELOG_ERR_CONTENT, event_type, event_size);

	/* Make sure event data can fit */
	if (elog_state.last_write + event_size > elog_state.size)
		return ELOG_ERR("ELOG: Event(%X) does not fit",
				ELOG_ERR_CONTENT, event_type);
	event = elog_state.data + elog_state.last_write;

	/* Copy the timestamp from the last valid event header */
	if (elog_state.last_rtc_event)
		memcpy(event, elog_state.last_rtc_event,
		       sizeof(struct event_header));

	/* Fill out event data */
	event->type = event_type;
	event->length = event_size;

	if (data_size)
		memcpy(&event[1], data, data_size);

	printf("ELOG: Event(%X) added with size %u\n", event_type, event_size);

	/* Zero the checksum byte and then compute checksum */
	elog_update_checksum(event, 0);
	elog_update_checksum(event, -(elog_checksum_event(event)));

	elog_state.last_write += event_size;

	/* Shrink the log if we are getting too full */
	/* No need to shrink in depthcharge now since we have already checked it
	   in coreboot and we only want to add one event now. */

	/* Ensure the updates hit the non-volatile storage. */
	return elog_sync_to_flash();
}
