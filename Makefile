##
## This file is part of the depthcharge project.
##
## Copyright (C) 2008 Advanced Micro Devices, Inc.
## Copyright (C) 2008 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright 2012 Google LLC
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; version 2 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##

.SECONDEXPANSION:

src ?= $(shell pwd)
export override src := $(abspath $(src))

obj ?= $(src)/build
export override obj := $(abspath $(obj))

KCONFIG_AUTOHEADER := $(obj)/config.h
KCONFIG_CONFIG_OUT := $(obj)/config
KCONFIG_FILE := $(src)/Kconfig
DOTCONFIG ?= .config
export KCONFIG_CONFIG = $(DOTCONFIG)


CONFIG_SHELL := sh
ifdef BOARD
KBUILD_DEFCONFIG := $(src)/board/$(BOARD)/defconfig
endif
UNAME_RELEASE := $(shell uname -r)
MAKEFLAGS += -rR --no-print-directory

# Make is silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q:=@
.SILENT:
endif

HOSTCC = gcc
HOSTCXX = g++
HOSTAR = ar
HOSTAS = as
HOSTCFLAGS :=
HOSTCXXFLAGS :=
HOSTARFLAGS :=
HOSTASFLAGS :=

UNIT_TEST:=
ifneq ($(filter %-test %-tests %coverage-report coverage-report%, \
	$(MAKECMDGOALS)),)
ifneq ($(filter-out %-test %-tests %screenshot \
	%coverage-report coverage-report%, \
	$(MAKECMDGOALS)),)
$(error Cannot mix unit-tests targets with other targets)
endif
UNIT_TEST:=1
endif

SCREENSHOT :=
ifneq ($(filter %screenshot,$(MAKECMDGOALS)),)
ifneq ($(filter-out %screenshot, $(MAKECMDGOALS)),)
$(error Cannot mix screenshot targets with other targets)
endif
SCREENSHOT := 1
endif

MAKE_CONFIG :=
ifneq ($(filter %config,$(MAKECMDGOALS)),)
ifneq ($(filter-out %config, $(MAKECMDGOALS)),)
$(error Cannot mix config targets with other targets)
endif
MAKE_CONFIG := 1
endif

LIBPAYLOAD_DIR ?= ../libpayload/install/libpayload

# Run genconfig before including the config
$(shell [ -d "$(obj)" ] || mkdir -p "$(obj)")

run_kconfig_tool = KCONFIG_CONFIG="$(KCONFIG_CONFIG)" $(1)

ifeq ($(UNIT_TEST)$(SCREENSHOT)$(MAKE_CONFIG),)
_ := $(shell $(call run_kconfig_tool, \
	genconfig --header-path "$(KCONFIG_AUTOHEADER).unused" \
	--config-out "$(KCONFIG_CONFIG_OUT)" "$(KCONFIG_FILE)"))
_ := $(shell $(call run_kconfig_tool, \
	"$(src)/util/autoheader.py" --header-path "$(KCONFIG_AUTOHEADER)" \
	"$(KCONFIG_FILE)"))

include $(KCONFIG_CONFIG_OUT)
include $(LIBPAYLOAD_DIR)/libpayload.config
include $(LIBPAYLOAD_DIR)/libpayload.xcompile

# Function to check architecture configuration and set ARCH and ARCH_DIR variables
define define_arch
ifeq ($($(1)),y)
ifneq ($($(2)),y)
$$(error $(1) is set but $(2) is not in $(LIBPAYLOAD_DIR))
endif
ARCH = $(3)
ARCH_DIR = $(4)
endif
endef

# Verify configuration for each architecture and select corresponding ARCH
# and ARCH_DIR variables
$(eval $(call define_arch,CONFIG_ARCH_X86_32,CONFIG_LP_ARCH_X86_32,x86,x86))
$(eval $(call define_arch,CONFIG_ARCH_ARM_V7,CONFIG_LP_ARCH_ARM,arm,arm))
$(eval $(call define_arch,CONFIG_ARCH_ARM_V8,CONFIG_LP_ARCH_ARM64,arm64,arm))
$(eval $(call define_arch,CONFIG_ARCH_X86_64,CONFIG_LP_ARCH_X86_64,x86_64,x86))

ifndef ARCH_DIR
$(error ARCH_DIR is not defined)
endif

ARCH_TO_TOOLCHAIN_x86    := i386
ARCH_TO_TOOLCHAIN_arm    := arm
ARCH_TO_TOOLCHAIN_arm64  := arm64

