/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _MOCKS_PAYLOAD_H
#define _MOCKS_PAYLOAD_H

#include <commonlib/list.h>

extern struct list_node payload_altfw_head;
extern int payload_altfw_head_initialized;

size_t payload_get_altfw_count(void);

#endif /* _MOCKS_PAYLOAD_H */
