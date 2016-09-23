/*
 * Copyright 2016 Chromium OS Authors
 * Command for testing TPM.
 */

#include "common.h"
#include "drivers/tpm/tpm.h"

#define MAX_BUF_LEN 1024

static int do_tpm_raw(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	uint8_t recv[MAX_BUF_LEN];
	uint8_t send[MAX_BUF_LEN];
	int arg;

	if (argc > MAX_BUF_LEN) {
		printf("Argument list too long\n");
		return CMD_RET_USAGE;
	}

	for (arg = 1; arg < argc; arg++)
		send[arg-1] = (uint8_t)strtoul(argv[arg], 0, 16);

	size_t recv_size = MAX_BUF_LEN;
	int ret = tpm_xmit(send, argc, recv, &recv_size);

	printf("TPM returned %d (%zd bytes)\n", ret, recv_size);
	if (!ret)
		hexdump(recv, recv_size);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	   tpm_raw,	MAX_BUF_LEN,	1,
	   "Send a raw TPM command and read response data",
	   "\n"
);
