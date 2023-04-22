#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

include dpf/Makefile.base.mk

# also set in:
# plugins/Common/IldaeilPlugin.cpp `getVersion`
VERSION = 1.2

# ---------------------------------------------------------------------------------------------------------------------
# Build targets

all: carla dgl plugins gen

# ---------------------------------------------------------------------------------------------------------------------
# Build config

PREFIX  ?= /usr/local
DESTDIR ?=

# ---------------------------------------------------------------------------------------------------------------------
# Carla config

ifeq ($(WASM),true)
USE_SYSTEM_CARLA_BINS = true
endif

CARLA_EXTRA_ARGS = CARLA_BACKEND_NAMESPACE=Ildaeil \
	CAN_GENERATE_LV2_TTL=false \
	CUSTOM_DPF_PATH=$(CURDIR)/dpf \
	DGL_NAMESPACE=IldaeilDGL \
	STATIC_PLUGIN_TARGET=true \
	USING_CUSTOM_DPF=true \
	HAVE_FFMPEG=false \
	HAVE_FLUIDSYNTH=false \
	HAVE_PROJECTM=false \
	HAVE_ZYN_DEPS=false \
	HAVE_ZYN_UI_DEPS=false

ifneq ($(DEBUG),true)
CARLA_EXTRA_ARGS += EXTERNAL_PLUGINS=true
endif

CARLA_TARGETS = static-plugin

ifneq ($(USE_SYSTEM_CARLA_BINS),true)
CARLA_TARGETS += bridges-plugin bridges-ui

ifeq ($(CARLA_EXTRA_TARGETS),true)

# native 32bit bridge
ifeq ($(WINDOWS)$(CPU_X86_64),truetrue)
CARLA_TARGETS += win32
else ifneq ($(MACOS)$(WINDOWS),true)
CARLA_TARGETS += posix32
endif

# wine bridges
ifeq ($(LINUX)$(CPU_X86_64),truetrue)
CARLA_TARGETS += wine32 wine64
else ifeq ($(LINUX)$(CPU_I386),truetrue)
CARLA_TARGETS += wine32
endif

endif # CARLA_EXTRA_TARGETS
endif # USE_SYSTEM_CARLA_BINS

# --------------------------------------------------------------
# DGL config

DGL_EXTRA_ARGS = \
	DISTRHO_NAMESPACE=IldaeilDISTRHO \
	DGL_NAMESPACE=IldaeilDGL

# ---------------------------------------------------------------------------------------------------------------------
# DPF bundled plugins

ifneq ($(MACOS),true)
ILDAEIL_FX_ARGS = VST2_FILENAME=Ildaeil.vst/Ildaeil-FX$(LIB_EXT) CLAP_FILENAME=Ildaeil.clap/Ildaeil-FX.clap
ILDAEIL_MIDI_ARGS = VST2_FILENAME=Ildaeil.vst/Ildaeil-MIDI$(LIB_EXT) CLAP_FILENAME=Ildaeil.clap/Ildaeil-MIDI.clap
ILDAEIL_SYNTH_ARGS = VST2_FILENAME=Ildaeil.vst/Ildaeil-Synth$(LIB_EXT) CLAP_FILENAME=Ildaeil.clap/Ildaeil-Synth.clap
endif

# ---------------------------------------------------------------------------------------------------------------------
# Check for OpenGL + X11 dependencies

ifneq ($(HAIKU_OR_MACOS_OR_WASM_OR_WINDOWS),true)

ifneq ($(HAVE_OPENGL),true)
$(error OpenGL dependency not installed/available)
endif
ifneq ($(HAVE_X11),true)
$(error X11 dependency not installed/available)
endif
ifneq ($(HAVE_XCURSOR),true)
$(warning Xcursor dependency not installed/available)
endif
ifneq ($(HAVE_XEXT),true)
$(warning Xext dependency not installed/available)
endif
ifneq ($(HAVE_XRANDR),true)
$(warning Xrandr dependency not installed/available)
endif

endif

# ---------------------------------------------------------------------------------------------------------------------

carla: dgl
	$(MAKE) $(CARLA_EXTRA_ARGS) -C carla $(CARLA_TARGETS)

extra-bins:
	$(MAKE) $(CARLA_EXTRA_ARGS) $(DGL_EXTRA_ARGS) $(ILDAEIL_FX_ARGS) carlabins -C plugins/FX
ifneq ($(WASM),true)
	$(MAKE) $(CARLA_EXTRA_ARGS) $(DGL_EXTRA_ARGS) $(ILDAEIL_MIDI_ARGS) carlabins -C plugins/MIDI
	$(MAKE) $(CARLA_EXTRA_ARGS) $(DGL_EXTRA_ARGS) $(ILDAEIL_SYNTH_ARGS) carlabins -C plugins/Synth
endif

extra-posix32:
	$(MAKE) $(CARLA_EXTRA_ARGS) -C carla posix32

extra-win32:
	$(MAKE) $(CARLA_EXTRA_ARGS) -C carla win32 AR=i686-w64-mingw32-ar CC=i686-w64-mingw32-gcc CXX=i686-w64-mingw32-g++

extra-win64:
	$(MAKE) $(CARLA_EXTRA_ARGS) -C carla win64 AR=x86_64-w64-mingw32-ar CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++

