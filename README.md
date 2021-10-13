# DISTRHO Ildaeil

DISTRHO Ildaeil is mini-plugin host working as a plugin, allowing one-to-one plugin format reusage.  
Load a VST2 plugin inside a LV2 host, or an LV2 plugin on a VST3 host, etc.

THIS IS CURRENTLY A WORK IN PROGRESS RESEARCH PROJECT.

It is not known yet how well this can work, mostly testing waters here.  
Currently the plugin will link against an internal [Carla](https://github.com/falkTX/Carla) for the plugin host part, embeding UIs if possible.  
Audio, MIDI and Time information works, but there is no latency, parameters or state handled right now.  
Also, only LV2 hosting is enabled at the moment.

![screenshot](Screenshot.png "Ildaeil")

## Goals

The current formats Ildaeil can work as are:

- JACK/Standalone
- LV2
- VST2
- VST3

And it can load the following plugin formats:
- LV2

Later on, in theory, should be able to load the following plugin formats:

- JACK (applications as plugins, Linux only)
- LADSPA
- DSSI
- LV2
- VST2
- VST3
- AU (macOS only)

Additionally the following files could eventually be loaded:

- Audio files (synced to host transport)
- MIDI files (aligned to real/wall-clock time, synced to host transport)
- SF2/3 files (through internal FluidSynth)
- SFZ files (through internal SFZero)

Ildaeil basically works as a mini-wrapper around Carla, leveraging it for all its host support.

The name comes from the korean 일대일, which means "one to one".
