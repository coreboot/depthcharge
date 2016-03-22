/*
 * Copyright 2013 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __NETBOOT_PARAMS_H__
#define __NETBOOT_PARAMS_H__

#include <stdint.h>

#include "net/uip.h"

typedef enum NetbootParamId
{
	NetbootParamIdTftpServerIp = 1,
	NetbootParamIdKernelArgs = 2,
	NetbootParamIdBootfile = 3,
	NetbootParamIdArgsFile = 4,

	NetbootParamIdMax
} NetbootParamId;

typedef struct NetbootParam
{
	void *data;
	int size;
} NetbootParam;

int netboot_params_init(void *data, uintptr_t size);
int netboot_params_read(uip_ipaddr_t **tftp_ip, char *cmd_line,
			size_t cmd_line_max, char **bootfile, char **argsfile);
NetbootParam *netboot_params_val(NetbootParamId paramId);

#endif /* __NETBOOT_PARAMS_H__ */
