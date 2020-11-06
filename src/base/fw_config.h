// SPDX-License-Identifier: GPL-2.0
/* Copyright 2020 Google LLC. */

#ifndef _BASE_FW_CONFIG_H_
#define _BASE_FW_CONFIG_H_

#include "static_fw_config.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * struct fw_config - Firmware configuration field and option.
 * @field_name: Name of the field that this option belongs to.
 * @option_name: Name of the option within this field.
 * @mask: Bitmask of the field.
 * @value: Value of the option within the mask.
 */
struct fw_config {
	const char *field_name;
	const char *option_name;
	uint64_t mask;
	uint64_t value;
};

/* Generate a pointer to a compound literal of the fw_config structure. */
#define FW_CONFIG(__field, __option)	(&(const struct fw_config) {	       \
	.field_name = FW_CONFIG_FIELD_##__field##_NAME,			       \
	.option_name = FW_CONFIG_FIELD_##__field##_OPTION_##__option##_NAME,   \
	.mask = FW_CONFIG_FIELD_##__field##_MASK,			       \
	.value = FW_CONFIG_FIELD_##__field##_OPTION_##__option##_VALUE	       \
})

/**
 * fw_config_probe() - Check if field and option matches.
 * @match: Structure containing field and option to probe.
 *
 * Return %true if match is found, %false if match is not found.
 */
bool fw_config_probe(const struct fw_config *match);

/**
 * fw_config_is_provisioned() - Check if FW_CONFIG has been provisioned
 *
 * Return %true if FW_CONFIG value was located, %false if not.
 */
bool fw_config_is_provisioned(void);

#endif /* _BASE_FW_CONFIG_H_ */
