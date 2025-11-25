#include <libpayload.h>
#include "vpd_util.h"
#include "drivers/soc/qcom_smem.h"

const u8 *vpd_find(const char *key, const u8 *blob, u32 *offset, u32 *size)
{
	return NULL;
}

char *vpd_gets(const char *key, char *buffer, u32 size)
{
	if (strcmp(key, "serial_number") == 0) {
		uint32_t serial = qcom_smem_get_chip_serial();
		if (serial) {
			snprintf(buffer, size, "%08x", serial);
			return buffer;
		}
	}

	return NULL;
}
