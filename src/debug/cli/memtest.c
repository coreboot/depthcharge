/*
 * Very simple but very effective user-space memory tester.
 *
 * Originally by Simon Kirby <sim@stormix.com> <sim@neato.org>
 * Version 2 by Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Version 3 not publicly released.
 * Version 4 rewrite:
 * Copyright (C) 2004-2012 Charles Cazabon <charlesc-memtester@pyropus.ca>
 *
 * Licensed under the terms of the GNU General Public License version 2 (only).
 */

#include "common.h"

#define UL_ONEBITS 0xffffffff
#define UL_LEN 32
#define UL_BYTE(x) ((x | x << 8 | x << 16 | x << 24))
#define rand_ul() ((unsigned int) rand() | ( (unsigned int) rand() << 16))

#define CHECKERBOARD1 0x55555555
#define CHECKERBOARD2 0xaaaaaaaa

const char progress[] = "-\\|/";
#define PROGRESSLEN 4
#define PROGRESSOFTEN 2500
#define ONE 0x00000001L

typedef unsigned long ul;
typedef unsigned long volatile ulv;

struct test {
	char *name;
	int (*fp)();
};

static int compare_regions(ulv *bufa, ulv *bufb, size_t count)
{
	int r = 0;
	size_t i;
	ulv *p1 = bufa;
	ulv *p2 = bufb;

	for (i = 0; i < count; i++, p1++, p2++) {
		if (*p1 == *p2)
			continue;
		fprintf(stderr,
			"FAILURE: 0x%08lx != 0x%08lx at address 0x%08lx.\n",
			(ul)*p1, (ul)*p2, (ul)(i * sizeof(ul)));
		r = -1;
	}
	return r;
}

/* Test definitions */

static int test_stuck_address(ulv *bufa, size_t count)
{
	ulv *p1 = bufa;
	unsigned int j;
	size_t i;

	printf("           ");

	for (j = 0; j < 16; j++) {
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		p1 = (ulv *)bufa;
		printf("setting %3u", j);

		for (i = 0; i < count; i++) {
			*p1 = ((j + i) % 2) == 0 ? (ul)p1 : ~((ul)p1);
			*p1++;
		}
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		printf("testing %3u", j);

		p1 = (ulv *)bufa;
		for (i = 0; i < count; i++, p1++) {
			if (*p1 != (((j + i) % 2) == 0 ? (ul)p1 : ~((ul)p1))) {
				fprintf(stderr,	"FAILURE: possible bad address"
					" line at address 0x%08lx.\n",
					(ul) (i * sizeof(ul)));
				return -1;
			}
		}
	}
	printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");

	return 0;
}

static int test_random_value(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	ul j = 0;
	size_t i;

	putchar(' ');

	for (i = 0; i < count; i++) {
		*p1++ = *p2++ = rand_ul();
		if (!(i % PROGRESSOFTEN)) {
			putchar('\b');
			putchar(progress[++j % PROGRESSLEN]);

		}
	}
	printf("\b \b");

	return compare_regions(bufa, bufb, count);
}

static int test_xor_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	size_t i;
	ul q = rand_ul();

	for (i = 0; i < count; i++) {
		*p1++ ^= q;
		*p2++ ^= q;
	}
	return compare_regions(bufa, bufb, count);
}

static int test_sub_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	size_t i;
	ul q = rand_ul();

	for (i = 0; i < count; i++) {
		*p1++ -= q;
		*p2++ -= q;
	}
	return compare_regions(bufa, bufb, count);
}

static int test_mul_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	size_t i;
	ul q = rand_ul();

	for (i = 0; i < count; i++) {
		*p1++ *= q;
		*p2++ *= q;
	}
	return compare_regions(bufa, bufb, count);
}

static int test_div_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	size_t i;
	ul q = rand_ul();

	for (i = 0; i < count; i++) {
		if (!q) {
			q++;
		}
		*p1++ /= q;
		*p2++ /= q;
	}
	return compare_regions(bufa, bufb, count);
}

static int test_or_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	size_t i;
	ul q = rand_ul();

	for (i = 0; i < count; i++) {
		*p1++ |= q;
		*p2++ |= q;
	}
	return compare_regions(bufa, bufb, count);
}

