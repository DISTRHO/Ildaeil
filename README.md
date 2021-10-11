# DISTRHO Ildaeil

DISTRHO Ildaeil is mini-plugin host working as a plugin, allowing one-to-one plugin format reusage.  
Load a VST2 plugin inside a LV2 host, or an LV2 plugin on a VST3 host, etc.

THIS IS CURRENTLY A WORK IN PROGRESS RESEARCH PROJECT.

It is not known yet how well this can work, mostly testing waters here.  
Currently the plugin will link against carla shared libraries for the plugin host part, embeding UIs if possible.  
Audio and time information works, but there is no latency, MIDI, parameters or state handled right now.  
Also, only LV2 hosting is enabled at the moment.

The project's goals follow

## Goals

The current formats Ildaeil can work as are:
- JACK/Standalone
- LV2
- VST2
- VST3

And it can (in theory, later on) load the following plugin formats:
- JACK (applications as plugins, Linux only)
- LADSPA
- DSSI
- LV2
- VST2
- VST3
- AU (macOS only)

Additionally the following files can be loaded:
- Audio files (synced to host transport)
- MIDI files (aligned to real/wall-clock time, synced to host transport)
- SF2/3 files (through internal FluidSynth)
- SFZ files (through internal SFZero)

Ildaeil basically works as a mini-wrapper around Carla, leveraging it for all its host support.

The name comes from the korean 일대일, which means "one to one".
