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
	IldaeilUI.cpp \
	../../dpf-widgets/opengl/DearImGui.cpp

# --------------------------------------------------------------
# Do some magic

include ../../dpf/Makefile.plugins.mk

BUILD_CXX_FLAGS += -I../Common
BUILD_CXX_FLAGS += -I../../dpf-widgets/generic
BUILD_CXX_FLAGS += -I../../dpf-widgets/opengl

BUILD_CXX_FLAGS += $(shell pkg-config --cflags carla-host-plugin carla-native-plugin carla-utils)
LINK_FLAGS      += $(shell pkg-config --libs carla-host-plugin carla-native-plugin carla-utils)

# --------------------------------------------------------------
# Enable all possible plugin types

all: jack lv2 vst2 vst3

# --------------------------------------------------------------
