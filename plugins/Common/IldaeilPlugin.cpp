/*
 * DISTRHO Ildaeil Plugin
 * Copyright (C) 2021 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE file.
 */

#include "CarlaNativePlugin.h"

#include "DistrhoPlugin.hpp"

START_NAMESPACE_DISTRHO

// -----------------------------------------------------------------------------------------------------------

using namespace CarlaBackend;

static uint32_t host_get_buffer_size(NativeHostHandle);
static double host_get_sample_rate(NativeHostHandle);
static bool host_is_offline(NativeHostHandle);
static const NativeTimeInfo* host_get_time_info(NativeHostHandle handle);
static bool host_write_midi_event(NativeHostHandle handle, const NativeMidiEvent* event);
static intptr_t host_dispatcher(NativeHostHandle handle, NativeHostDispatcherOpcode opcode, int32_t index, intptr_t value, void* ptr, float opt);

// -----------------------------------------------------------------------------------------------------------

class IldaeilPlugin : public Plugin
{
public:
    const NativePluginDescriptor* fCarlaPluginDescriptor;
    NativePluginHandle fCarlaPluginHandle;

    NativeHostDescriptor fCarlaHostDescriptor;
    CarlaHostHandle fCarlaHostHandle;

#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
    static constexpr const uint kMaxMidiEventCount = 512;
    NativeMidiEvent* fMidiEvents;
    uint32_t fMidiEventCount;
    float* fDummyBuffer;
    float* fDummyBuffers[2];
#endif

    mutable NativeTimeInfo fCarlaTimeInfo;

    void* fUI;

    IldaeilPlugin()
        : Plugin(0, 0, 0),
          fCarlaPluginDescriptor(nullptr),
          fCarlaPluginHandle(nullptr),
          fCarlaHostHandle(nullptr),
#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
          fMidiEvents(nullptr),
          fMidiEventCount(0),
          fDummyBuffer(nullptr),
#endif
          fUI(nullptr)
    {
        fCarlaPluginDescriptor = carla_get_native_rack_plugin();
        DISTRHO_SAFE_ASSERT_RETURN(fCarlaPluginDescriptor != nullptr,);

        memset(&fCarlaHostDescriptor, 0, sizeof(fCarlaHostDescriptor));
        memset(&fCarlaTimeInfo, 0, sizeof(fCarlaTimeInfo));

        fCarlaHostDescriptor.handle = this;
        fCarlaHostDescriptor.resourceDir = carla_get_library_folder();
        fCarlaHostDescriptor.uiName = "Ildaeil";
        fCarlaHostDescriptor.uiParentId = 0;

        fCarlaHostDescriptor.get_buffer_size = host_get_buffer_size;
        fCarlaHostDescriptor.get_sample_rate = host_get_sample_rate;
        fCarlaHostDescriptor.is_offline = host_is_offline;

        fCarlaHostDescriptor.get_time_info = host_get_time_info;
        fCarlaHostDescriptor.write_midi_event = host_write_midi_event;
        fCarlaHostDescriptor.ui_parameter_changed = nullptr;
        fCarlaHostDescriptor.ui_midi_program_changed = nullptr;
        fCarlaHostDescriptor.ui_custom_data_changed = nullptr;
        fCarlaHostDescriptor.ui_closed = nullptr;
        fCarlaHostDescriptor.ui_open_file = nullptr;
        fCarlaHostDescriptor.ui_save_file = nullptr;
        fCarlaHostDescriptor.dispatcher = host_dispatcher;

        fCarlaPluginHandle = fCarlaPluginDescriptor->instantiate(&fCarlaHostDescriptor);
        DISTRHO_SAFE_ASSERT_RETURN(fCarlaPluginHandle != nullptr,);

        fCarlaHostHandle = carla_create_native_plugin_host_handle(fCarlaPluginDescriptor, fCarlaPluginHandle);
        DISTRHO_SAFE_ASSERT_RETURN(fCarlaHostHandle != nullptr,);

        carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_BINARIES, 0, "/usr/lib/carla");
        carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_RESOURCES, 0, "/usr/share/carla/resources");

#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
        fMidiEvents = new NativeMidiEvent[kMaxMidiEventCount];
        // create dummy buffers
        bufferSizeChanged(getBufferSize());
