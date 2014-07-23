/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Copyright 2014 Google Inc.
 */

#include "common.h"

/* Max width the dump takes on the screen. */
#define MAX_LINE_LENGTH (76)

/* Default number of bytes displayed in one dump line. */
#define DEFAULT_LINE_LENGTH_BYTES (16)

int print_buffer(unsigned long addr, const void *data, unsigned width,
		 unsigned count, unsigned linelen)
{
	/* linebuf as a union causes proper alignment */
	union linebuf {
		u32 ui[MAX_LINE_LENGTH/sizeof(u32) + 1];
		u16 us[MAX_LINE_LENGTH/sizeof(u16) + 1];
		u8  uc[MAX_LINE_LENGTH/sizeof(u8) + 1];
	} lb;
	int i, spaces, overhead;

	/* How many spaces one number takes on the screen. */
	spaces = 2 * width + 1;

	/*
	 * How many spaces the overhead (address, separating whitespace and
	 * ascii representation) takes on the screen.
	 */
	overhead = 8 + width * linelen + 4;

	if (linelen < 1)
		linelen = DEFAULT_LINE_LENGTH_BYTES / width;
	else if ((linelen * spaces + overhead) > MAX_LINE_LENGTH)
		linelen = (MAX_LINE_LENGTH - overhead) / spaces;

	while (count) {
		unsigned thislinelen;
		int pad_size;

		printf("%08lx:", addr);

		/* Limit number of elements in the last line of the dump. */
		thislinelen = count < linelen ? count : linelen;

		/* Copy from memory into linebuf and print hex values */
		for (i = 0; i < thislinelen; i++) {
			u32 x;
			if (width == 4)
				x = lb.ui[i] = *(volatile u32 *)data;
			else if (width == 2)
				x = lb.us[i] = *(volatile u16 *)data;
			else
				x = lb.uc[i] = *(volatile u8 *)data;
			printf(" %0*x", width * 2, x);
			data += width;
		}

		pad_size = spaces * (linelen - thislinelen);
		while (pad_size--)
			printf(" ");

		/* Print data in ASCII characters */
		for (i = 0; i < thislinelen * width; i++) {
			if (!isprint(lb.uc[i]) || lb.uc[i] >= 0x80)
				lb.uc[i] = '.';
		}
		lb.uc[i] = '\0';
		printf("    %s\n", lb.uc); /* 4 space separator. */

		/* update references */
		addr += thislinelen * width;
		count -= thislinelen;

		if (ctrlc())
			return -1;
	}

	return 0;
}