toolchain := $(ARCH_TO_TOOLCHAIN_$(ARCH))

# libpayload's xcompile adapted the coreboot naming scheme, which is different
# in some places. If the names above don't work, use another set.
ifeq ($(CC_$(toolchain)),)
new_toolchain_name_i386 := x86_32

toolchain := $(new_toolchain_name_$(toolchain))
endif

CC:=$(firstword $(CC_$(toolchain)))
XCC := CC=$(CC) $(abspath $(LIBPAYLOAD_DIR))/bin/lpgcc
AS = $(LIBPAYLOAD_DIR)/bin/lpas
GCOV := $(GCOV_$(toolchain))
OBJCOPY ?= $(OBJCOPY_$(toolchain))
STRIP ?= $(STRIP_$(toolchain))

include $(src)/src/arch/$(ARCH_DIR)/build_vars

INCLUDES = -I$(obj) -I$(src)/src/ -I$(src)/src/arch/$(ARCH_DIR)/includes/ \
	-I$(LIBPAYLOAD_DIR)/vboot/firmware/include \
	-include $(LIBPAYLOAD_DIR)/include/kconfig.h \
	-include $(KCONFIG_AUTOHEADER)

ifndef EC_HEADERS
$(error Set EC_HEADERS to point to the EC headers directory)
else
INCLUDES += -I$(EC_HEADERS)
endif

ifdef COREBOOT_HEADERS
INCLUDES += -I$(COREBOOT_HEADERS)
endif

ABI_FLAGS := $(ARCH_ABI_FLAGS) -ffreestanding -fno-builtin \
	-fno-stack-protector -fomit-frame-pointer
LINK_FLAGS = $(ARCH_LINK_FLAGS) $(ABI_FLAGS) -fuse-ld=bfd \
	-Wl,-T,$(LDSCRIPT_OBJ) -Wl,--gc-sections -Wl,-Map=$@.map
CFLAGS := $(ARCH_CFLAGS) -Wall -Werror -Wstrict-prototypes -Wshadow \
	-Wno-address-of-packed-member \
	$(INCLUDES) -std=gnu99 $(ABI_FLAGS) -ffunction-sections -fdata-sections -ggdb3

ifneq ($(SOURCE_DEBUG),)
CFLAGS += -Og
else
CFLAGS += -Os
endif

ifeq ($(COV),1)
CFLAGS += --coverage
LINK_FLAGS += --coverage

# Initial zero coverage data
coverage-init: depthcharge
	lcov -o $(obj)/init.info -c -d $(obj) \
		--exclude '$(src)/tests/*' --include '$(src)/src/*' \
		-b $(src) --gcov-tool $(GCOV) -i
endif

endif # ifeq ($(UNIT_TEST)$(SCREENSHOT)$(MAKE_CONFIG),)

all:
	@echo  'You must specify one of the following targets to build:'
	@echo
	@echo  '  depthcharge		- Build default binary'
	@echo  '  netboot		- Build netboot binary'
	@echo  '  fastboot		- Build fastboot binary'
	@echo  '  dev			- Build developer binary'
	@echo
	@echo  '  clean			- Delete final output binaries'
	@echo  '  distclean		- Delete whole build directory'

