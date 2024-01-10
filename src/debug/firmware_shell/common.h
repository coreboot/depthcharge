/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#ifndef __DEBUG_FIRMWARE_SHELL_COMMON_H__
#define __DEBUG_FIRMWARE_SHELL_COMMON_H__

#include <libpayload.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* HACK */
#define _DEBUG 1
#define MAX_CONSOLE_LINE 512
#define SYS_MAXARGS 40

#include "debug/firmware_shell/linker_lists.h"
#include "debug/firmware_shell/command.h"

/*
 * Output a debug text when condition "cond" is met. The "cond" should be
 * computed by a preprocessor in the best case, allowing for the best
 * optimization.
 */
#define debug_cond(cond, fmt, args...)		\
	do {					\
		if (cond)			\
			printf(fmt, ##args);	\
	} while (0)

#define debug(fmt, args...)			\
	debug_cond(_DEBUG, fmt, ##args)

/*
 * Function Prototypes
 */

/* console_main.c */
void dc_dev_enter_firmware_shell(void);
bool dc_dev_firmware_shell_enabled(void);
int run_command(const char *cmd, int flag);
int ctrlc(void);

/* video.c*/
int init_video(void);
void console_print(const char *str);
int console_printf(const char *fmt, ...);

/**
 * Run a list of commands separated by ; or even \0
 *
 * Note that if 'len' is not -1, then the command does not need to be nul
 * terminated, Memory will be allocated for the command in that case.
 *
 * @param cmd	List of commands to run, each separated bu semicolon
 * @param len	Length of commands excluding terminator if known (-1 if not)
 * @param flag	Execution flags (CMD_FLAG_...)
 * @return 0 on success, or != 0 on error.
 */
int run_command_list(const char *cmd, int len, int flag);

char *getenv(const char *);

/*
 * Print data buffer in hex and ascii form to the terminal.
 *
 * data reads are buffered so that each memory address is only read once.
 * Useful when displaying the contents of volatile registers.
 *
 * parameters:
 *    addr: Starting address to display at start of line
 *    data: pointer to data buffer
 *    width: data value width.  May be 1, 2, or 4.
 *    count: number of values to display
 *    linelen: Number of values to print per line; specify 0 for default length
 */

int print_buffer(unsigned long addr, const void *data, unsigned width,
		 unsigned count, unsigned linelen);

#endif	/* __DEBUG_FIRMWARE_SHELL_COMMON_H__ */
