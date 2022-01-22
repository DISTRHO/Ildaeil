#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

include dpf/Makefile.base.mk

all: carla dgl plugins gen

# --------------------------------------------------------------
# Carla config

CARLA_EXTRA_ARGS = \
	HAVE_FFMPEG=false \
	HAVE_FLUIDSYNTH=false \
	HAVE_PROJECTM=false \
	HAVE_ZYN_DEPS=false \
	HAVE_ZYN_UI_DEPS=false

ifneq ($(DEBUG),true)
CARLA_EXTRA_ARGS += EXTERNAL_PLUGINS=true
endif

CARLA_EXTRA_ARGS += USING_JUCE=false
CARLA_EXTRA_ARGS += USING_JUCE_GUI_EXTRA=false

# --------------------------------------------------------------

carla:
	$(MAKE) bridges-plugin bridges-ui static-plugin -C carla $(CARLA_EXTRA_ARGS) \
		CAN_GENERATE_LV2_TTL=false \
		STATIC_PLUGIN_TARGET=true

dgl:
	$(MAKE) -C dpf/dgl opengl

plugins: carla dgl
	$(MAKE) all -C plugins/FX
	$(MAKE) all -C plugins/MIDI
	$(MAKE) all -C plugins/Synth

ifneq ($(CROSS_COMPILING),true)
gen: plugins dpf/utils/lv2_ttl_generator
	@$(CURDIR)/dpf/utils/generate-ttl.sh

dpf/utils/lv2_ttl_generator:
	$(MAKE) -C dpf/utils/lv2-ttl-generator
else
gen:
endif

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
