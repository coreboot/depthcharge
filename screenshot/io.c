// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stddef.h>

#include "io.h"

size_t get_bmp_size(const char *path)
{
	size_t size;
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		printf("ERROR: Unable to open %s\n", path);
		return 0;
	}

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fclose(fp);
	return size;
}

size_t read_bmp(void *buffer, size_t size, const char *path)
{
	size_t ret;
	FILE *fp = fopen(path, "rb");
	ret = fread(buffer, 1, size, fp);
	fclose(fp);
	return ret;
}

size_t write_raw(const void *buffer, size_t size, const char *path)
{
	int ret;
	FILE *fp = fopen(path, "wb");
	if (!fp) {
		printf("ERROR: Unable to open %s\n", path);
		return 0;
	}
	ret = fwrite(buffer, 1, size, fp);
	fclose(fp);
	return ret;
}
