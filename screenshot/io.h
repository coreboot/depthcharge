/* SPDX-License-Identifier: GPL-2.0 */

#include <stddef.h>

size_t read_bmp(void *buffer, size_t size, const char *path);
size_t write_raw(const void *buffer, size_t size, const char *outfile);
