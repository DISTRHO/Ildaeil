/*
 * DISTRHO Ildaeil Plugin
 * Copyright (C) 2021 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
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
#include "DistrhoUI.hpp"

START_NAMESPACE_DISTRHO

// -----------------------------------------------------------------------------------------------------------

static uint32_t host_get_buffer_size(NativeHostHandle);
static double host_get_sample_rate(NativeHostHandle);
static bool host_is_offline(NativeHostHandle);
static const NativeTimeInfo* host_get_time_info(NativeHostHandle handle);
static bool host_write_midi_event(NativeHostHandle handle, const NativeMidiEvent* event);
static intptr_t host_dispatcher(NativeHostHandle handle, NativeHostDispatcherOpcode opcode, int32_t index, intptr_t value, void* ptr, float opt);

// -----------------------------------------------------------------------------------------------------------

class IldaeilPlugin : public Plugin
{
    const NativePluginDescriptor* fCarlaPluginDescriptor;
    NativePluginHandle fCarlaPluginHandle;

    NativeHostDescriptor fCarlaHostDescriptor;
    CarlaHostHandle fCarlaHostHandle;

    NativeTimeInfo fCarlaTimeInfo;

    UI* fUI;

public:
    IldaeilPlugin()
        : Plugin(0, 0, 0),
          fCarlaPluginDescriptor(nullptr),
          fCarlaPluginHandle(nullptr),
          fCarlaHostHandle(nullptr),
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
    }

    ~IldaeilPlugin() override
    {
        if (fCarlaHostHandle != nullptr)
        {
            carla_host_handle_free(fCarlaHostHandle);
        }

        if (fCarlaPluginHandle != nullptr)
            fCarlaPluginDescriptor->cleanup(fCarlaPluginHandle);
    }

    const NativeTimeInfo* getTimeInfo()
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

    void resizeUI(const uint width, const uint height)
    {
        DISTRHO_SAFE_ASSERT_RETURN(fUI != nullptr,);

        fUI->setSize(width, height);
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
        return "ISC";
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

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        if (fCarlaPluginHandle != nullptr)
        {
            fCarlaPluginDescriptor->process(fCarlaPluginHandle, (float**)inputs, outputs, frames, nullptr, 0);
        }
        else
        {
            std::memset(outputs[0], 0, sizeof(float)*frames);
            std::memset(outputs[1], 0, sizeof(float)*frames);
        }
    }

    // -------------------------------------------------------------------------------------------------------

private:
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

static const NativeTimeInfo* host_get_time_info(NativeHostHandle handle)
{
    return static_cast<IldaeilPlugin*>(handle)->getTimeInfo();
}

static bool host_write_midi_event(NativeHostHandle handle, const NativeMidiEvent* event)
{
    return false;
}

static intptr_t host_dispatcher(NativeHostHandle handle, NativeHostDispatcherOpcode opcode,
                                int32_t index, intptr_t value, void* ptr, float opt)
{
    switch (opcode)
    {
    case NATIVE_HOST_OPCODE_UI_RESIZE:
        static_cast<IldaeilPlugin*>(handle)->resizeUI(index, value);
        break;
    default:
        break;
    }

    return 0;
}

/* ------------------------------------------------------------------------------------------------------------
 * Plugin entry point, called by DPF to create a new plugin instance. */

Plugin* createPlugin()
{
    return new IldaeilPlugin();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
