// SPDX-License-Identifier: GPL-2.0

#include <vboot/secdata_tpm.h>
#include <tests/test.h>
#include <tss_constants.h>

uint8_t workbuf_firmware[VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
uint8_t workbuf_kernel[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));

/* This mock requires two calls to will_return(). First sets read buffer,
   second sets return value. */
uint32_t TlclWrite(uint32_t index, const void *data, uint32_t length)
{
	check_expected(index);
	check_expected(length);
	check_expected_ptr(data);

	memcpy(mock_ptr_type(void *), data, length);

	return mock_type(uint32_t);
}

/* This mock requires two calls to will_return(). First sets read buffer,
   second sets return value. */
uint32_t TlclRead(uint32_t index, void *data, uint32_t length)
{
	check_expected(index);
	check_expected(length);
	check_expected_ptr(data);

	memcpy(data, mock_ptr_type(void *), length);

	return mock_type(uint32_t);
}

uint32_t TlclForceClear(void)
{
	function_called();

	return mock_type(uint32_t);
}

uint32_t TlclSetEnable(void)
{
	function_called();

	return mock_type(uint32_t);
}

uint32_t TlclSetDeactivated(uint8_t flag)
{
	function_called();
	check_expected(flag);

	return mock_type(uint32_t);
}

uint32_t TlclLockPhysicalPresence(void)
{
	function_called();
	return mock_type(uint32_t);
}