#endif
    }

    ~IldaeilPlugin() override
    {
        if (fCarlaHostHandle != nullptr)
        {
            carla_host_handle_free(fCarlaHostHandle);
#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
            delete[] fMidiEvents;
            delete[] fDummyBuffer;
#endif
        }

        if (fCarlaPluginHandle != nullptr)
            fCarlaPluginDescriptor->cleanup(fCarlaPluginHandle);
    }

    const NativeTimeInfo* hostGetTimeInfo() const noexcept
    {
        const TimePosition& timePos(getTimePosition());

        fCarlaTimeInfo.playing = timePos.playing;
        fCarlaTimeInfo.frame = timePos.frame;
        fCarlaTimeInfo.bbt.valid = timePos.bbt.valid;
        fCarlaTimeInfo.bbt.bar = timePos.bbt.bar;
        fCarlaTimeInfo.bbt.beat = timePos.bbt.beat;
        fCarlaTimeInfo.bbt.tick = timePos.bbt.tick;
        fCarlaTimeInfo.bbt.barStartTick = timePos.bbt.barStartTick;
        fCarlaTimeInfo.bbt.beatsPerBar = timePos.bbt.beatsPerBar;
        fCarlaTimeInfo.bbt.beatType = timePos.bbt.beatType;
        fCarlaTimeInfo.bbt.ticksPerBeat = timePos.bbt.ticksPerBeat;
        fCarlaTimeInfo.bbt.beatsPerMinute = timePos.bbt.beatsPerMinute;

        return &fCarlaTimeInfo;
    }

#if DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
    bool hostWriteMidiEvent(const NativeMidiEvent* const event)
    {
        MidiEvent midiEvent;
        midiEvent.frame = event->time;
        midiEvent.size = event->size;
        midiEvent.dataExt = nullptr;

        uint32_t i = 0;
        for (; i < event->size; ++i)
            midiEvent.data[i] = event->data[i];
        for (; i < MidiEvent::kDataSize; ++i)
            midiEvent.data[i] = 0;

        return writeMidiEvent(midiEvent);
    }
#endif

    void hostResizeUI(const uint width, const uint height)
    {
        DISTRHO_SAFE_ASSERT_RETURN(fUI != nullptr,);

        d_stdout("asking to resizing ui to %u %u - I SAY NO", width, height);
        // fUI->setSize(width, height);
    }

protected:
   /* --------------------------------------------------------------------------------------------------------
    * Information */

   /**
      Get the plugin label.
      A plugin label follows the same rules as Parameter::symbol, with the exception that it can start with numbers.
    */
    const char* getLabel() const override
    {
#if DISTRHO_PLUGIN_IS_SYNTH
        return "IldaeilSynth";
#elif DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
        return "IldaeilMIDI";
#else
        return "IldaeilFX";
#endif
    }

   /**
      Get an extensive comment/description about the plugin.
    */
    const char* getDescription() const override
    {
        return "Ildaeil is a mini-plugin host working as a plugin, allowing one-to-one plugin format reusage.";
    }

   /**
      Get the plugin author/maker.
    */
    const char* getMaker() const override
    {
        return "DISTRHO";
    }

   /**
      Get the plugin homepage.
    */
    const char* getHomePage() const override
    {
        return "https://github.com/DISTRHO/Ildaeil";
    }

   /**
      Get the plugin license name (a single line of text).
      For commercial plugins this should return some short copyright information.
    */
    const char* getLicense() const override
    {
        return "GPLv2+";
    }

   /**
      Get the plugin version, in hexadecimal.
    */
    uint32_t getVersion() const override
    {
        return d_version(1, 0, 0);
    }

   /**
      Get the plugin unique Id.
      This value is used by LADSPA, DSSI and VST plugin formats.
    */
    int64_t getUniqueId() const override
    {
#if DISTRHO_PLUGIN_IS_SYNTH
        return d_cconst('d', 'I', 'l', 'S');
#elif DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
        return d_cconst('d', 'I', 'l', 'M');
#else
        return d_cconst('d', 'I', 'l', 'F');
#endif
    }

   /* --------------------------------------------------------------------------------------------------------
    * Init */

   /* --------------------------------------------------------------------------------------------------------
    * Internal data */

   /* --------------------------------------------------------------------------------------------------------
    * Process */

    void activate() override
    {
        if (fCarlaPluginHandle != nullptr)
            fCarlaPluginDescriptor->activate(fCarlaPluginHandle);
    }

    void deactivate() override
    {
        if (fCarlaPluginHandle != nullptr)
            fCarlaPluginDescriptor->deactivate(fCarlaPluginHandle);
    }

