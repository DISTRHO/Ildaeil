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

ifneq ($(DEBUG),true)
EXTERNAL_PLUGINS = true
endif

CWD = ../../carla/source
include $(CWD)/Makefile.deps.mk

CARLA_BUILD_DIR = ../../carla/build
ifeq ($(DEBUG),true)
CARLA_BUILD_TYPE = Debug
else
CARLA_BUILD_TYPE = Release
endif

CARLA_EXTRA_LIBS  = $(CARLA_BUILD_DIR)/plugin/$(CARLA_BUILD_TYPE)/carla-host-plugin.cpp.o
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/carla_engine_plugin.a
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/carla_plugin.a
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/native-plugins.a
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/audio_decoder.a
ifneq ($(WASM),true)
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/dgl.a
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/jackbridge.min.a
endif
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/lilv.a
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/rtmempool.a
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/sfzero.a
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/water.a
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/ysfx.a
CARLA_EXTRA_LIBS += $(CARLA_BUILD_DIR)/modules/$(CARLA_BUILD_TYPE)/zita-resampler.a

# FIXME
ifeq ($(WASM),true)
STATIC_CARLA_PLUGIN_LIBS = -lsndfile -lopus -lFLAC -lvorbisenc -lvorbis -logg -lm
endif

EXTRA_DEPENDENCIES = $(CARLA_EXTRA_LIBS)
EXTRA_LIBS = $(CARLA_EXTRA_LIBS) $(STATIC_CARLA_PLUGIN_LIBS)

# --------------------------------------------------------------
# Do some more magic

USE_CLAP_BUNDLE = true
USE_VST2_BUNDLE = true
include ../../dpf/Makefile.plugins.mk

ifeq ($(WASM),true)
# used for testing
LINK_FLAGS += -sALLOW_MEMORY_GROWTH
LINK_FLAGS += --use-preload-plugins
# LINK_FLAGS += --preload-file=foolme.mp3
# LINK_FLAGS += --preload-file=furelise.mid
# LINK_FLAGS += --preload-file=./jsfx
LINK_FLAGS += --preload-file=./lv2
# LINK_FLAGS += --shell-file=../Cardinal/src/emscripten/shell.html
else ifneq ($(HAIKU),true)
BUILD_CXX_FLAGS += -pthread
endif

BUILD_CXX_FLAGS += -I../Common
BUILD_CXX_FLAGS += -I../../dpf-widgets/generic
BUILD_CXX_FLAGS += -I../../dpf-widgets/opengl

BUILD_CXX_FLAGS += -DCARLA_BACKEND_NAMESPACE=Ildaeil
BUILD_CXX_FLAGS += -DREAL_BUILD
BUILD_CXX_FLAGS += -DSTATIC_PLUGIN_TARGET
BUILD_CXX_FLAGS += -I../../carla/source/backend
BUILD_CXX_FLAGS += -I../../carla/source/includes
BUILD_CXX_FLAGS += -I../../carla/source/modules
BUILD_CXX_FLAGS += -I../../carla/source/utils

ifeq ($(MACOS),true)
$(BUILD_DIR)/../Common/PluginHostWindow.cpp.o: BUILD_CXX_FLAGS += -ObjC++
$(BUILD_DIR)/../Common/SizeUtils.cpp.o: BUILD_CXX_FLAGS += -ObjC++
endif

# --------------------------------------------------------------
# Enable all possible plugin types

ifeq ($(WASM),true)

TARGETS_EXTRA = jack

else

TARGETS_BASE = lv2 vst2 clap
TARGETS_EXTRA = jack carlabins

# VST3 does not do MIDI filter plugins, by design
ifneq ($(NAME),Ildaeil-MIDI)
TARGETS_BASE += vst3
endif

endif

all: $(TARGETS_BASE) $(TARGETS_EXTRA)

# --------------------------------------------------------------
# special step for carla binaries

ifneq ($(USE_SYSTEM_CARLA_BINS),true)

CARLA_BINARIES  = $(CURDIR)/../../carla/bin/carla-bridge-native$(APP_EXT)
CARLA_BINARIES += $(CURDIR)/../../carla/bin/carla-bridge-lv2-gtk2$(APP_EXT)
CARLA_BINARIES += $(CURDIR)/../../carla/bin/carla-bridge-lv2-gtk3$(APP_EXT)

ifeq ($(CARLA_EXTRA_TARGETS),true)

# 32bit bridge
ifeq ($(CPU_X86_64),true)
ifeq ($(WINDOWS),true)
CARLA_BINARIES += $(CURDIR)/../../carla/bin/carla-bridge-win32$(APP_EXT)
else ifneq ($(MACOS),true)
CARLA_BINARIES += $(CURDIR)/../../carla/bin/carla-bridge-posix32$(APP_EXT)
endif
endif

# Windows bridges
ifeq ($(CPU_I386_OR_X86_64)$(LINUX),truetrue)
CARLA_BINARIES += $(CURDIR)/../../carla/bin/carla-bridge-win32.exe
CARLA_BINARIES += $(CURDIR)/../../carla/bin/jackbridge-wine32.dll
ifeq ($(CPU_X86_64),true)
CARLA_BINARIES += $(CURDIR)/../../carla/bin/carla-bridge-win64.exe
CARLA_BINARIES += $(CURDIR)/../../carla/bin/jackbridge-wine64.dll
endif
endif

endif # CARLA_EXTRA_TARGETS

carlabins: $(TARGETS_BASE)
	install -m 755 $(CARLA_BINARIES) $(shell dirname $(lv2))
	install -m 755 $(CARLA_BINARIES) $(shell dirname $(vst2))
	install -m 755 $(CARLA_BINARIES) $(shell dirname $(clap))
ifneq ($(NAME),Ildaeil-MIDI)
	install -m 755 $(CARLA_BINARIES) $(shell dirname $(vst3))
endif

else # USE_SYSTEM_CARLA_BINS

carlabins:

endif # USE_SYSTEM_CARLA_BINS

# --------------------------------------------------------------
