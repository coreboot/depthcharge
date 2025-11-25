#ifndef __DRIVERS_SOC_QCOM_SMEM_H__
#define __DRIVERS_SOC_QCOM_SMEM_H__

#include <stdint.h>

#define SMEM_TARG_INFO_IDENTIFIER               0x49494953
#define SMEM_VERSION_ID                         0x000C0000
#define SMEM_MAJOR_VERSION_MASK                 0xFFFF0000
#define SMEM_VERSION_BOOT_OFFSET                7
#define SMEM_VERSION_INFO_SIZE                  32

typedef struct {
	void* (*alloc)(uint32_t smem_type, uint32_t buf_size);
	void* (*get_addr)(uint32_t smem_type, uint32_t *buf_size);
	int32_t (*alloc_ex)(void *params);
	int32_t (*get_addr_ex)(void *params);
} smem_funcs_type;

typedef struct {
	uint32_t initialized;
	uint32_t free_offset;
	uint32_t heap_remaining;
	uint32_t reserved;
} smem_heap_info_type;

typedef struct {
	uint32_t proc_comm[16];
	uint32_t version[32];
	smem_heap_info_type heap_info;
} smem_static_allocs_type;

typedef struct {
	uint32_t identifier;
	uint32_t smem_size;
	uint64_t smem_base_phys_addr;
	uint16_t smem_max_items;
	uint16_t smem_reserved;
} smem_targ_info_type;

typedef enum {
	SMEM_STATE_UNINITIALIZED = 0,
	SMEM_STATE_PRE_BOOT_INIT = 1,
	SMEM_STATE_BOOT_INITIALIZED = 2,
	SMEM_STATE_INITIALIZED = 3
} smem_state_type;

typedef struct {
	uint8_t *smem_base_addr;
	uint8_t *smem_phys_base_addr;
	uint32_t smem_size;
	uint32_t version;
	uint16_t max_items;
	const smem_funcs_type *funcs;
	smem_state_type state;
} smem_info_type;

typedef struct {
	uint16_t tag;
	uint16_t flags;
	uint32_t offset;
	uint32_t size;
	uint16_t reserved;
	uint16_t padding_data;
} smem_alloc_entry_type;

typedef enum {
	SMEM_MEM_FIRST = 0,
	SMEM_RESERVED_PROC_COMM = SMEM_MEM_FIRST,
	SMEM_FIRST_FIXED_BUFFER = SMEM_RESERVED_PROC_COMM,
	SMEM_HEAP_INFO,
	SMEM_ALLOCATION_TABLE,
	SMEM_VERSION_INFO,
	SMEM_HW_RESET_DETECT,
	SMEM_RESERVED_AARM_WARM_BOOT,
	SMEM_DIAG_ERR_MESSAGE,
	SMEM_SPINLOCK_ARRAY,
	SMEM_MEMORY_BARRIER_LOCATION,
	SMEM_LAST_FIXED_BUFFER = SMEM_MEMORY_BARRIER_LOCATION,
	SMEM_HW_SW_BUILD_ID = 137,
	SMEM_MEM_LAST = 497,
	SMEM_INVALID = 498
} smem_mem_type;

/* SMEM status codes */
#define SMEM_STATUS_SUCCESS          0
#define SMEM_STATUS_NOT_FOUND        1
#define SMEM_STATUS_INVALID_PARAM    2
#define SMEM_STATUS_OUT_OF_RESOURCES 3
#define SMEM_STATUS_SIZE_MISMATCH    4
#define SMEM_STATUS_FAILURE          5

/* SMEM allocation flags */
#define SMEM_ALLOC_FLAG_NONE            0x00
#define SMEM_ALLOC_FLAG_CACHED          0x01
#define SMEM_ALLOC_FLAG_PARTITION_ONLY  0x02

typedef struct {
	uint32_t nFormat;
	uint32_t eChipId;
	uint32_t nChipVersion;
	char     aBuildId[32];
	uint32_t nRawChipId;
	uint32_t nRawChipVersion;
	uint32_t ePlatformType;
	uint32_t nPlatformVersion;
	uint32_t bFusion;
	uint32_t nPlatformSubtype;
	uint32_t aPMICInfo[3 * 2];
	uint32_t nFoundryId;
	uint32_t nChipSerial;
} DalPlatformInfoSMemType;

