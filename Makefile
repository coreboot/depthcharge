##
## This file is part of the depthcharge project.
##
## Copyright (C) 2008 Advanced Micro Devices, Inc.
## Copyright (C) 2008 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright 2012 Google Inc.
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
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

.SECONDEXPANSION:

export src ?= $(shell pwd)
export srck ?= $(src)/util/kconfig
export obj ?= $(src)/build
export objk ?= $(obj)/util/kconfig

export KERNELVERSION      := 0.1.0

export KCONFIG_AUTOHEADER := $(obj)/config.h
export KCONFIG_AUTOCONFIG := $(obj)/auto.conf
export KCONFIG_DEPENDENCIES := $(obj)/auto.conf.cmd
export KCONFIG_SPLITCONFIG := $(obj)/config
export KCONFIG_TRISTATE := $(obj)/tristate.conf
export KCONFIG_NEGATIVES := 1
DOTCONFIG ?= .config
export KCONFIG_CONFIG = $(DOTCONFIG)


CONFIG_SHELL := sh
ifdef BOARD
KBUILD_DEFCONFIG := $(src)/board/$(BOARD)/defconfig
endif
UNAME_RELEASE := $(shell uname -r)
HAVE_DOTCONFIG := $(wildcard $(DOTCONFIG))
MAKEFLAGS += -rR --no-print-directory

# Make is silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q:=@
.SILENT:
endif

HOSTCC = gcc
HOSTCXX = g++
HOSTCFLAGS := -I$(srck) -I$(objk)
HOSTCXXFLAGS := -I$(srck) -I$(objk)

ifdef LIBPAYLOAD_DIR
# libpayload's xcompile script checks for this config flag to decide which
# compiler to use
CONFIG_LP_COMPILER_GCC=y
include $(LIBPAYLOAD_DIR)/libpayload.xcompile
endif
LIBPAYLOAD_DIR ?= ../libpayload/install/libpayload
LZMA := lzma

ifeq ($(strip $(HAVE_DOTCONFIG)),)

all: help

else

include $(src)/.config

ifeq ($(CONFIG_ARCH_X86),y)
ARCH = x86
endif
ifeq ($(CONFIG_ARCH_ARM),y)
ARCH = arm
endif
ifeq ($(CONFIG_ARCH_ARM_V8),y)
ARCH = arm64
ARCH_DIR = arm
else
ARCH_DIR = $(ARCH)
endif
ifeq ($(CONFIG_ARCH_MIPS),y)
ARCH = mips
ARCH_DIR = mips
endif

ifeq ($(CONFIG_LDSCRIPT_ARCH),y)
LDSCRIPT := $(src)/src/arch/$(ARCH_DIR)/depthcharge.ldscript
ifeq ("$(wildcard $(LDSCRIPT))","")
$(error $(LDSCRIPT): no such file.)
endif
else
LDSCRIPT := $(src)/src/image/depthcharge.ldscript
endif

ARCH_TO_TOOLCHAIN_x86    := i386
ARCH_TO_TOOLCHAIN_arm    := arm
ARCH_TO_TOOLCHAIN_arm64  := arm64
ARCH_TO_TOOLCHAIN_mips   := mipsel

toolchain := $(ARCH_TO_TOOLCHAIN_$(ARCH))

# libpayload's xcompile adapted the coreboot naming scheme, which is different
# in some places. If the names above don't work, use another set.
ifeq ($(CC_$(toolchain)),)
new_toolchain_name_i386 := x86_32
new_toolchain_name_mipsel := mips

toolchain := $(new_toolchain_name_$(toolchain))
endif

CC:=$(firstword $(CC_$(toolchain)))
XCC := CC=$(CC) $(LIBPAYLOAD_DIR)/bin/lpgcc
AS = $(LIBPAYLOAD_DIR)/bin/lpas
OBJCOPY ?= $(OBJCOPY_$(toolchain))
STRIP ?= $(STRIP_$(toolchain))

include $(src)/src/arch/$(ARCH_DIR)/build_vars

INCLUDES = -I$(obj) -I$(src)/src/ -I$(src)/src/arch/$(ARCH_DIR)/includes/ \
	-I$(VB_SOURCE)/firmware/include
ABI_FLAGS := $(ARCH_ABI_FLAGS) -ffreestanding -fno-builtin \
	-fno-stack-protector -fomit-frame-pointer
LINK_FLAGS = $(ARCH_LINK_FLAGS) $(ABI_FLAGS) -fuse-ld=bfd \
	-Wl,-T,$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$@.map
CFLAGS := $(ARCH_CFLAGS) -Wall -Werror $(INCLUDES) -std=gnu99 \
	$(ABI_FLAGS) -ffunction-sections -fdata-sections -ggdb3

ifneq ($(SOURCE_DEBUG),)
CFLAGS += -O0 -g
else
CFLAGS += -Os
endif

all:
	@echo  'You must specify one of the following targets to build:'
	@echo
	@echo  '  depthcharge_unified	- Build unified depthcharge (with no'
	@echo  '  			  firmware selection, for use with EFS)'
	@echo  '  depthcharge_ro_rw	- Build depthcharge RO and RW (with'
	@echo  '  			  old trampoline and VbSelectFirmware)'
	@echo  '  netboot_unified	- Build netboot binary'
	@echo  '  fastboot_bin		- Build fastboot binary'
	@echo  '  dev_unified		- Build unified developer binary (with'
	@echo  '  			  (EFS) vboot, netboot and gdb)'
	@echo  '  dev_ro_rw		- Build RO/RW developer binary (with
	@echo  '			  (legacy) vboot, netboot and gdb)'
	@echo  '  dts                   - Preprocess fmap.dts file'
	@echo  '  clean			- Delete final output binaries'
	@echo  '  distclean		- Delete whole build directory'

endif

$(KCONFIG_AUTOHEADER): $(KCONFIG_CONFIG)
	$(MAKE) oldconfig

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
$(eval $(call evaluate_subdirs))

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
$(shell mkdir -p $(obj) $(KCONFIG_SPLITCONFIG) $(objk)/lxdialog $(additional-dirs) $(alldirs))
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

foreach-src=$(foreach file,$($(1)-srcs),$(eval $(call $(1)-objs_$(subst .,,$(suffix $(file)))_template,$(subst src/,,$(basename $(file))))))
$(eval $(foreach class,$(classes),$(call foreach-src,$(class))))

# Kconfig options intended for the linker.
$(foreach option,$(link_config_options), \
	$(eval LINK_FLAGS += -Wl,--defsym=$(option)=$$(CONFIG_$(option))))

DEPENDENCIES = $(allobjs:.o=.d)
-include $(DEPENDENCIES)

DTS_DIR := $(src)/board/$(BOARD)
DTS_CPPFLAGS := -x assembler-with-cpp
$(obj)/%.dts : $(DTS_DIR)/%.dts $(KCONFIG_AUTOHEADER)
	$(HOSTCC) -E -P $(DTS_CPPFLAGS) --include $(KCONFIG_AUTOHEADER) $< -o $@
dts: $(obj)/fmap.dts

prepare:
	$(Q)mkdir -p $(obj)/util/kconfig/lxdialog

clean:
	$(Q)rm -rf $(obj)/*.elf $(obj)/*.o

distclean: clean
	$(Q)rm -rf $(obj)
	$(Q)rm -f .config .config.old ..config.tmp .kconfig.d .tmpconfig*

include util/kconfig/Makefile

.PHONY: $(PHONY) prepare clean distclean

