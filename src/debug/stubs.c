/*
 * Copyright 2014 Google LLC
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
 */

#include "debug/firmware_shell/common.h"
#include "drivers/ec/cros/i2c.h"

/*
 * These stubs are linked for debug-related functions in non-developer builds
 * to ensure that those features do not work. Developer builds will override
 * them with the definitions from dev.c.
 */

void dc_dev_gdb_enter(void) __attribute__((weak));
void dc_dev_gdb_enter(void) { /* do nothing */ }

void dc_dev_gdb_exit(int exit_code) __attribute__((weak));
void dc_dev_gdb_exit(int exit_code) { (void)exit_code; /* do nothing */ }

void dc_dev_netboot(void) __attribute__((weak));
void dc_dev_netboot(void) { /* do nothing */ }

void dc_dev_fastboot(void) __attribute__((weak));
void dc_dev_fastboot(void) { /* do nothing */ }

void dc_dev_add_i2c_controller_to_list(I2cOps *ops, const char *fmt, ...) __attribute__((weak));
void dc_dev_add_i2c_controller_to_list(I2cOps *ops, const char *fmt, ...) { /* do nothing */ }

void dc_dev_enter_firmware_shell(void) __attribute__((weak));
void dc_dev_enter_firmware_shell(void) { /* do nothing */ }

bool dc_dev_firmware_shell_enabled(void) __attribute__((weak));
bool dc_dev_firmware_shell_enabled(void) { return false; }