static int test_and_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	size_t i;
	ul q = rand_ul();

	for (i = 0; i < count; i++) {
		*p1++ &= q;
		*p2++ &= q;
	}
	return compare_regions(bufa, bufb, count);
}

static int test_seqinc_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	size_t i;
	ul q = rand_ul();

	for (i = 0; i < count; i++) {
		*p1++ = *p2++ = (i + q);
	}
	return compare_regions(bufa, bufb, count);
}

static int test_solidbits_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	unsigned int j;
	ul q;
	size_t i;

	printf("           ");

	for (j = 0; j < 64; j++) {
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		q = (j % 2) == 0 ? UL_ONEBITS : 0;
		printf("setting %3u", j);

		p1 = (ulv *) bufa;
		p2 = (ulv *) bufb;
		for (i = 0; i < count; i++) {
			*p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
		}
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		printf("testing %3u", j);

		if (compare_regions(bufa, bufb, count)) {
			return -1;
		}
	}
	printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");

	return 0;
}

static int test_checkerboard_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	unsigned int j;
	ul q;
	size_t i;

	printf("           ");

	for (j = 0; j < 64; j++) {
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		q = (j % 2) == 0 ? CHECKERBOARD1 : CHECKERBOARD2;
		printf("setting %3u", j);

		p1 = (ulv *) bufa;
		p2 = (ulv *) bufb;
		for (i = 0; i < count; i++) {
			*p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
		}
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		printf("testing %3u", j);

		if (compare_regions(bufa, bufb, count)) {
			return -1;
		}
	}
	printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");

	return 0;
}

static int test_blockseq_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	unsigned int j;
	size_t i;

	printf("           ");

	for (j = 0; j < 256; j++) {
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		p1 = (ulv *) bufa;
		p2 = (ulv *) bufb;
		printf("setting %3u", j);

		for (i = 0; i < count; i++) {
			*p1++ = *p2++ = (ul) UL_BYTE(j);
		}
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		printf("testing %3u", j);

		if (compare_regions(bufa, bufb, count)) {
			return -1;
		}
	}
	printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");

	return 0;
}

static int test_walkbits0_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	unsigned int j;
	size_t i;

	printf("           ");

	for (j = 0; j < UL_LEN * 2; j++) {
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		p1 = (ulv *) bufa;
		p2 = (ulv *) bufb;
		printf("setting %3u", j);

		for (i = 0; i < count; i++) {
			if (j < UL_LEN) { /* Walk it up. */
				*p1++ = *p2++ = ONE << j;
			} else { /* Walk it back down. */
				*p1++ = *p2++ = ONE << (UL_LEN * 2 - j - 1);
			}
		}
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		printf("testing %3u", j);

		if (compare_regions(bufa, bufb, count)) {
			return -1;
		}
	}
	printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");

	return 0;
}

static int test_walkbits1_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	unsigned int j;
	size_t i;

	printf("           ");

	for (j = 0; j < UL_LEN * 2; j++) {
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		p1 = (ulv *) bufa;
		p2 = (ulv *) bufb;
		printf("setting %3u", j);

		for (i = 0; i < count; i++) {
			if (j < UL_LEN) { /* Walk it up. */
				*p1++ = *p2++ = UL_ONEBITS ^ (ONE << j);
			} else { /* Walk it back down. */
				*p1++ = *p2++ = UL_ONEBITS ^
					(ONE << (UL_LEN * 2 - j - 1));
			}
		}
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		printf("testing %3u", j);

		if (compare_regions(bufa, bufb, count)) {
			return -1;
		}
	}
	printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");

	return 0;
}

