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
    static constexpr const uint kInitialWidth  = 1220;
    static constexpr const uint kInitialHeight = 640;
    static constexpr const uint kGenericWidth  = 600;
    static constexpr const uint kGenericHeight = 400;
    static constexpr const uint kExtraHeight   = 35;

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
        : UI(kInitialWidth, kInitialHeight),
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

        std::strcpy(fPluginSearchString, "Search...");

        fPlugin->setUI(this);

        const double scaleFactor = getScaleFactor();

        if (d_isNotEqual(scaleFactor, 1.0))
            setSize(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);

        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        if (carla_get_current_plugin_count(handle) != 0)
        {
            showPluginUI(handle);
            startThread();
            return;
        }
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

        if (info->hints & PLUGIN_HAS_CUSTOM_EMBED_UI)
        {
            // carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, winIdStr);
            carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_UI_SCALE, getScaleFactor()*1000, nullptr);

            carla_embed_custom_ui(handle, 0, (void*)fOurWindowId);

            fDrawingState = kDrawingPluginCustomUI;
            fInitialSizeHasBeenSet = false;
            tryResizingToChildWindowContent();
        }
        else
        {
            // TODO query parameter information and store it
            const double scaleFactor = getScaleFactor();
            setSize(kGenericWidth * scaleFactor, (kGenericHeight + kExtraHeight) * scaleFactor);
            fDrawingState = kDrawingPluginGenericUI;
        }

        repaint();
    }

protected:
    void uiIdle() override
    {
        switch (fDrawingState)
        {
        case kDrawingInit:
            fDrawingState = kDrawingLoading;
            startThread();
            repaint();
            break;

        case kDrawingPluginCustomUI:
            if (! fInitialSizeHasBeenSet)
                tryResizingToChildWindowContent();
            // fall-through

        case kDrawingPluginGenericUI:
            fPlugin->fCarlaPluginDescriptor->ui_idle(fPlugin->fCarlaPluginHandle);
            break;

        default:
            break;
        }
    }

    void run() override
    {
        if (const uint count = carla_get_cached_plugin_count(PLUGIN_LV2, nullptr))
        {
            fPluginCount = 0;
            fPlugins = new CarlaCachedPluginInfo[count];

            if (fDrawingState == kDrawingLoading)
                fDrawingState = kDrawingPluginList;

            for (uint i=0; i < count && ! shouldThreadExit(); ++i)
            {
                std::memcpy(&fPlugins[i], carla_get_cached_plugin_info(PLUGIN_LV2, i), sizeof(CarlaCachedPluginInfo));
                // TODO fix leaks
                fPlugins[i].name = strdup(fPlugins[i].name);
                fPlugins[i].label = strdup(fPlugins[i].label);
                ++fPluginCount;
            }
        }
    }

    void onImGuiDisplay() override
    {
        switch (fDrawingState)
        {
        case kDrawingInit:
            break;
        case kDrawingPluginList:
            drawPluginList();
            break;
        case kDrawingError:
            // TODO display error message
            break;
        case kDrawingLoading:
            drawLoading();
            break;
        case kDrawingPluginGenericUI:
            drawGenericUI();
            // fall-through
        case kDrawingPluginCustomUI:
            drawBottomBar();
            break;
        }
    }

    void drawBottomBar()
    {
        ImGui::SetNextWindowPos(ImVec2(0, getHeight() - kExtraHeight * getScaleFactor()));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), kExtraHeight * getScaleFactor()));

        if (ImGui::Begin("Current Plugin", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize))
        {
            if (ImGui::Button("Pick Another..."))
            {
                if (fDrawingState == kDrawingPluginGenericUI || fDrawingState == kDrawingPluginCustomUI)
                    carla_show_custom_ui(fPlugin->fCarlaHostHandle, 0, false);

                fDrawingState = kDrawingPluginList;
                const double scaleFactor = getScaleFactor();
                setSize(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);
                return ImGui::End();
            }

            if (fDrawingState == kDrawingPluginGenericUI)
            {
                ImGui::SameLine();

                if (ImGui::Button("Show Custom GUI"))
                {
                    carla_show_custom_ui(fPlugin->fCarlaHostHandle, 0, true);
                    return ImGui::End();
                }
            }
        }

        ImGui::End();
    }

    void setupMainWindowPos()
    {
        float width = getWidth();
        float height = getHeight();
        float margin = 20.0f * getScaleFactor();

        if (fDrawingState == kDrawingPluginGenericUI)
            height -= kExtraHeight * getScaleFactor();

        ImGui::SetNextWindowPos(ImVec2(margin, margin));
        ImGui::SetNextWindowSize(ImVec2(width - 2 * margin, height - 2 * margin));
    }

    void drawGenericUI()
    {
        setupMainWindowPos();

        if (ImGui::Begin("Plugin Parameters", nullptr, ImGuiWindowFlags_NoResize))
        {
            ImGui::TextUnformatted("TODO :: here will go plugin parameters", nullptr);
        }

        ImGui::End();
    }

    void drawLoading()
    {
        setupMainWindowPos();

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoResize))
        {
            ImGui::TextUnformatted("Loading...", nullptr);
        }

        ImGui::End();
    }

    void drawPluginList()
    {
        setupMainWindowPos();

        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;
        const bool pluginIsRunning = carla_get_current_plugin_count(handle) != 0;

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoResize))
        {
            if (ImGui::InputText("", fPluginSearchString, sizeof(fPluginSearchString)-1, ImGuiInputTextFlags_CharsNoBlank|ImGuiInputTextFlags_AutoSelectAll))
                fPluginSearchActive = true;

            if (ImGui::Button("Load Plugin"))
            {
                if (pluginIsRunning)
                    carla_replace_plugin(handle, 0);

                do {
                    const CarlaCachedPluginInfo& info(fPlugins[fPluginSelected]);

                    const char* const slash = std::strchr(info.label, DISTRHO_OS_SEP);
                    DISTRHO_SAFE_ASSERT_BREAK(slash != nullptr);

                    d_stdout("Loading %s...", info.name);

                    if (carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_LV2, nullptr, nullptr,
                                         slash+1, 0, 0x0, PLUGIN_OPTIONS_NULL))
                    {
                        showPluginUI(handle);

                        /*
                        delete[] fPlugins;
                        fPlugins = nullptr;
                        fPluginCount = 0;
                        */

                        return ImGui::End();
                    }

                } while (false);
            }

            if (pluginIsRunning)
            {
                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    showPluginUI(handle);
                    return ImGui::End();
                }
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

                        const char* const slash = std::strchr(info.label, DISTRHO_OS_SEP);
                        DISTRHO_SAFE_ASSERT_CONTINUE(slash != nullptr);

                        if (search != nullptr && strcasestr(info.name, search) == nullptr)
                            continue;

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
        Size<uint> size(getChildWindowSize(fOurWindowId));

        if (size.isValid())
        {
            fInitialSizeHasBeenSet = true;
            size.setHeight(size.getHeight() + kExtraHeight * getScaleFactor());
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
