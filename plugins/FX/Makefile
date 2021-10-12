#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

# --------------------------------------------------------------
# Project name, used for binaries

NAME = Ildaeil-FX

# --------------------------------------------------------------
# Files to build

FILES_DSP = \
	IldaeilPlugin.cpp

FILES_UI = \
	IldaeilUI.cpp \
	../Common/SizeUtils.cpp \
	../../dpf-widgets/opengl/DearImGui.cpp

# --------------------------------------------------------------
# Carla stuff

CARLA_BUILD_DIR = ../../carla/build
ifeq ($(DEBUG),true)
CARLA_BUILD_TYPE = Debug
else
CARLA_BUILD_TYPE = Release
endif

EXTRA_LIBS += $(CARLA_BUILD_DIR)/plugin/$(CARLA_BUILD_TYPE)/carla-host-plugin.cpp.o
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/carla_engine_plugin.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/carla_plugin.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/native-plugins.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/audio_decoder.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/jackbridge.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/lilv.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/rtmempool.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/sfzero.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/water.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/zita-resampler.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_audio_basics.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_audio_processors.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_core.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_data_structures.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_events.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_graphics.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_gui_basics.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_gui_extra.a

# --------------------------------------------------------------
# Do some magic

include ../../dpf/Makefile.plugins.mk

BUILD_CXX_FLAGS += -pthread
BUILD_CXX_FLAGS += -I../Common
BUILD_CXX_FLAGS += -I../../dpf-widgets/generic
BUILD_CXX_FLAGS += -I../../dpf-widgets/opengl

BUILD_CXX_FLAGS += -DREAL_BUILD
BUILD_CXX_FLAGS += -I../../carla/source/backend
BUILD_CXX_FLAGS += -I../../carla/source/includes

ifeq ($(MACOS),true)
$(BUILD_DIR)/../Common/SizeUtils.cpp.o: BUILD_CXX_FLAGS += -ObjC++
LINK_FLAGS += -framework AppKit
LINK_FLAGS += -framework Accelerate
LINK_FLAGS += -framework AudioToolbox
LINK_FLAGS += -framework CoreFoundation
LINK_FLAGS += -framework AppKit
LINK_FLAGS += -framework AudioUnit
LINK_FLAGS += -framework CoreAudio
LINK_FLAGS += -framework CoreAudioKit
LINK_FLAGS += -framework Carbon
LINK_FLAGS += -framework QuartzCore
LINK_FLAGS += -framework IOKit
else ifeq ($(WIN32),true)
else ifeq ($(LINUX),true)
LINK_FLAGS += $(shell pkg-config --libs fluidsynth freetype2)
LINK_FLAGS += -ldl -lrt
LINK_FLAGS += -lmagic
endif

# BUILD_CXX_FLAGS += $(shell pkg-config --cflags carla-host-plugin carla-native-plugin carla-utils)
# LINK_FLAGS      += $(shell pkg-config --libs carla-host-plugin carla-native-plugin carla-utils)

# --------------------------------------------------------------
# Enable all possible plugin types

all: jack lv2 vst2 vst3

# --------------------------------------------------------------
