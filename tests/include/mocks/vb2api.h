/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __TESTS_MOCKS_VB2API_H__
#define __TESTS_MOCKS_VB2API_H__

extern uint32_t mock_locale_id;

#define WILL_NV_SET(_param, _value) do {		\
	expect_value(vb2api_nv_set, param, (_param));	\
	expect_value(vb2api_nv_set, value, (_value));	\
} while (0)

#endif /*__TESTS_MOCKS_VB2API_H__ */
