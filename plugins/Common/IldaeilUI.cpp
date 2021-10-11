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

#include "../FX/DistrhoPluginInfo.h"

#include "DistrhoUI.hpp"
#include "DistrhoPlugin.hpp"
#include "SizeUtils.hpp"
#include "extra/Thread.hpp"

START_NAMESPACE_DISTRHO

class IldaeilPlugin : public Plugin
{
public:
    const NativePluginDescriptor* fCarlaPluginDescriptor;
    NativePluginHandle fCarlaPluginHandle;

    NativeHostDescriptor fCarlaHostDescriptor;
    CarlaHostHandle fCarlaHostHandle;

    UI* fUI;

    void setUI(UI* const ui)
    {
        fUI = ui;
    }

    // ...
};

// -----------------------------------------------------------------------------------------------------------

using namespace CarlaBackend;

// shared resource pointer
// carla_juce_init();

class IldaeilUI : public UI, public Thread
{
    enum {
        kDrawingInit,
        kDrawingError,
        kDrawingLoading,
        kDrawingPluginList,
        kDrawingPluginCustomUI,
        kDrawingPluginGenericUI
    } fDrawingState;

    IldaeilPlugin* const fPlugin;

    uint fPluginCount;
    uint fPluginSelected;
    CarlaCachedPluginInfo* fPlugins;

    bool fPluginSearchActive;
    char fPluginSearchString[0xff];

    bool fInitialSizeHasBeenSet;
    const uintptr_t fOurWindowId;

public:
    IldaeilUI()
        : UI(1280, 720),
          Thread("IldaeilScanner"),
          fDrawingState(kDrawingInit),
          fPlugin((IldaeilPlugin*)getPluginInstancePointer()),
          fPluginCount(0),
          fPluginSelected(0),
          fPlugins(nullptr),
          fPluginSearchActive(false),
          fInitialSizeHasBeenSet(false),
          fOurWindowId(getWindow().getNativeWindowHandle())
    {
        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
        {
            fDrawingState = kDrawingError;
            return;
        }

        fPlugin->setUI(this);

        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        if (carla_get_current_plugin_count(handle) != 0)
        {
            showPluginUI(handle);
            return;
        }

        std::strcpy(fPluginSearchString, "Search...");
    }

    ~IldaeilUI() override
    {
        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
            return;

        stopThread(-1);

        fPlugin->fUI = nullptr;

        if (fDrawingState == kDrawingPluginCustomUI)
            carla_show_custom_ui(fPlugin->fCarlaHostHandle, 0, false);

        delete[] fPlugins;
    }

    void showPluginUI(const CarlaHostHandle handle)
    {
        const CarlaPluginInfo* const info = carla_get_plugin_info(handle, 0);

        if (info->hints & PLUGIN_HAS_CUSTOM_UI) // FIXME use PLUGIN_HAS_CUSTOM_EMBED_UI
        {
            // carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, winIdStr);
            carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_UI_SCALE, getScaleFactor()*1000, nullptr);

            carla_embed_custom_ui(handle, 0, (void*)fOurWindowId);

            // tryResizingToChildWindowContent();
            fDrawingState = kDrawingPluginCustomUI;
        }
    }