strip_quotes = $(subst ",,$(subst \",,$(1)))

# Add a new class of source/object files to the build system
add-class= \
	$(eval $(1)-srcs:=) \
	$(eval $(1)-objs:=) \
	$(eval classes+=$(1))

# Special classes are managed types with special behaviour
# On parse time, for each entry in variable $(1)-y
# a handler $(1)-handler is executed with the arguments:
# * $(1): directory the parser is in
# * $(2): current entry
add-special-class= \
	$(eval $(1):=) \
	$(eval special-classes+=$(1))

# Clean -y variables, include Makefile.inc
# Add paths to files in X-y to X-srcs
# Add subdirs-y to subdirs
includemakefiles= \
	$(foreach class,classes subdirs $(classes) $(special-classes), $(eval $(class)-y:=)) \
	$(eval -include $(1)) \
	$(foreach class,$(classes-y), $(call add-class,$(class))) \
	$(foreach class,$(classes), \
		$(eval $(class)-srcs+= \
			$$(subst $(src)/,, \
			$$(abspath $$(subst $(dir $(1))/,/,$$(addprefix $(dir $(1)),$$($(class)-y))))))) \
	$(foreach special,$(special-classes), \
		$(foreach item,$($(special)-y), $(call $(special)-handler,$(dir $(1)),$(item)))) \
	$(eval subdirs+=$$(subst $(CURDIR)/,,$$(abspath $$(addprefix $(dir $(1)),$$(subdirs-y)))))

# For each path in $(subdirs) call includemakefiles
# Repeat until subdirs is empty
evaluate_subdirs= \
	$(eval cursubdirs:=$(subdirs)) \
	$(eval subdirs:=) \
	$(foreach dir,$(cursubdirs), \
		$(eval $(call includemakefiles,$(dir)/Makefile.inc))) \
	$(if $(subdirs),$(eval $(call evaluate_subdirs)))

# collect all object files eligible for building
subdirs:=$(src)

ifneq ($(UNIT_TEST),)
include tests/Makefile.inc
else ifneq ($(SCREENSHOT),)
include screenshot/Makefile.inc
else
$(eval $(call evaluate_subdirs))
endif

# Eliminate duplicate mentions of source files in a class
$(foreach class,$(classes),$(eval $(class)-srcs:=$(sort $($(class)-srcs))))

src-to-obj=$(addsuffix .$(1).o, $(basename $(patsubst src/%, $(obj)/%, $($(1)-srcs))))
$(foreach class,$(classes),$(eval $(class)-objs:=$(call src-to-obj,$(class))))

allsrcs:=$(foreach var, $(addsuffix -srcs,$(classes)), $($(var)))
allobjs:=$(foreach var, $(addsuffix -objs,$(classes)), $($(var)))
alldirs:=$(sort $(abspath $(dir $(allobjs))))

printall:
	@$(foreach class,$(classes),echo $(class)-objs:=$($(class)-objs); )
	@echo alldirs:=$(alldirs)
	@echo allsrcs=$(allsrcs)
	@echo DEPENDENCIES=$(DEPENDENCIES)
	@echo LIBGCC_FILE_NAME=$(LIBGCC_FILE_NAME)
	@$(foreach class,$(special-classes),echo $(class):='$($(class))'; )

ifndef NOMKDIR
ifneq ($(alldirs),)
$(shell mkdir -p $(alldirs))
endif
endif

# macro to define template macros that are used by use_template macro
define create_cc_template
# $1 obj class
# $2 source suffix (c, S)
# $3 additional compiler flags
# $4 additional dependencies
ifn$(EMPTY)def $(1)-objs_$(2)_template
de$(EMPTY)fine $(1)-objs_$(2)_template
$(obj)/$$(1).$(1).o: src/$$(1).$(2) $(KCONFIG_AUTOHEADER) $(4)
	@printf "    CC         $$$$(subst $$$$(obj)/,,$$$$(@))\n"
	$(Q)$(XCC) $(3) -MMD $$$$(CFLAGS) -c -o $$$$@ $$$$<
en$(EMPTY)def
end$(EMPTY)if
endef

filetypes-of-class=$(subst .,,$(sort $(suffix $($(1)-srcs))))
$(foreach class,$(classes), \
	$(foreach type,$(call filetypes-of-class,$(class)), \
		$(eval $(call create_cc_template,$(class),$(type),$($(class)-$(type)-ccopts),$($(class)-$(type)-deps)))))

foreach-src=$(foreach file,$($(1)-srcs),$(eval $(call $(1)-objs_$(subst .,,$(suffix $(file)))_template,$(patsubst src/%,%,$(basename $(file))))))
$(eval $(foreach class,$(classes),$(call foreach-src,$(class))))

DEPENDENCIES = $(allobjs:.o=.d)
-include $(DEPENDENCIES)

configuration_uis := menuconfig guiconfig
PHONY += $(configuration_uis)
$(configuration_uis):
	$(Q)$(call run_kconfig_tool, $@ "$(KCONFIG_FILE)")

PHONY += defconfig
defconfig:
	$(Q)$(call run_kconfig_tool, defconfig --kconfig "$(KCONFIG_FILE)" \
		"$(KBUILD_DEFCONFIG)")

clean:
	$(Q)rm -rf $(obj)/*.elf $(obj)/*.o

distclean: clean
	$(Q)rm -rf $(obj)
	$(Q)rm -f .config .config.old ..config.tmp .kconfig.d .tmpconfig*

.PHONY: $(PHONY) clean distclean
