/*
 * Copyright 2014 Google Inc.
 *
 * (C) Copyright 2008 Semihalf
 *
 * (C) Copyright 2000-2005
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * This file is based on the u-boot implementation.
 *
 * See file CREDITS for list of people who contributed to this project.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __BOOT_LEGACY_IMAGE_H__
#define __BOOT_LEGACY_IMAGE_H__

/*
 * This file provides all necesary information to descibe a legacy kernel
 * wrapped in uImage format
 */

#define IH_MAGIC	0x27051956	/* Image Magic Number		*/
#define IH_NMLEN		32	/* Image Name Length		*/

/*
 * uImage header, all data in network byte order (aka natural aka
 * bigendian).
 */
typedef struct {
	uint32_t	ih_magic;	/* Image Header Magic Number	*/
	uint32_t	ih_hcrc;	/* Image Header CRC Checksum	*/
	uint32_t	ih_time;	/* Image Creation Timestamp	*/
	uint32_t	ih_size;	/* Image Data Size		*/
	uint32_t	ih_load;	/* Data	 Load  Address		*/
	uint32_t	ih_ep;		/* Entry Point Address		*/
	uint32_t	ih_dcrc;	/* Image Data CRC Checksum	*/
	uint8_t		ih_os;		/* Operating System		*/
	uint8_t		ih_arch;	/* CPU architecture		*/
	uint8_t		ih_type;	/* Image Type			*/
	uint8_t		ih_comp;	/* Compression Type		*/
	uint8_t		ih_name[IH_NMLEN];	/* Image Name		*/
} image_header_t;

/* General information about a legacy kernel image being booted. */
typedef struct image_info {
	uint32_t       	start, end;		/* start/end of blob */
	uint32_t       	image_start;		/* start of image within blob */
	uint32_t	image_len;		/* lendth of image */
	uint32_t       	load;			/* load addr for the image */
	uint32_t	actual_size;		/* size of the loaded image */
	uint8_t		comp, type;		/* compression, type of image */
} image_info_t;

typedef struct {
	image_info_t	os;			/* os image info */
	uint32_t	ep;			/* entry point of OS */
	const char	*cmdline;

} bootm_header_t;

#define IH_COMP_NONE		0	/*  No	 Compression Used	*/
#define IH_COMP_GZIP		1	/* gzip	 Compression Used	*/
#define IH_COMP_BZIP2		2	/* bzip2 Compression Used	*/
#define IH_COMP_LZMA		3	/* lzma  Compression Used	*/
#define IH_COMP_LZO		4	/* lzo   Compression Used	*/

#define uimage_to_cpu(x)		be32toh(x)

#define image_get_hdr_b(f) \
	static inline uint8_t image_get_##f(const image_header_t *hdr) \
	{ \
		return hdr->ih_##f; \
	}
image_get_hdr_b(comp)		/* image_get_comp */
image_get_hdr_b(os)		/* image_get_os */
image_get_hdr_b(type)		/* image_get_type */

#define image_get_hdr_l(f) \
	static inline uint32_t image_get_##f(const image_header_t *hdr) \
	{ \
		return be32toh(hdr->ih_##f); \
	}

image_get_hdr_l(dcrc)		/* image_get_dcrc */
image_get_hdr_l(ep)		/* image_get_ep */
image_get_hdr_l(hcrc)		/* image_get_hcrc */
image_get_hdr_l(load)		/* image_get_load */
image_get_hdr_l(magic)		/* image_get_magic */
image_get_hdr_l(size)		/* image_get_size */

#define image_set_hdr_l(f) \
	static inline void image_set_##f(image_header_t *hdr, uint32_t val) \
	{ \
		hdr->ih_##f = htobe32(val); \
	}
image_set_hdr_l(hcrc)		/* image_set_hcrc */


#define IMAGE_FORMAT_INVALID	0x00
#define IMAGE_FORMAT_LEGACY	0x01

#define BOOTM_ERR_RESET		-1
#define BOOTM_ERR_OVERLAP	-2
#define BOOTM_ERR_UNIMPLEMENTED	-3

#endif // __BOOT_LEGACY_IMAGE_H__
