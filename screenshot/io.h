/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __SCREENSHOT_IO_H__
#define __SCREENSHOT_IO_H__

#include <stddef.h>

size_t get_file_size(const char *path);
size_t read_file(void *buffer, size_t size, const char *path);
size_t write_file(const void *buffer, size_t size, const char *path);

#endif /* __SCREENSHOT_IO_H__ */