#define SMEM_HEAP_INFO_INIT              1
#define SMEM_ALLOCATION_TABLE_OFFSET     0x0C
#define SMEM_MAX_ITEMS                   512
#define SMEM_TOC_MAX_EXCLUSIONS 4
#define SMEM_MAX_PARTITIONS 20
#define SMEM_TOC_SIZE (4 * 1024)
#define SMEM_ALLOC_HDR_CANARY 0xA5A5
#define SMEM_TOC_IDENTIFIER              0x434F5424
#define SMEM_PARTITION_HEADER_ID         0x54525024

/* SMEM spinlock IDs */
#define SMEM_SPINLOCK_SMEM_ALLOC 3

/* Helper macros */
#define ROUND_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define SMEM_PARTITION_ITEM_PADDING(cacheline) ((cacheline) ? (cacheline) : 0)

typedef struct {
	uint32_t offset;
	uint32_t size;
	uint32_t flags;
	uint16_t host0;
	uint16_t host1;
	uint32_t size_cacheline;
	uint32_t reserved[3];
	uint32_t exclusion_sizes[SMEM_TOC_MAX_EXCLUSIONS];
} smem_toc_entry_type;

typedef struct {
	uint32_t identifier;
	uint32_t version;
	uint32_t num_entries;
	uint32_t reserved[5];
	smem_toc_entry_type entry[];
} smem_toc_type;

typedef enum {
	SMEM_APPS         = 0,
	SMEM_MODEM        = 1,
	SMEM_ADSP         = 2,
	SMEM_SSC          = 3,
	SMEM_WCN          = 4,
	SMEM_CDSP         = 5,
	SMEM_RPM          = 6,
	SMEM_TZ           = 7,
	SMEM_SPSS         = 8,
	SMEM_HYP          = 9,
	SMEM_NPU          = 10,
	SMEM_SPSS_SP      = 11,
	SMEM_NSP1         = 12,
	SMEM_WPSS         = 13,
	SMEM_TME          = 14,
	SMEM_APPS_VM_LA   = 15,
	SMEM_EXT_PM       = 16,
	SMEM_GPDSP0       = 17,
	SMEM_GPDSP1       = 18,
	SMEM_SOCCP        = 19,
	SMEM_OOBSS        = 20,
	SMEM_OOBNS        = 21,
	SMEM_DCP          = 22,
	SMEM_NUM_HOSTS    = 23,

	SMEM_Q6           = SMEM_ADSP,
	SMEM_RIVA         = SMEM_WCN,
	SMEM_DSPS         = SMEM_SSC,
	SMEM_SPSS_NONSP   = SMEM_SPSS,
	SMEM_NSP          = SMEM_CDSP,
	SMEM_CMDDB        = 0xFFFD,
	SMEM_COMMON_HOST  = 0xFFFE,
	SMEM_INVALID_HOST = 0xFFFF,
} smem_host_type;

#define SMEM_THIS_HOST SMEM_APPS

/* SMEM allocation parameters structure - must be after smem_host_type */
typedef struct {
	smem_host_type remote_host;
	smem_mem_type smem_type;
	uint32_t size;
	void *buffer;
	uint32_t flags;
} smem_alloc_params_type;

typedef struct {
	uint32_t identifier;
	uint16_t host0;
	uint16_t host1;
	uint32_t size;
	uint32_t offset_free_uncached;
	uint32_t offset_free_cached;
	uint32_t reserved[3];
} smem_partition_header_type;

typedef struct {
	uint16_t canary;
	uint16_t smem_type;
	uint32_t size;
	uint16_t padding_data;
	uint16_t padding_header;
	uint32_t reserved[1];
} smem_partition_allocation_header_type;

typedef struct {
	smem_partition_header_type *header;
	uint32_t size;
	uint32_t size_cacheline;
	uint32_t offset_free_uncached;
	uint32_t offset_free_cached;
} smem_partition_info_type;


#if CONFIG(DRIVER_SOC_QCOM_SMEM)
uint32_t qcom_smem_get_chip_serial(void);
#else
static inline uint32_t qcom_smem_get_chip_serial(void) { return 0; }
#endif

#endif /* __DRIVERS_SOC_QCOM_SMEM_H__ */
