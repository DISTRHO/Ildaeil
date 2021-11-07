#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

# NOTE This file MUST be imported after setting `NAME`

# --------------------------------------------------------------
# Files to build

FILES_DSP = \
	IldaeilPlugin.cpp

FILES_UI = \
	IldaeilUI.cpp \
	../Common/PluginHostWindow.cpp \
	../../dpf-widgets/opengl/DearImGui.cpp

# --------------------------------------------------------------
# Carla stuff

CWD = ../../carla/source
include $(CWD)/Makefile.deps.mk

CARLA_BUILD_DIR = ../../carla/build
ifeq ($(DEBUG),true)
CARLA_BUILD_TYPE = Debug
else
CARLA_BUILD_TYPE = Release
endif

EXTRA_LIBS  = $(CARLA_BUILD_DIR)/plugin/$(CARLA_BUILD_TYPE)/carla-host-plugin.cpp.o
ifneq ($(MACOS),true)
EXTRA_LIBS += -Wl,--start-group -Wl,--whole-archive
endif
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/carla_engine_plugin.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/carla_plugin.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/native-plugins.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/audio_decoder.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/jackbridge.min.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/lilv.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/rtmempool.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/sfzero.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/water.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/zita-resampler.a
# EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/eel2.a
# EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/jsusfx.a
ifeq ($(USING_JUCE),true)
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_audio_basics.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_audio_processors.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_core.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_data_structures.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_events.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_graphics.a
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_gui_basics.a
ifeq ($(USING_JUCE_GUI_EXTRA),true)
EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/juce_gui_extra.a
endif
endif
ifneq ($(MACOS),true)
EXTRA_LIBS += -Wl,--no-whole-archive -Wl,--end-group
endif

# FIXME patch fluidsynth package
ifeq ($(WIN32),true)
STATIC_CARLA_PLUGIN_LIBS += -ldsound -lwinmm
endif

# --------------------------------------------------------------
# Do some more magic

include ../../dpf/Makefile.plugins.mk

BUILD_CXX_FLAGS += -pthread
BUILD_CXX_FLAGS += -I../Common
BUILD_CXX_FLAGS += -I../../dpf-widgets/generic
BUILD_CXX_FLAGS += -I../../dpf-widgets/opengl

BUILD_CXX_FLAGS += -DREAL_BUILD
BUILD_CXX_FLAGS += -DSTATIC_PLUGIN_TARGET
BUILD_CXX_FLAGS += -I../../carla/source/backend
BUILD_CXX_FLAGS += -I../../carla/source/includes
BUILD_CXX_FLAGS += -I../../carla/source/modules
BUILD_CXX_FLAGS += -I../../carla/source/utils

EXTRA_LIBS += $(STATIC_CARLA_PLUGIN_LIBS)

ifeq ($(MACOS),true)
$(BUILD_DIR)/../Common/PluginHostWindow.cpp.o: BUILD_CXX_FLAGS += -ObjC++
$(BUILD_DIR)/../Common/SizeUtils.cpp.o: BUILD_CXX_FLAGS += -ObjC++
endif

# TODO Find if carla is installed system-wide and use its binaries (assumed yes for now)
#      Otherwise we need to ship the bridge binaries ourselves, this is not done yet

# BUILD_CXX_FLAGS += $(shell pkg-config --cflags carla-host-plugin carla-native-plugin carla-utils)
# LINK_FLAGS      += $(shell pkg-config --libs carla-host-plugin carla-native-plugin carla-utils)

# --------------------------------------------------------------
# Enable all possible plugin types

all: jack lv2 vst2 vst3

# --------------------------------------------------------------