protected:
    void uiIdle() override
    {
        switch (fDrawingState)
        {
        case kDrawingPluginCustomUI:
            break;
        case kDrawingInit:
            fDrawingState = kDrawingLoading;
            startThread();
            repaint();
            return;
        default:
            return;
        }

        fPlugin->fCarlaPluginDescriptor->ui_idle(fPlugin->fCarlaPluginHandle);

        if (! fInitialSizeHasBeenSet)
            tryResizingToChildWindowContent();
    }

    void run() override
    {
        fPluginCount = carla_get_cached_plugin_count(PLUGIN_LV2, nullptr);

        if (fPluginCount != 0)
        {
            fPlugins = new CarlaCachedPluginInfo[fPluginCount];

            for (uint i=0; i < fPluginCount && !shouldThreadExit(); ++i)
            {
                std::memcpy(&fPlugins[i], carla_get_cached_plugin_info(PLUGIN_LV2, i), sizeof(CarlaCachedPluginInfo));
                // TODO fix leaks
                fPlugins[i].name = strdup(fPlugins[i].name);
                fPlugins[i].label = strdup(fPlugins[i].label);
            }
        }

        if (!shouldThreadExit())
            fDrawingState = kDrawingPluginList;
    }

    void onImGuiDisplay() override
    {
        switch (fDrawingState)
        {
        case kDrawingPluginList:
            break;
        case kDrawingError:
            // TODO display error message
            return;
        case kDrawingLoading:
            // TODO display loading message
            return;
        default:
            return;
        }

        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        float width = getWidth();
        float height = getHeight();
        float margin = 20.0f;

        ImGui::SetNextWindowPos(ImVec2(margin, margin));
        ImGui::SetNextWindowSize(ImVec2(width - 2 * margin, height - 2 * margin));

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoResize))
        {
            if (ImGui::InputText("", fPluginSearchString, sizeof(fPluginSearchString)-1, ImGuiInputTextFlags_CharsNoBlank|ImGuiInputTextFlags_AutoSelectAll))
                fPluginSearchActive = true;

            if (ImGui::Button("Load Plugin"))
            {
                do {
                    const CarlaCachedPluginInfo& info(fPlugins[fPluginSelected]);

                    const char* const slash = std::strchr(info.label, DISTRHO_OS_SEP);
                    DISTRHO_SAFE_ASSERT_BREAK(slash != nullptr);

                    d_stdout("Loading %s...", info.name);

                    if (carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_LV2, nullptr, nullptr,
                                         slash+1, 0, 0x0, PLUGIN_OPTIONS_NULL))
                    {
                        showPluginUI(handle);
                        repaint();

                        delete[] fPlugins;
                        fPlugins = nullptr;
                        fPluginCount = 0;
                    }

                } while (false);
            }

            if (ImGui::BeginChild("pluginlistwindow"))
            {
                if (ImGui::BeginTable("pluginlist", 3, ImGuiTableFlags_NoSavedSettings|ImGuiTableFlags_NoClip))
                {
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Bundle");
                    ImGui::TableSetupColumn("URI");
                    ImGui::TableHeadersRow();

                    const char* const search = fPluginSearchActive && fPluginSearchString[0] != '\0' ? fPluginSearchString : nullptr;

                    for (uint i=0; i<fPluginCount; ++i)
                    {
                        const CarlaCachedPluginInfo& info(fPlugins[i]);

                        /*
                        #if DISTRHO_PLUGIN_IS_SYNTH
                        if (info.midiIns != 1 || info.audioOuts != 2)
                            continue;
                        #elif DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
                        if (info.midiIns != 1 || info.midiOuts != 1)
                            continue;
                        if (info.audioIns != 0 || info.audioOuts != 0)
                            continue;
                        #else
                        if (info.audioIns != 2 || info.audioOuts != 2)
                            continue;
                        #endif
                        */

                        const char* const slash = std::strchr(info.label, DISTRHO_OS_SEP);
                        DISTRHO_SAFE_ASSERT_CONTINUE(slash != nullptr);

                        // if (search != nullptr && strcasestr(info.name, search) == nullptr)
                        //     continue;

                        bool selected = fPluginSelected == i;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Selectable(info.name, &selected);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Selectable(slash+1, &selected);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(info.label, slash);

                        if (selected)
                            fPluginSelected = i;
                    }

                    ImGui::EndTable();
                }
                ImGui::EndChild();
            }
        }

        ImGui::End();
    }

private:
    void tryResizingToChildWindowContent()
    {
        const Size<uint> size(getChildWindowSize(fOurWindowId));

        if (size.isValid())
        {
            fInitialSizeHasBeenSet = true;
            setSize(size);
        }
    }

protected:
   /* --------------------------------------------------------------------------------------------------------
    * DSP/Plugin Callbacks */

   /**
      A parameter has changed on the plugin side.
      This is called by the host to inform the UI about parameter changes.
    */
    void parameterChanged(uint32_t, float) override
    {
    }

    // -------------------------------------------------------------------------------------------------------

private:
   /**
      Set our UI class as non-copyable and add a leak detector just in case.
    */
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IldaeilUI)
};

/* ------------------------------------------------------------------------------------------------------------
 * UI entry point, called by DPF to create a new UI instance. */

UI* createUI()
{
    return new IldaeilUI();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
