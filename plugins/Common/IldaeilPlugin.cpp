/*
 * DISTRHO Ildaeil Plugin
 * Copyright (C) 2021-2022 Filipe Coelho <falktx@falktx.com>
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

#include "IldaeilBasePlugin.hpp"

#include "CarlaEngine.hpp"
#include "water/files/File.h"
#include "water/streams/MemoryOutputStream.h"
#include "water/xml/XmlDocument.h"

START_NAMESPACE_DISTRHO

using namespace CARLA_BACKEND_NAMESPACE;

// --------------------------------------------------------------------------------------------------------------------

static uint32_t host_get_buffer_size(NativeHostHandle);
static double host_get_sample_rate(NativeHostHandle);
static bool host_is_offline(NativeHostHandle);
static const NativeTimeInfo* host_get_time_info(NativeHostHandle handle);
static bool host_write_midi_event(NativeHostHandle handle, const NativeMidiEvent* event);
static void host_ui_parameter_changed(NativeHostHandle handle, uint32_t index, float value);
static void host_ui_midi_program_changed(NativeHostHandle handle, uint8_t channel, uint32_t bank, uint32_t program);
static void host_ui_custom_data_changed(NativeHostHandle handle, const char* key, const char* value);
static void host_ui_closed(NativeHostHandle handle);
static const char* host_ui_open_file(NativeHostHandle handle, bool isDir, const char* title, const char* filter);
static const char* host_ui_save_file(NativeHostHandle handle, bool isDir, const char* title, const char* filter);
static intptr_t host_dispatcher(NativeHostHandle handle, NativeHostDispatcherOpcode opcode, int32_t index, intptr_t value, void* ptr, float opt);

// --------------------------------------------------------------------------------------------------------------------

Mutex IldaeilBasePlugin::sPluginInfoLoadMutex;

// --------------------------------------------------------------------------------------------------------------------

const char* IldaeilBasePlugin::getPathForJSFX()
{
    static water::String path;

    if (path.isEmpty())
    {
       #if defined(CARLA_OS_MAC)
        path = water::File::getSpecialLocation(water::File::userHomeDirectory).getFullPathName()
             + "/Library/Application Support/REAPER/Effects";
       #elif defined(CARLA_OS_WASM)
        path = "/jsfx";
       #elif defined(CARLA_OS_WIN)
        path = water::File::getSpecialLocation(water::File::winAppData).getFullPathName() + "\\REAPER\\Effects";
        if (! water::File(path).isDirectory())
            path = water::File::getSpecialLocation(water::File::winProgramFiles).getFullPathName()
                 + "\\REAPER\\InstallData\\Effects";
       #else
        if (const char* const configHome = std::getenv("XDG_CONFIG_HOME"))
            path = configHome;
        else
            path = water::File::getSpecialLocation(water::File::userHomeDirectory).getFullPathName() + "/.config";
        path += "/REAPER/Effects";
       #endif
    }

    return path.toRawUTF8();
}

// --------------------------------------------------------------------------------------------------------------------

class IldaeilPlugin : public IldaeilBasePlugin
{
#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
    static constexpr const uint kMaxMidiEventCount = 512;
    NativeMidiEvent* fMidiEvents;
    uint32_t fMidiEventCount;
    float* fDummyBuffer;
    float* fDummyBuffers[2];
#endif

    mutable NativeTimeInfo fCarlaTimeInfo;
    mutable water::MemoryOutputStream fLastProjectState;
    uint32_t fLastLatencyValue;

public:
    IldaeilPlugin()
        : IldaeilBasePlugin(),
#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
          fMidiEvents(nullptr),
          fMidiEventCount(0),
          fDummyBuffer(nullptr),
#endif
          fLastLatencyValue(0)
    {
        fCarlaPluginDescriptor = carla_get_native_rack_plugin();
        DISTRHO_SAFE_ASSERT_RETURN(fCarlaPluginDescriptor != nullptr,);

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
        fCarlaHostDescriptor.ui_parameter_changed = host_ui_parameter_changed;
        fCarlaHostDescriptor.ui_midi_program_changed = host_ui_midi_program_changed;
        fCarlaHostDescriptor.ui_custom_data_changed = host_ui_custom_data_changed;
        fCarlaHostDescriptor.ui_closed = host_ui_closed;
        fCarlaHostDescriptor.ui_open_file = host_ui_open_file;
        fCarlaHostDescriptor.ui_save_file = host_ui_save_file;
        fCarlaHostDescriptor.dispatcher = host_dispatcher;

        fCarlaPluginHandle = fCarlaPluginDescriptor->instantiate(&fCarlaHostDescriptor);
        DISTRHO_SAFE_ASSERT_RETURN(fCarlaPluginHandle != nullptr,);

        fCarlaHostHandle = carla_create_native_plugin_host_handle(fCarlaPluginDescriptor, fCarlaPluginHandle);
        DISTRHO_SAFE_ASSERT_RETURN(fCarlaHostHandle != nullptr,);

        if (const char* const bundlePath = getBundlePath())
        {
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_BINARIES, 0, bundlePath);
            // carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_RESOURCES, 0, "");
        }
        else
        {
           #ifdef CARLA_OS_MAC
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_BINARIES, 0, "/Applications/Carla.app/Contents/MacOS");
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_RESOURCES, 0, "/Applications/Carla.app/Contents/MacOS/resources");
           #else
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_BINARIES, 0, "/usr/lib/carla");
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PATH_RESOURCES, 0, "/usr/share/carla/resources");
           #endif
        }

        if (const char* const path = std::getenv("LV2_PATH"))
            carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PLUGIN_PATH, PLUGIN_LV2, path);

        carla_set_engine_option(fCarlaHostHandle, ENGINE_OPTION_PLUGIN_PATH, PLUGIN_JSFX, getPathForJSFX());

        fCarlaPluginDescriptor->dispatcher(fCarlaPluginHandle, NATIVE_PLUGIN_OPCODE_HOST_USES_EMBED,
                                           0, 0, nullptr, 0.0f);

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

    intptr_t hostDispatcher(const NativeHostDispatcherOpcode opcode,
                            const int32_t index, const intptr_t value, void* const ptr, const float opt)
    {
        switch (opcode)
        {
        // cannnot be supported
        case NATIVE_HOST_OPCODE_HOST_IDLE:
            break;
        // other stuff
        case NATIVE_HOST_OPCODE_NULL:
        case NATIVE_HOST_OPCODE_UPDATE_PARAMETER:
        case NATIVE_HOST_OPCODE_UPDATE_MIDI_PROGRAM:
        case NATIVE_HOST_OPCODE_RELOAD_PARAMETERS:
        case NATIVE_HOST_OPCODE_RELOAD_MIDI_PROGRAMS:
        case NATIVE_HOST_OPCODE_RELOAD_ALL:
        case NATIVE_HOST_OPCODE_UI_UNAVAILABLE:
        case NATIVE_HOST_OPCODE_INTERNAL_PLUGIN:
        case NATIVE_HOST_OPCODE_QUEUE_INLINE_DISPLAY:
        case NATIVE_HOST_OPCODE_UI_TOUCH_PARAMETER:
        case NATIVE_HOST_OPCODE_REQUEST_IDLE:
        case NATIVE_HOST_OPCODE_GET_FILE_PATH:
        case NATIVE_HOST_OPCODE_UI_RESIZE:
        case NATIVE_HOST_OPCODE_PREVIEW_BUFFER_DATA:
            // TESTING
            d_stdout("dispatcher %i, %i, %li, %p, %f", opcode, index, value, ptr, opt);
            break;
        }

        return 0;
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

    void initState(const uint32_t index, String& stateKey, String& defaultStateValue) override
    {
        DISTRHO_SAFE_ASSERT_RETURN(index == 0,);

        stateKey = "project";
        defaultStateValue = ""
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE CARLA-PROJECT>\n"
        "<CARLA-PROJECT VERSION='" CARLA_VERSION_STRMIN "'>\n"
        "</CARLA-PROJECT>\n";
    }

   /* --------------------------------------------------------------------------------------------------------
    * Internal data */

    String getState(const char* const key) const override
    {
        if (std::strcmp(key, "project") == 0)
        {
            CarlaEngine* const engine = carla_get_engine_from_handle(fCarlaHostHandle);

            fLastProjectState.reset();
            engine->saveProjectInternal(fLastProjectState);
            return String(static_cast<char*>(fLastProjectState.getDataAndRelease()), false);
        }

        return String();
    }

    void setState(const char* const key, const char* const value) override
    {
        if (std::strcmp(key, "project") == 0)
        {
            CarlaEngine* const engine = carla_get_engine_from_handle(fCarlaHostHandle);

            water::XmlDocument xml(value);

            {
                const MutexLocker cml(sPluginInfoLoadMutex);
                engine->loadProjectInternal(xml, true);
            }

            if (fUI != nullptr)
                ildaeilProjectLoadedFromDSP(fUI);
        }
    }

   /* --------------------------------------------------------------------------------------------------------
    * Process */

    void checkLatencyChanged()
    {
        if (fCarlaHostHandle == nullptr)
            return;

        uint32_t latency = 0;

        for (uint32_t i=0; i < carla_get_current_plugin_count(fCarlaHostHandle); ++i)
            latency += carla_get_plugin_latency(fCarlaHostHandle, i);

        if (fLastLatencyValue != latency)
        {
            fLastLatencyValue = latency;
            setLatency(latency);
        }
    }

    void activate() override
    {
        if (fCarlaPluginHandle != nullptr)
            fCarlaPluginDescriptor->activate(fCarlaPluginHandle);

        checkLatencyChanged();
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

            checkLatencyChanged();
        }
        else
        {
            std::memset(outputs[0], 0, sizeof(float)*frames);
            std::memset(outputs[1], 0, sizeof(float)*frames);
        }
    }

    void bufferSizeChanged(const uint32_t newBufferSize) override
    {
#if DISTRHO_PLUGIN_WANT_MIDI_INPUT
        delete[] fDummyBuffer;
        fDummyBuffer = new float[newBufferSize];
        fDummyBuffers[0] = fDummyBuffer;
        fDummyBuffers[1] = fDummyBuffer;
        std::memset(fDummyBuffer, 0, sizeof(float)*newBufferSize);
#endif
        if (fCarlaPluginHandle != nullptr)
            fCarlaPluginDescriptor->dispatcher(fCarlaPluginHandle, NATIVE_PLUGIN_OPCODE_BUFFER_SIZE_CHANGED,
                                               0, newBufferSize, nullptr, 0.0f);
    }

    void sampleRateChanged(const double newSampleRate) override
    {
        if (fCarlaPluginHandle != nullptr)
            fCarlaPluginDescriptor->dispatcher(fCarlaPluginHandle, NATIVE_PLUGIN_OPCODE_SAMPLE_RATE_CHANGED,
                                               0, 0, nullptr, newSampleRate);
    }

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

