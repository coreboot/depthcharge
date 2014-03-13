/*
 * (C) Copyright 2000
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "debug/cli/common.h"

typedef unsigned long ulong;

/*
 * Memory Functions
 *
 * Copied from FADS ROM, Dan Malek (dmalek@jlc.net)
 */


static int mod_mem(cmd_tbl_t *, int, int, int, char * const []);

/* Display values from last command.
 * Memory modify remembered values are different from display memory.
 */
static unsigned	dp_last_addr, dp_last_size;
static unsigned	dp_last_length = 0x40;
static unsigned	mm_last_addr, mm_last_size;

static	ulong	base_address = 0;

static inline void *map_sysmem(ulong addr, int bytes)
{
	return (void *)addr;
}

static inline void unmap_sysmem(const void *ptr) {}


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
#define MAX_LINE_LENGTH_BYTES (64)
#define DEFAULT_LINE_LENGTH_BYTES (16)

static int print_buffer(ulong addr, const void *data, unsigned width,
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

/* Memory Display
 *
 * Syntax:
 *	md{.b, .w, .l} {addr} {len}
 */
#define DISP_LINE_LEN	16
static int do_md(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong	addr, length;
	int	size;
	int rc = 0;

	/* We use the last specified parameters, unless new ones are
	 * entered.
	 */
	addr = dp_last_addr;
	size = dp_last_size;
	length = dp_last_length;

	if (argc < 2)
		return CMD_RET_USAGE;

	if ((flag & CMD_FLAG_REPEAT) == 0) {
		/* New command specified.  Check for a size specification.
		 * Defaults to long if no or incorrect specification.
		 */
		if ((size = cmd_get_data_size(argv[0], 4)) < 0)
			return 1;

		/* Address is specified since argc > 1
		*/
		addr = strtoul(argv[1], NULL, 16);
		addr += base_address;

		/* If another parameter, it is the length to display.
		 * Length is the number of objects, not number of bytes.
		 */
		if (argc > 2)
			length = strtoul(argv[2], NULL, 16);
	}


	{
		ulong bytes = size * length;
		const void *buf = map_sysmem(addr, bytes);

		/* Print the lines. */
		print_buffer(addr, buf, size, length, DISP_LINE_LEN / size);
		addr += bytes;
		unmap_sysmem(buf);
	}

	dp_last_addr = addr;
	dp_last_length = length;
	dp_last_size = size;
	return (rc);
}

static int do_mm(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	return mod_mem (cmdtp, 1, flag, argc, argv);
}
static int do_nm(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	return mod_mem (cmdtp, 0, flag, argc, argv);
}

static int do_mw(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong	addr, writeval, count;
	int	size;
	void *buf;
	ulong bytes;

	if ((argc < 3) || (argc > 4))
		return CMD_RET_USAGE;

	/* Check for size specification.
	*/
	if ((size = cmd_get_data_size(argv[0], 4)) < 1)
		return 1;

	/* Address is specified since argc > 1
	*/
	addr = strtoul(argv[1], NULL, 16);
	addr += base_address;

	/* Get the value to write.
	*/
	writeval = strtoul(argv[2], NULL, 16);

	/* Count ? */
	if (argc == 4) {
		count = strtoul(argv[3], NULL, 16);
	} else {
		count = 1;
	}

	bytes = size * count;
	buf = map_sysmem(addr, bytes);
	while (count-- > 0) {
		if (size == 4)
			*((ulong *)buf) = (ulong)writeval;
		else if (size == 2)
			*((u16 *)buf) = (u16)writeval;
		else
			*((u8 *)buf) = (u8)writeval;
		buf += size;
	}
	unmap_sysmem(buf);
	return 0;
}

static int do_cmp(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong	addr1, addr2, count, ngood, bytes;
	int	size;
	int     rcode = 0;
	const char *type;
	const void *buf1, *buf2, *base;

	if (argc != 4)
		return CMD_RET_USAGE;

	/* Check for size specification.
	*/
	if ((size = cmd_get_data_size(argv[0], 4)) < 0)
		return 1;
	type = size == 4 ? "word" : size == 2 ? "halfword" : "byte";

	addr1 = strtoul(argv[1], NULL, 16);
	addr1 += base_address;

	addr2 = strtoul(argv[2], NULL, 16);
	addr2 += base_address;

	count = strtoul(argv[3], NULL, 16);

	bytes = size * count;
	base = buf1 = map_sysmem(addr1, bytes);
	buf2 = map_sysmem(addr2, bytes);
	for (ngood = 0; ngood < count; ++ngood) {
		ulong word1, word2;
		if (size == 4) {
			word1 = *(ulong *)buf1;
			word2 = *(ulong *)buf2;
		} else if (size == 2) {
			word1 = *(u16 *)buf1;
			word2 = *(u16 *)buf2;
		} else {
			word1 = *(u8 *)buf1;
			word2 = *(u8 *)buf2;
		}
		if (word1 != word2) {
			ulong offset = buf1 - base;

			printf("%s at 0x%08lx (%#0*lx) != %s at 0x%08lx (%#0*lx)\n",
				type, (ulong)(addr1 + offset), size, word1,
				type, (ulong)(addr2 + offset), size, word2);
			rcode = 1;
			break;
		}

		buf1 += size;
		buf2 += size;
	}
	unmap_sysmem(buf1);
	unmap_sysmem(buf2);

	printf("Total of %ld %s(s) were the same\n", ngood, type);
	return rcode;
}

static int do_cp(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong	addr, dest, count, bytes;
	int	size;
	const void *src;
	void *buf;

	if (argc != 4)
		return CMD_RET_USAGE;

	/* Check for size specification.
	*/
	if ((size = cmd_get_data_size(argv[0], 4)) < 0)
		return 1;

	addr = strtoul(argv[1], NULL, 16);
	addr += base_address;

	dest = strtoul(argv[2], NULL, 16);
	dest += base_address;

	count = strtoul(argv[3], NULL, 16);

	if (count == 0) {
		puts ("Zero length ???\n");
		return 1;
	}

	bytes = size * count;
	buf = map_sysmem(dest, bytes);
	src = map_sysmem(addr, bytes);
	while (count-- > 0) {
		if (size == 4)
			*((ulong *)buf) = *((ulong  *)src);
		else if (size == 2)
			*((u16 *)buf) = *((u16 *)src);
		else
			*((u8 *)buf) = *((u8 *)src);
		src += size;
		buf += size;
	}
	return 0;
}

/* Modify memory.
 *
 * Syntax:
 *	mm{.b, .w, .l} {addr}
 *	nm{.b, .w, .l} {addr}
 */
static int
mod_mem(cmd_tbl_t *cmdtp, int incrflag, int flag, int argc, char * const argv[])
{
	ulong	addr, i;
	int	nbytes, size;
	void *ptr = NULL;

	if (argc != 2)
		return CMD_RET_USAGE;

	/* We use the last specified parameters, unless new ones are
	 * entered.
	 */
	addr = mm_last_addr;
	size = mm_last_size;

	if ((flag & CMD_FLAG_REPEAT) == 0) {
		/* New command specified.  Check for a size specification.
		 * Defaults to long if no or incorrect specification.
		 */
		if ((size = cmd_get_data_size(argv[0], 4)) < 0)
			return 1;

		/* Address is specified since argc > 1
		*/
		addr = strtoul(argv[1], NULL, 16);
		addr += base_address;
	}

	/*
	 * Create the prompt which is the address, followed by value, followed
	 * by the question mark. Then accept input for the next value.
	 * A non-converted value exits.
	 */
	do {
		char new_prompt[50];
		int index;

		ptr = map_sysmem(addr, size);
		index = snprintf(new_prompt, sizeof(new_prompt),
				 "%08lx:", addr);
		if (size == 4)
			index += snprintf(new_prompt + index,
					  sizeof(new_prompt) - index,
					  " %08x", *((unsigned *)ptr));
		else if (size == 2)
			index += snprintf(new_prompt + index,
					  sizeof(new_prompt) - index,
					  " %04x", *((u16 *)ptr));
		else
			index += snprintf(new_prompt + index,
					  sizeof(new_prompt) - index,
					  " %02x", *((u8 *)ptr));

		snprintf(new_prompt + index,
			 sizeof(new_prompt) - index,
			 " ? ");

		nbytes = read_line (new_prompt, console_buffer);
		if (nbytes == 0 || (nbytes == 1 && console_buffer[0] == '-')) {
			/* <CR> pressed as only input, don't modify current
			 * location and move to next. "-" pressed will go back.
			 */
			if (incrflag)
				addr += nbytes ? -size : size;
			nbytes = 1;
		} else {
			char *endp;
			i = strtoul(console_buffer, &endp, 16);
			nbytes = endp - console_buffer;
			if (nbytes) {
				if (size == 4)
					*((unsigned *)ptr) = i;
				else if (size == 2)
					*((u16 *)ptr) = i;
				else
					*((u8 *)ptr) = i;
				if (incrflag)
					addr += size;
			}
		}
	} while (nbytes);
	if (ptr)
		unmap_sysmem(ptr);

	mm_last_addr = addr;
	mm_last_size = size;
	return 0;
}

/**************************************************/
U_BOOT_CMD(
	md,	3,	1,
	"memory display",
	"[.b, .w, .l] address [# of objects]"
);


U_BOOT_CMD(
	mm,	2,	1,
	"memory modify (auto-incrementing address)",
	"[.b, .w, .l] address"
);


U_BOOT_CMD(
	nm,	2,	1,
	"memory modify (constant address)",
	"[.b, .w, .l] address"
);

U_BOOT_CMD(
	mw,	4,	1,
	"memory write (fill)",
	"[.b, .w, .l] address value [count]"
);

U_BOOT_CMD(
	cp,	4,	1,
	"memory copy",
	"[.b, .w, .l] source target count"
);

U_BOOT_CMD(
	cmp,	4,	1,
	"memory compare",
	"[.b, .w, .l] addr1 addr2 count"
);

