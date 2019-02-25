/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC.
 */

#ifndef __DRIVERS_TPM_CR50_SWITCHES_H__
#define __DRIVERS_TPM_CR50_SWITCHES_H__

#include "drivers/gpio/gpio.h"
#include "drivers/tpm/tpm.h"

typedef struct Cr50Switch {
	TpmOps *tpm;
	GpioOps ops;
} Cr50Switch;

/**
 * Returns method that reports the state of the recovery button from the Cr50.
 *
 * @param tpm   TPM operations initialized for the board.
 *
 * @return pointer to method that reports if a recovery button press is
 *         currently detected by the Cr50, 0 if not detected (-1 if
 *         detection failed).
 */
Cr50Switch *new_cr50_rec_switch(TpmOps *tpm);

/**
 * Returns method that reports the state of the power button from the Cr50.
 * This status is also used as a trusted report of physical presence.
 *
 * @param tpm   TPM operations initialized for the board.
 *
 * @return pointer to method that reports if a recent power press
 *         was detected by the Cr50, 0 if not detected (-1 if detection failed).
 */
Cr50Switch *new_cr50_power_switch(TpmOps *tpm);

#endif /* __DRIVERS_TPM_CR50_SWITCHES_H__ */
