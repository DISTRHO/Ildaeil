#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

include dpf/Makefile.base.mk

all: carla dgl plugins gen

# --------------------------------------------------------------

carla:
	$(MAKE) -C carla plugin \
		CAN_GENERATE_LV2_TTL=false \
		HAVE_ALSA=false \
		HAVE_DGL=false \
		HAVE_HYLIA=false \
		HAVE_JACK=false \
		HAVE_LIBLO=false \
		HAVE_PYQT=false \
		HAVE_QT4=false \
		HAVE_QT5=false \
		HAVE_QT5PKG=false \
		HAVE_PULSEAUDIO=false \
		USING_JUCE_AUDIO_DEVICES=false \
		USING_RTAUDIO=false

dgl:
	$(MAKE) -C dpf/dgl opengl

plugins: carla dgl
	$(MAKE) all -C plugins/FX
	$(MAKE) all -C plugins/MIDI
	$(MAKE) all -C plugins/Synth

ifneq ($(CROSS_COMPILING),true)
gen: plugins dpf/utils/lv2_ttl_generator
	@$(CURDIR)/dpf/utils/generate-ttl.sh
ifeq ($(MACOS),true)
	@$(CURDIR)/dpf/utils/generate-vst-bundles.sh
endif

dpf/utils/lv2_ttl_generator:
	$(MAKE) -C dpf/utils/lv2-ttl-generator
else
gen:
endif

# --------------------------------------------------------------

clean:
	$(MAKE) clean -C carla
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
