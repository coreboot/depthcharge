// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "common.h"
#include "drivers/video/display.h"

int video_initialized = 0;
int video_console_enabled = 1;

int init_video(void)
{
	if (video_initialized)
		return 0;
	/* initialize video console */
	video_init();
	video_console_clear();
	video_console_cursor_enable(0);

	video_initialized = 1;
	return 0;
}

int console_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char str[MAX_CONSOLE_LINE];
	int rc = vsnprintf(str, sizeof(str), fmt, args);
	va_end(args);
	console_print(str);
	return rc;
}

void console_print(const char *str)
{
	int str_len = strlen(str);

	printf("%s", str);

	if (!video_console_enabled)
		return;

	if (init_video()) {
		printf("Failed to initialize display\n");
		return;
	}

	while (str_len--) {
		if (*str == '\n')
			video_console_putchar('\r');
		video_console_putchar(*str++);
	}
}

static int do_cls(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc > 1)
		return CMD_RET_USAGE;

	video_console_clear();
	video_console_cursor_enable(0);

	return CMD_RET_SUCCESS;
}

static int do_vcstop(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc > 1)
		return CMD_RET_USAGE;
	video_console_enabled = 0;
	return CMD_RET_SUCCESS;
}

static int do_vcstart(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc > 1)
		return CMD_RET_USAGE;
	video_console_enabled = 1;
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	cls,	1,	1,
	"Clear Screen",
	""
);

U_BOOT_CMD(
	vcstop,	1,	1,
	"Stop Video Console",
	""
);

U_BOOT_CMD(
	vcstart,	1,	1,
	"Start Video Console",
	""
);