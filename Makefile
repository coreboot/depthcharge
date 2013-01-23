##
## This file is part of the depthcharge project.
##
## Copyright (C) 2008 Advanced Micro Devices, Inc.
## Copyright (C) 2008 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (c) 2012 The Chromium OS Authors.
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

export src := $(shell pwd)
export srck := $(src)/util/kconfig
export obj := $(src)/build
export objk := $(obj)/util/kconfig

export KERNELVERSION      := 0.1.0
export KCONFIG_AUTOHEADER := $(obj)/config.h
export KCONFIG_AUTOCONFIG := $(obj)/auto.conf

CONFIG_SHELL := sh
ifdef BOARD
KBUILD_DEFCONFIG := $(src)/board/$(BOARD)/defconfig
endif
UNAME_RELEASE := $(shell uname -r)
HAVE_DOTCONFIG := $(wildcard .config)
MAKEFLAGS += -rR --no-print-directory

# Make is silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q := @
endif

HOSTCC = gcc
HOSTCXX = g++
HOSTCFLAGS := -I$(srck) -I$(objk)
HOSTCXXFLAGS := -I$(srck) -I$(objk)

LIBPAYLOAD_DIR := ../libpayload/install/libpayload
XCC := CC=$(CC) $(LIBPAYLOAD_DIR)/bin/lpgcc
AS = $(LIBPAYLOAD_DIR)/bin/lpas
OBJCOPY ?= $(CROSS_COMPILE)objcopy
STRIP ?= $(CROSS_COMPILE)strip
LZMA := lzma

LDSCRIPT := $(src)/src/image/depthcharge.ldscript




ifeq ($(strip $(HAVE_DOTCONFIG)),)

all: config

else

include $(src)/.config

ifeq ($(CONFIG_ARCH_X86),y)
ARCH_DIR = x86
endif
ifeq ($(CONFIG_ARCH_ARM),y)
ARCH_DIR = arm
endif

include $(src)/src/arch/$(ARCH_DIR)/build_vars

INCLUDES = -Ibuild -I$(src)/src/ -I$(src)/src/arch/$(ARCH_DIR)/includes/ \
	-I$(VB_INC_DIR) -I$(VB_INC_DIR)/arch/$(VB_FIRMWARE_ARCH)/
ABI_FLAGS := $(ARCH_ABI_FLAGS) -ffreestanding -fno-builtin \
	-fno-stack-protector -fomit-frame-pointer
LINK_FLAGS := $(ARCH_LINK_FLAGS) $(ABI_FLAGS) -fuse-ld=bfd \
	-Wl,-T,$(LDSCRIPT) -Wl,--gc-sections
CFLAGS := $(ARCH_CFLAGS) -Wall -Werror -Os $(INCLUDES) -std=gnu99 \
	$(ABI_FLAGS) -ffunction-sections

all: real-target

endif



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
$(shell mkdir -p $(obj) $(objk)/lxdialog $(additional-dirs) $(alldirs))
endif

# macro to define template macros that are used by use_template macro
define create_cc_template
# $1 obj class
# $2 source suffix (c, S)
# $3 additional compiler flags
# $4 additional dependencies
ifn$(EMPTY)def $(1)-objs_$(2)_template
de$(EMPTY)fine $(1)-objs_$(2)_template
$(obj)/$$(1).$(1).o: src/$$(1).$(2) $(obj)/config.h $(4)
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

prepare:
	$(Q)mkdir -p $(obj)/util/kconfig/lxdialog

clean:
	$(Q)rm -rf build/*.elf build/*.o

distclean: clean
	$(Q)rm -rf build
	$(Q)rm -f .config .config.old ..config.tmp .kconfig.d .tmpconfig*

include util/kconfig/Makefile

.PHONY: $(PHONY) prepare clean distclean

