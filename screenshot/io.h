/* SPDX-License-Identifier: GPL-2.0 */

#include <stddef.h>

size_t get_file_size(const char *path);
size_t read_file(void *buffer, size_t size, const char *path);
size_t write_file(const void *buffer, size_t size, const char *path);
