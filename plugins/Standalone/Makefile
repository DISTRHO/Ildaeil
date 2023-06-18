#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

# ---------------------------------------------------------------------------------------------------------------------
# Project name, used for binaries

NAME = Ildaeil

# ---------------------------------------------------------------------------------------------------------------------
# Do some magic

STANDALONE = true
include ../Common/Makefile.mk

# ---------------------------------------------------------------------------------------------------------------------
# Extra rules for macOS app bundle

$(TARGET_DIR)/Ildaeil.app/Contents/Info.plist: macOS/Info.plist $(TARGET_DIR)/Ildaeil.app/Contents/Resources/distrho.icns
	-@mkdir -p $(shell dirname $@)
	cp $< $@

$(TARGET_DIR)/Ildaeil.app/Contents/Resources/distrho.icns: macOS/distrho.icns
	-@mkdir -p $(shell dirname $@)
	cp $< $@

# ---------------------------------------------------------------------------------------------------------------------
# Extra rules for Windows icon

ifeq ($(WINDOWS),true)
WINDRES ?= $(subst gcc,windres,$(CC))

$(BUILD_DIR)/distrho.rc.o: win32/distrho.rc win32/distrho.ico
	-@mkdir -p "$(shell dirname $(BUILD_DIR)/$<)"
	@echo "Compiling distrho.rc"
	$(SILENT)$(WINDRES) $< -O coff -o $@
endif

# ---------------------------------------------------------------------------------------------------------------------
