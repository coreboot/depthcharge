/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Copyright 2014 Google Inc.
 */

#include "common.h"

#define MAX_LINE_LENGTH_BYTES (64)
#define DEFAULT_LINE_LENGTH_BYTES (16)

int print_buffer(unsigned long addr, const void *data, unsigned width,
		 unsigned count, unsigned linelen)
{
	/* linebuf as a union causes proper alignment */
	union linebuf {
		u32 ui[MAX_LINE_LENGTH_BYTES/sizeof(u32) + 1];
		u16 us[MAX_LINE_LENGTH_BYTES/sizeof(u16) + 1];
		u8  uc[MAX_LINE_LENGTH_BYTES/sizeof(u8) + 1];
	} lb;
	int i;

	if (linelen*width > MAX_LINE_LENGTH_BYTES)
		linelen = MAX_LINE_LENGTH_BYTES / width;
	if (linelen < 1)
		linelen = DEFAULT_LINE_LENGTH_BYTES / width;

	while (count) {
		unsigned thislinelen = linelen;
		printf("%08lx:", addr);

		/* check for overflow condition */
		if (count < thislinelen)
			thislinelen = count;

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

		while (thislinelen < linelen) {
			/* fill line with whitespace for nice ASCII print */
			for (i=0; i<width*2+1; i++)
				puts(" ");
			linelen--;
		}

		/* Print data in ASCII characters */
		for (i = 0; i < thislinelen * width; i++) {
			if (!isprint(lb.uc[i]) || lb.uc[i] >= 0x80)
				lb.uc[i] = '.';
		}
		lb.uc[i] = '\0';
		printf("    %s\n", lb.uc);

		/* update references */
		addr += thislinelen * width;
		count -= thislinelen;

		if (ctrlc())
			return -1;
	}

	return 0;
}
