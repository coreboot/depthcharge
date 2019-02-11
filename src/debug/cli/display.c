
#include <vboot_api.h>
#include <vboot/screens.h>
#include "common.h"

#include "drivers/video/display.h"

static const enum VbScreenType_t screen_map[] = {
	VB_SCREEN_BLANK,
	VB_SCREEN_DEVELOPER_WARNING,
	VB_SCREEN_RECOVERY_INSERT,
	VB_SCREEN_RECOVERY_TO_DEV,
	VB_SCREEN_RECOVERY_NO_GOOD,
	VB_SCREEN_DEVELOPER_TO_NORM,
	VB_SCREEN_WAIT,
	VB_SCREEN_TO_NORM_CONFIRMED,
	VB_SCREEN_OS_BROKEN,
};

static int do_display(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned display = strtoul(argv[1], 0, 10);
	uint32_t locale = 0;

	if (display >= ARRAY_SIZE(screen_map)) {
		printf("Unsupported screen number %d\n", display);
		return CMD_RET_USAGE;
	}

	return vboot_draw_screen(screen_map[display], locale, NULL);
}

U_BOOT_CMD(
	   display,	2,	1,
	   "rudimentary display test",
	   "<num> - trigger display of a certain screen\n"
	   "         0 - off\n"
	   "         1 - dev mode\n"
	   "         2 - recovery USB insert\n"
	   "         3 - waiting to transition to dev mode\n"
	   "         4 - bad USB stick\n"
	   "         5 - waiting to transition to normal mode\n"
	   "         6 - waiting EC programming\n"
	   "         7 - confirm to normal mode\n"
	   "         8 - verification failure\n"
);
