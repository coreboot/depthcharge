#ifndef __BOOT_ZIMAGE_BOOTPARAM_H__
#define __BOOT_ZIMAGE_BOOTPARAM_H__

#include <stdint.h>

#include "boot/zimage/apm_bios.h"
#include "boot/zimage/e820.h"
#include "boot/zimage/edd.h"
#include "boot/zimage/edid.h"
#include "boot/zimage/ist.h"
#include "boot/zimage/screen_info.h"

/* setup data types */
#define SETUP_NONE			0
#define SETUP_E820_EXT			1
#define SETUP_DTB			2

/* extensible setup data list node */
struct setup_data {
	u64 next;
	u32 type;
	u32 len;
	u8 data[0];
};

struct setup_header {
	u8	setup_sects;
	u16	root_flags;
	u32	syssize;
	u16	ram_size;
#define RAMDISK_IMAGE_START_MASK	0x07FF
#define RAMDISK_PROMPT_FLAG		0x8000
#define RAMDISK_LOAD_FLAG		0x4000
	u16	vid_mode;
	u16	root_dev;
	u16	boot_flag;
	u16	jump;
	u32	header;
	u16	version;
	u32	realmode_swtch;
	u16	start_sys;
	u16	kernel_version;
	u8	type_of_loader;
	u8	loadflags;
#define LOADED_HIGH	(1<<0)
#define QUIET_FLAG	(1<<5)
#define KEEP_SEGMENTS	(1<<6)
#define CAN_USE_HEAP	(1<<7)
	u16	setup_move_size;
	u32	code32_start;
	u32	ramdisk_image;
	u32	ramdisk_size;
	u32	bootsect_kludge;
	u16	heap_end_ptr;
	u8	ext_loader_ver;
	u8	ext_loader_type;
	u32	cmd_line_ptr;
	u32	initrd_addr_max;
	u32	kernel_alignment;
	u8	relocatable_kernel;
	u8	_pad2[3];
	u32	cmdline_size;
	u32	hardware_subarch;
	u64	hardware_subarch_data;
	u32	payload_offset;
	u32	payload_length;
	u64	setup_data;
} __attribute__((packed));

struct sys_desc_table {
	u16 length;
	u8  table[14];
};

/* Gleaned from OFW's set-parameters in cpu/x86/pc/linux.fth */
struct olpc_ofw_header {
	u32 ofw_magic;	/* OFW signature */
	u32 ofw_version;
	u32 cif_handler;	/* callback into OFW */
	u32 irq_desc_table;
} __attribute__((packed));

struct efi_info {
	u32 efi_loader_signature;
	u32 efi_systab;
	u32 efi_memdesc_size;
	u32 efi_memdesc_version;
	u32 efi_memmap;
	u32 efi_memmap_size;
	u32 efi_systab_hi;
	u32 efi_memmap_hi;
};

/* The so-called "zeropage" */
struct boot_params {
	struct screen_info screen_info;			/* 0x000 */
	struct apm_bios_info apm_bios_info;		/* 0x040 */
	u8  _pad2[4];					/* 0x054 */
	u64  tboot_addr;				/* 0x058 */
	struct ist_info ist_info;			/* 0x060 */
	u8  _pad3[16];					/* 0x070 */
	u8  hd0_info[16];	/* obsolete! */		/* 0x080 */
	u8  hd1_info[16];	/* obsolete! */		/* 0x090 */
	struct sys_desc_table sys_desc_table;		/* 0x0a0 */
	struct olpc_ofw_header olpc_ofw_header;		/* 0x0b0 */
	u8  _pad4[128];					/* 0x0c0 */
	struct edid_info edid_info;			/* 0x140 */
	struct efi_info efi_info;			/* 0x1c0 */
	u32 alt_mem_k;					/* 0x1e0 */
	u32 scratch;		/* Scratch field! */	/* 0x1e4 */
	u8  e820_entries;				/* 0x1e8 */
	u8  eddbuf_entries;				/* 0x1e9 */
	u8  edd_mbr_sig_buf_entries;			/* 0x1ea */
	u8  _pad6[6];					/* 0x1eb */
	struct setup_header hdr;    /* setup header */	/* 0x1f1 */
	u8  _pad7[0x290-0x1f1-sizeof(struct setup_header)];
	u32 edd_mbr_sig_buffer[EDD_MBR_SIG_MAX];	/* 0x290 */
	struct e820entry e820_map[E820MAX];		/* 0x2d0 */
	u8  _pad8[48];					/* 0xcd0 */
	struct edd_info eddbuf[EDDMAXNR];		/* 0xd00 */
	u8  _pad9[276];					/* 0xeec */
} __attribute__((packed));

enum {
	X86_SUBARCH_PC = 0,
	X86_SUBARCH_LGUEST,
	X86_SUBARCH_XEN,
	X86_SUBARCH_MRST,
	X86_SUBARCH_CE4100,
	X86_NR_SUBARCHS,
};



#endif /* __BOOT_ZIMAGE_BOOTPARAM_H__ */
