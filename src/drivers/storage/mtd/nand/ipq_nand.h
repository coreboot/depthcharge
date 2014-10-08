#ifndef __DRIVERS_STORAGE_MTD_NAND_IPQ_NAND_H__
#define __DRIVERS_STORAGE_MTD_NAND_IPQ_NAND_H__

#include "drivers/storage/mtd/mtd.h"

MtdDevCtrlr *new_ipq_nand(void *ebi2nd_base);

#endif