#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
    void run(const float** inputs, float** outputs, uint32_t frames,
             const MidiEvent* dpfMidiEvents, uint32_t dpfMidiEventCount) override
#else
    void run(const float** inputs, float** outputs, uint32_t frames) override
#endif
    {
        if (fCarlaPluginHandle != nullptr)
        {
#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
            uint32_t midiEventCount = 0;
            for (uint32_t i=0; i < dpfMidiEventCount; ++i)
            {
                const MidiEvent& dpfMidiEvent(dpfMidiEvents[i]);
                if (dpfMidiEvent.size > 4)
                    continue;

                NativeMidiEvent& midiEvent(fMidiEvents[midiEventCount]);

                midiEvent.time = dpfMidiEvent.frame;
                midiEvent.port = 0;
                midiEvent.size = dpfMidiEvent.size;
                std::memcpy(midiEvent.data, dpfMidiEvent.data, midiEvent.size);

                if (++midiEventCount == kMaxMidiEventCount)
                    break;
            }
# if DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
            fCarlaPluginDescriptor->process(fCarlaPluginHandle, fDummyBuffers, fDummyBuffers, frames,
                                            fMidiEvents, midiEventCount);
            // unused
            (void)outputs;
# else
            fCarlaPluginDescriptor->process(fCarlaPluginHandle, fDummyBuffers, outputs, frames,
                                            fMidiEvents, midiEventCount);
# endif
            // unused
            (void)inputs;
#else
            fCarlaPluginDescriptor->process(fCarlaPluginHandle, (float**)inputs, outputs, frames, nullptr, 0);
#endif
        }
        else
        {
            std::memset(outputs[0], 0, sizeof(float)*frames);
            std::memset(outputs[1], 0, sizeof(float)*frames);
        }
    }

#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
    void bufferSizeChanged(const uint32_t newBufferSize) override
    {
        delete[] fDummyBuffer;
        fDummyBuffer = new float[newBufferSize];
        fDummyBuffers[0] = fDummyBuffer;
        fDummyBuffers[1] = fDummyBuffer;
        std::memset(fDummyBuffer, 0, sizeof(float)*newBufferSize);
    }
#endif
    // -------------------------------------------------------------------------------------------------------

   /**
      Set our plugin class as non-copyable and add a leak detector just in case.
    */
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IldaeilPlugin)
};

// -----------------------------------------------------------------------------------------------------------

static uint32_t host_get_buffer_size(const NativeHostHandle handle)
{
    return static_cast<IldaeilPlugin*>(handle)->getBufferSize();
}

static double host_get_sample_rate(const NativeHostHandle handle)
{
    return static_cast<IldaeilPlugin*>(handle)->getSampleRate();
}

static bool host_is_offline(NativeHostHandle)
{
    return false;
}

static const NativeTimeInfo* host_get_time_info(const NativeHostHandle handle)
{
    return static_cast<IldaeilPlugin*>(handle)->hostGetTimeInfo();
}

static bool host_write_midi_event(const NativeHostHandle handle, const NativeMidiEvent* const event)
{
#if DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
    return static_cast<IldaeilPlugin*>(handle)->hostWriteMidiEvent(event);
#else
    return handle != nullptr && event != nullptr && false;
#endif
}

static intptr_t host_dispatcher(const NativeHostHandle handle, const NativeHostDispatcherOpcode opcode,
                                const int32_t index, const intptr_t value, void* const ptr, const float opt)
{
    switch (opcode)
    {
    case NATIVE_HOST_OPCODE_UI_RESIZE:
        static_cast<IldaeilPlugin*>(handle)->hostResizeUI(index, value);
        break;
    default:
        break;
    }

    return 0;

    // unused
    (void)ptr;
    (void)opt;
}

/* ------------------------------------------------------------------------------------------------------------
 * Plugin entry point, called by DPF to create a new plugin instance. */

Plugin* createPlugin()
{
    return new IldaeilPlugin();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
