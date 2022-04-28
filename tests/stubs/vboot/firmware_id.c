// SPDX-License-Identifier: GPL-2.0

#include "vboot/firmware_id.h"

const char *get_fw_id(int index)
{
	return "mock fw_id";
}

int get_fw_size(int index)
{
	/* Random positive value */
	return 0x1000;
}

const char *get_ro_fw_id(void)
{
	return "mock ro_fw_id";
}

const char *get_rwa_fw_id(void)
{
	return "mock rwa_fw_id";
}

const char *get_rwb_fw_id(void)
{
	return "mock rwb_fw_id";
}

int get_ro_fw_size(void)
{
	return 0x1000;
}

int get_rwa_fw_size(void)
{
	return 0x1000;
}

int get_rwb_fw_size(void)
{
	return 0x1000;
}

int get_active_fw_index(void)
{
	return 0;
}

const char *get_active_fw_id(void)
{
	return "mock artive_fw_id";
}

int get_active_fw_size(void)
{
	return 0x1000;
}
