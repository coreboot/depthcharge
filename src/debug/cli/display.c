
#include <vboot_api.h>
#include <vboot/screens.h>
#include "common.h"

#include "drivers/video/display.h"

static const enum VbScreenType_t screen_map[] = {
	VB_SCREEN_BLANK,
	VB_SCREEN_DEVELOPER_WARNING,
	VB_SCREEN_RECOVERY_REMOVE,
	VB_SCREEN_RECOVERY_INSERT,
	VB_SCREEN_RECOVERY_TO_DEV,
	VB_SCREEN_RECOVERY_NO_GOOD,
	VB_SCREEN_DEVELOPER_TO_NORM,
	VB_SCREEN_WAIT,
	VB_SCREEN_TO_NORM_CONFIRMED,
	VB_SCREEN_OS_BROKEN,
	VB_SCREEN_DEVELOPER_EGG,
};

static int do_display(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned display = strtoul(argv[1], 0, 10);
	uint32_t locale = 0;

	if (display >= ARRAY_SIZE(screen_map)) {
		printf("Unsupported screen number %d\n", display);
		return CMD_RET_USAGE;
	}

	return vboot_draw_screen(screen_map[display], locale);
}

U_BOOT_CMD(
	   display,	2,	1,
	   "rudimentary display test",
	   "<num> - trigger display of a sertain screen\n"
	   "         0 - off\n"
	   "         1 - dev mode\n"
	   "         2 - recovery USB remove\n"
	   "         3 - recovery USB insert\n"
	   "         4 - waiting to transition to dev mode\n"
	   "         5 - bad USB stick\n"
	   "         6 - waiting to transition to normal mode\n"
	   "         7 - waiting EC programming\n"
	   "         8 - confirm to normal mode\n"
	   "         9 - verification failure\n"
	   "        10 - easter egg\n"
);
