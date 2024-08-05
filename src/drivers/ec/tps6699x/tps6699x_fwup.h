/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2024 Google LLC.  */

#ifndef __DRIVERS_EC_TPS6699X_TPS6699X_FWUP_H_
#define __DRIVERS_EC_TPS6699X_TPS6699X_FWUP_H_

#include "tps6699x.h"

/**
 * @brief Performs the TI PDC FW update procedure. Expects that the caller
 *        has already configured the EC I2C tunnel.
 *
 * @param me Chip object
 * @return int 0 on success
 */
int tps6699x_perform_fw_update(Tps6699x *me);

#endif /* __DRIVERS_EC_TPS6699X_TPS6699X_FWUP_H_ */
