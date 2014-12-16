
#ifndef __DRIVERS_STORAGE_MTD_NAND_SPI_NAND_H__
#define __DRIVERS_STORAGE_MTD_NAND_SPI_NAND_H__

#include "drivers/storage/mtd/mtd.h"
#include "drivers/bus/spi/spi.h"

MtdDevCtrlr *new_spi_nand(SpiOps *spi);

#endif