extra-wine32:
	$(MAKE) $(CARLA_EXTRA_ARGS) -C carla wine32

extra-wine64:
	$(MAKE) $(CARLA_EXTRA_ARGS) -C carla wine64

dgl:
	$(MAKE) $(DGL_EXTRA_ARGS) -C dpf/dgl opengl

plugins: carla dgl
	$(MAKE) $(CARLA_EXTRA_ARGS) $(ILDAEIL_FX_ARGS) all -C plugins/FX
ifneq ($(WASM),true)
	$(MAKE) $(CARLA_EXTRA_ARGS) $(ILDAEIL_MIDI_ARGS) all -C plugins/MIDI
	$(MAKE) $(CARLA_EXTRA_ARGS) $(ILDAEIL_SYNTH_ARGS) all -C plugins/Synth
endif

ifneq ($(CROSS_COMPILING),true)
gen: plugins dpf/utils/lv2_ttl_generator
	@$(CURDIR)/dpf/utils/generate-ttl.sh

dpf/utils/lv2_ttl_generator:
	$(MAKE) -C dpf/utils/lv2-ttl-generator
else
gen:
endif

# ---------------------------------------------------------------------------------------------------------------------

install:
	install -d $(DESTDIR)$(PREFIX)/lib/clap/Ildaeil.clap
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-FX.lv2
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-MIDI.lv2
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-Synth.lv2
	install -d $(DESTDIR)$(PREFIX)/lib/vst/Ildaeil.vst
	install -d $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-FX.vst3/$(VST3_BINARY_DIR)
	install -d $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-Synth.vst3/$(VST3_BINARY_DIR)

	install -m 644 bin/Ildaeil-FX.lv2/*    $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-FX.lv2/
	install -m 644 bin/Ildaeil-MIDI.lv2/*  $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-MIDI.lv2/
	install -m 644 bin/Ildaeil-Synth.lv2/* $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-Synth.lv2/

	install -m 644 bin/Ildaeil-FX.vst3/$(VST3_BINARY_DIR)/*    $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-FX.vst3/$(VST3_BINARY_DIR)/
	install -m 644 bin/Ildaeil-Synth.vst3/$(VST3_BINARY_DIR)/* $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-Synth.vst3/$(VST3_BINARY_DIR)/

ifeq ($(MACOS),true)
else
	install -m 644 bin/Ildaeil.clap/*      $(DESTDIR)$(PREFIX)/lib/clap/Ildaeil.clap/
	install -m 644 bin/Ildaeil.vst/*       $(DESTDIR)$(PREFIX)/lib/vst/Ildaeil.vst/
endif

# ---------------------------------------------------------------------------------------------------------------------

clean:
	$(MAKE) $(CARLA_EXTRA_ARGS) distclean -C carla
	$(MAKE) $(CARLA_EXTRA_ARGS) $(ILDAEIL_FX_ARGS) clean -C plugins/FX
	$(MAKE) $(CARLA_EXTRA_ARGS) $(ILDAEIL_MIDI_ARGS) clean -C plugins/MIDI
	$(MAKE) $(CARLA_EXTRA_ARGS) $(ILDAEIL_SYNTH_ARGS) clean -C plugins/Synth
	$(MAKE) clean -C dpf/dgl
	$(MAKE) clean -C dpf/utils/lv2-ttl-generator
	rm -rf bin build
	rm -f dpf-widgets/opengl/*.d
	rm -f dpf-widgets/opengl/*.o

# ---------------------------------------------------------------------------------------------------------------------
# tarball target, generating release source-code tarballs ready for packaging

TAR_ARGS = \
	--exclude=".appveyor*" \
	--exclude=".ci*" \
	--exclude=".clang*" \
	--exclude=".drone*" \
	--exclude=".editor*" \
	--exclude=".git*" \
	--exclude="*.kdev*" \
	--exclude=".travis*" \
	--exclude=".vscode*" \
	--exclude="*.d" \
	--exclude="*.o" \
	--exclude=bin \
	--exclude=build \
	--exclude=carla/data \
	--exclude=carla/source/frontend \
	--exclude=carla/source/interposer \
	--exclude=carla/source/libjack \
	--exclude=carla/source/native-plugins/resources \
	--exclude=carla/source/rest \
	--exclude=carla/source/tests.old \
	--exclude=carla/source/theme \
	--exclude=carla/resources \
	--exclude=dpf/build \
	--exclude=dpf/cmake \
	--exclude=dpf/examples \
	--exclude=dpf/lac \
	--exclude=dpf/tests \
	--exclude=dpf-widgets/generic \
	--exclude=dpf-widgets/opengl/Blendish* \
	--exclude=dpf-widgets/opengl/DearImGuiColorTextEditor* \
	--exclude=dpf-widgets/opengl/Quantum* \
	--exclude=dpf-widgets/tests \
	--transform='s,^\.\.,-.-.,' \
	--transform='s,^\.,Ildaeil-$(VERSION),' \
	--transform='s,^-\.-\.,..,' \

tarball:
	rm -f ../Ildaeil-src-$(VERSION).tar.xz
	tar -c --lzma $(TAR_ARGS) -f ../Ildaeil-src-$(VERSION).tar.xz .

# ---------------------------------------------------------------------------------------------------------------------

.PHONY: carla plugins
