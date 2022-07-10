#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

include dpf/Makefile.base.mk

# --------------------------------------------------------------
# Build targets

all: carla dgl plugins gen

# --------------------------------------------------------------
# Build config

PREFIX  ?= /usr/local
DESTDIR ?=

# --------------------------------------------------------------
# Carla config

CARLA_EXTRA_ARGS = CARLA_BACKEND_NAMESPACE=Ildaeil \
	CAN_GENERATE_LV2_TTL=false \
	STATIC_PLUGIN_TARGET=true \
	HAVE_FFMPEG=false \
	HAVE_FLUIDSYNTH=false \
	HAVE_PROJECTM=false \
	HAVE_ZYN_DEPS=false \
	HAVE_ZYN_UI_DEPS=false

ifneq ($(DEBUG),true)
CARLA_EXTRA_ARGS += EXTERNAL_PLUGINS=true
endif

CARLA_TARGETS = static-plugin

ifneq ($(WASM),true)
CARLA_TARGETS += bridges-plugin bridges-ui
endif

# --------------------------------------------------------------
# Check for X11+OpenGL dependencies

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

# --------------------------------------------------------------

carla: dgl
	$(MAKE) $(CARLA_EXTRA_ARGS) -C carla $(CARLA_TARGETS)

dgl:
	$(MAKE) -C dpf/dgl opengl

plugins: carla dgl
	$(MAKE) $(CARLA_EXTRA_ARGS) all -C plugins/FX
ifneq ($(WASM),true)
	$(MAKE) $(CARLA_EXTRA_ARGS) all -C plugins/MIDI
	$(MAKE) $(CARLA_EXTRA_ARGS) all -C plugins/Synth
endif

ifneq ($(CROSS_COMPILING),true)
gen: plugins dpf/utils/lv2_ttl_generator
	@$(CURDIR)/dpf/utils/generate-ttl.sh

dpf/utils/lv2_ttl_generator:
	$(MAKE) -C dpf/utils/lv2-ttl-generator
else
gen:
endif

# --------------------------------------------------------------

install:
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-FX.lv2
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-MIDI.lv2
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-Synth.lv2
	install -d $(DESTDIR)$(PREFIX)/lib/vst/Ildaeil-FX.vst
	install -d $(DESTDIR)$(PREFIX)/lib/vst/Ildaeil-MIDI.vst
	install -d $(DESTDIR)$(PREFIX)/lib/vst/Ildaeil-Synth.vst
	install -d $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-FX.vst3/Contents
	install -d $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-MIDI.vst3/Contents
	install -d $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-Synth.vst3/Contents

	install -m 644 bin/Ildaeil-FX.lv2/*    $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-FX.lv2/
	install -m 644 bin/Ildaeil-MIDI.lv2/*  $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-MIDI.lv2/
	install -m 644 bin/Ildaeil-Synth.lv2/* $(DESTDIR)$(PREFIX)/lib/lv2/Ildaeil-Synth.lv2/

	install -m 644 bin/Ildaeil-FX.vst/*    $(DESTDIR)$(PREFIX)/lib/vst/Ildaeil-FX.vst/
	install -m 644 bin/Ildaeil-MIDI.vst/*  $(DESTDIR)$(PREFIX)/lib/vst/Ildaeil-MIDI.vst/
	install -m 644 bin/Ildaeil-Synth.vst/* $(DESTDIR)$(PREFIX)/lib/vst/Ildaeil-Synth.vst/

	cp -rL bin/Ildaeil-FX.vst3/Contents/*-*    $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-FX.vst3/Contents/
	cp -rL bin/Ildaeil-MIDI.vst3/Contents/*-*  $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-MIDI.vst3/Contents/
	cp -rL bin/Ildaeil-Synth.vst3/Contents/*-* $(DESTDIR)$(PREFIX)/lib/vst3/Ildaeil-Synth.vst3/Contents/

# --------------------------------------------------------------

clean:
	$(MAKE) distclean -C carla
	$(MAKE) clean -C dpf/dgl
	$(MAKE) clean -C dpf/utils/lv2-ttl-generator
	$(MAKE) clean -C plugins/FX
	$(MAKE) clean -C plugins/MIDI
	$(MAKE) clean -C plugins/Synth
	rm -rf bin build
	rm -f dpf-widgets/opengl/*.d
	rm -f dpf-widgets/opengl/*.o

# --------------------------------------------------------------

.PHONY: carla plugins
