#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

# --------------------------------------------------------------
# Project name, used for binaries

NAME = Ildaeil-FX

# --------------------------------------------------------------
# Files to build (DPF stuff)

FILES_DSP = \
	IldaeilPlugin.cpp

FILES_UI = \
	IldaeilUI.cpp

# --------------------------------------------------------------
# Do some magic

include ../../dpf/Makefile.plugins.mk

# --------------------------------------------------------------
# Enable all possible plugin types

all: jack lv2 vst2 vst3

# --------------------------------------------------------------