static int test_bitspread_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	unsigned int j;
	size_t i;

	printf("           ");

	for (j = 0; j < UL_LEN * 2; j++) {
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		p1 = (ulv *) bufa;
		p2 = (ulv *) bufb;
		printf("setting %3u", j);

		for (i = 0; i < count; i++) {
			if (j < UL_LEN) { /* Walk it up. */
				*p1++ = *p2++ = (i % 2 == 0)
					? (ONE << j) | (ONE << (j + 2))
					: UL_ONEBITS ^ ((ONE << j)
							| (ONE << (j + 2)));
			} else { /* Walk it back down. */
				*p1++ = *p2++ = (i % 2 == 0)
					? (ONE << (UL_LEN * 2 - 1 - j)) |
					(ONE << (UL_LEN * 2 + 1 - j))
					: UL_ONEBITS ^
					(ONE << (UL_LEN * 2 - 1 - j)
					 | (ONE << (UL_LEN * 2 + 1 - j)));
			}
		}
		printf("\b\b\b\b\b\b\b\b\b\b\b");
		printf("testing %3u", j);

		if (compare_regions(bufa, bufb, count)) {
			return -1;
		}
	}
	printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");

	return 0;
}

static int test_bitflip_comparison(ulv *bufa, ulv *bufb, size_t count)
{
	ulv *p1 = bufa;
	ulv *p2 = bufb;
	unsigned int j, k;
	ul q;
	size_t i;

	printf("           ");

	for (k = 0; k < UL_LEN; k++) {
		q = ONE << k;
		for (j = 0; j < 8; j++) {
			printf("\b\b\b\b\b\b\b\b\b\b\b");
			q = ~q;
			printf("setting %3u", k * 8 + j);

			p1 = (ulv *) bufa;
			p2 = (ulv *) bufb;
			for (i = 0; i < count; i++) {
				*p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
			}
			printf("\b\b\b\b\b\b\b\b\b\b\b");
			printf("testing %3u", k * 8 + j);

			if (compare_regions(bufa, bufb, count)) {
				return -1;
			}
		}
	}
	printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");

	return 0;
}

struct test tests[] = {
	{ "Random Value", test_random_value },
	{ "Compare XOR", test_xor_comparison },
	{ "Compare SUB", test_sub_comparison },
	{ "Compare MUL", test_mul_comparison },
	{ "Compare DIV",test_div_comparison },
	{ "Compare OR", test_or_comparison },
	{ "Compare AND", test_and_comparison },
	{ "Sequential Increment", test_seqinc_comparison },
	{ "Solid Bits", test_solidbits_comparison },
	{ "Block Sequential", test_blockseq_comparison },
	{ "Checkerboard", test_checkerboard_comparison },
	{ "Bit Spread", test_bitspread_comparison },
	{ "Bit Flip", test_bitflip_comparison },
	{ "Walking Ones", test_walkbits1_comparison },
	{ "Walking Zeroes", test_walkbits0_comparison },
	{ NULL, NULL }
};

static int do_memtest(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	struct test *t;
	ulv *bufa, *bufb;
	ul start, length;
	size_t halflen, count;
	int rc;
	int loop, loops = 1;

	if (argc < 3) {
		printf("Required command line parameters missing\n");
		return CMD_RET_USAGE;
	}

	start = strtoul(argv[1], 0, 16);
	length = strtoul(argv[2], 0, 16);

	if (argc > 3)
		loops = strtoul(argv[3], 0, 16);

	if (start == 0 || length == 0) {
		printf("Invalid start address\n");
		return CMD_RET_USAGE;
	}

	/* Set up buffer pointers */
	halflen = length / 2;
	count = halflen / sizeof(ul);
	bufa = (ulv *)start;
	bufb = (ulv *)((size_t)start + halflen);

	rc = CMD_RET_SUCCESS;
	for (loop = 1; loop <= loops; loop++) {

		printf("Loop %lu/%lu:\n", (ul)loop, (ul)loops);
		printf("  %-25s: ", "Stuck Address");

		if (!test_stuck_address(bufa, length / sizeof(ul)))
			printf("ok\n");
		else
			rc = CMD_RET_FAILURE;

		for (t = tests; t && t->name; t++) {
			printf("  %-25s: ", t->name);
			if (!t->fp(bufa, bufb, count))
				printf("ok\n");
			else {
				rc = CMD_RET_FAILURE;
				printf("fail\n");
			}
		}
	}
	printf("\nDone.\n");

	return rc;
}

U_BOOT_CMD(
	   memtest,	4,	1,
	   "basic memory test",
	   "\n<start> <length> [loops]  -  test a range of memory"
);