static void host_ui_parameter_changed(const NativeHostHandle handle, const uint32_t index, const float value)
{
    ildaeilParameterChangeForUI(static_cast<IldaeilPlugin*>(handle)->fUI, index, value);
}

static void host_ui_midi_program_changed(NativeHostHandle handle, uint8_t channel, uint32_t bank, uint32_t program)
{
    d_stdout("%s %p %u %u %u", __FUNCTION__, handle, channel, bank, program);
}

static void host_ui_custom_data_changed(NativeHostHandle handle, const char* key, const char* value)
{
    d_stdout("%s %p %s %s", __FUNCTION__, handle, key, value);
}

static void host_ui_closed(NativeHostHandle handle)
{
    d_stdout("%s %p", __FUNCTION__, handle);
    ildaeilCloseUI(static_cast<IldaeilPlugin*>(handle));
}

static const char* host_ui_open_file(const NativeHostHandle handle, const bool isDir, const char* const title, const char* const filter)
{
    return ildaeilOpenFileForUI(static_cast<IldaeilPlugin*>(handle)->fUI, isDir, title, filter);
}

static const char* host_ui_save_file(NativeHostHandle, bool, const char*, const char*)
{
    return nullptr;
}

static intptr_t host_dispatcher(const NativeHostHandle handle, const NativeHostDispatcherOpcode opcode,
                                const int32_t index, const intptr_t value, void* const ptr, const float opt)
{
    return static_cast<IldaeilPlugin*>(handle)->hostDispatcher(opcode, index, value, ptr, opt);
}

/* --------------------------------------------------------------------------------------------------------------------
 * Plugin entry point, called by DPF to create a new plugin instance. */

Plugin* createPlugin()
{
    return new IldaeilPlugin();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
