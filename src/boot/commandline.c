/*
 * Copyright 2012 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <stdint.h>

#include "boot/commandline.h"

static char *itoa(char *dest, int val)
{
	if (val > 9)
		*dest++ = '0' + val / 10;
	*dest++ = '0' + val % 10;
	return dest;
}

static void one_byte(char *dest, uint8_t val)
{
	dest[0] = "0123456789abcdef"[(val >> 4) & 0x0F];
	dest[1] = "0123456789abcdef"[val & 0x0F];
}

static char *emit_guid(char *dest, uint8_t *guid)
{
	one_byte(dest, guid[3]); dest += 2;
	one_byte(dest, guid[2]); dest += 2;
	one_byte(dest, guid[1]); dest += 2;
	one_byte(dest, guid[0]); dest += 2;
	*dest++ = '-';
	one_byte(dest, guid[5]); dest += 2;
	one_byte(dest, guid[4]); dest += 2;
	*dest++ = '-';
	one_byte(dest, guid[7]); dest += 2;
	one_byte(dest, guid[6]); dest += 2;
	*dest++ = '-';
	one_byte(dest, guid[8]); dest += 2;
	one_byte(dest, guid[9]); dest += 2;
	*dest++ = '-';
	one_byte(dest, guid[10]); dest += 2;
	one_byte(dest, guid[11]); dest += 2;
	one_byte(dest, guid[12]); dest += 2;
	one_byte(dest, guid[13]); dest += 2;
	one_byte(dest, guid[14]); dest += 2;
	one_byte(dest, guid[15]); dest += 2;
	return dest;
}


int commandline_subst(const char *src, int devnum, int partnum, uint8_t *guid,
		      char *dest, int dest_size)
{
	static const char cros_secure[] = "cros_secure ";
	const int cros_secure_size = sizeof(cros_secure) - 1;

	/* sanity check on inputs */
	if (devnum < 0 || devnum > 25 ||
	    partnum < 1 || partnum > 99 ||
	    dest_size < 0 || dest_size > 10000) {
		return 1;
	}

	/*
	 * Condition "dst + X <= dst_end" checks if there is at least X bytes
	 * left in dst. We use X > 1 so that there is always 1 byte for '\0'
	 * after the loop.
	 *
	 * We constantly estimate how many bytes we are going to write to dst
	 * for copying characters from src or for the string replacements, and
	 * check if there is sufficient space.
	 */

	char *dest_end = dest + dest_size;

#define CHECK_SPACE(bytes) \
	if (!(dest + (bytes) <= dest_end)) { \
		printf("update_cmdline: Out of space.\n"); \
		return 1; \
	}

	// Prepend "cros_secure " to the command line.
	CHECK_SPACE(cros_secure_size);
	memcpy(dest, cros_secure, cros_secure_size);
	dest += (cros_secure_size);

	int c;

	while ((c = *src++)) {
		if (c != '%') {
			CHECK_SPACE(2);
			*dest++ = c;
			continue;
		}

		switch ((c = *src++)) {
		case '\0':
			printf("update_cmdline: Input ended with '%%'\n");
			return 1;
		case 'D':
			/*
			 * TODO: Do we have any better way to know whether %D
			 * is replaced by a letter or digits? So far, this is
			 * done by a rule of thumb that if %D is followed by a
			 * 'p' character, then it is replaced by digits.
			 */
			if (*src == 'p') {
				CHECK_SPACE(3);
				dest = itoa(dest, devnum);
			} else {
				CHECK_SPACE(2);
				*dest++ = 'a' + devnum;
			}
			break;
		case 'P':
			CHECK_SPACE(3);
			dest = itoa(dest, partnum);
			break;
		case 'U':
			/* GUID replacement needs 36 bytes */
			CHECK_SPACE(36 + 1);
			dest = emit_guid(dest, guid);
			break;
		default:
			CHECK_SPACE(3);
			*dest++ = '%';
			*dest++ = c;
			break;
		}
	}

#undef CHECK_SPACE

	*dest = '\0';
	return 0;
}
