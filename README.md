# DISTRHO Ildaeil

DISTRHO Ildaeil is mini-plugin host working as a plugin, allowing one-to-one plugin format reusage.
Load a VST2 plugin inside a LV2 host, or an LV2 plugin on a VST3 host, etc.

The current formats Ildaeil can work as are:
- JACK/Standalone
- LV2
- VST2
- VST3

And it can load the following plugin formats:
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
