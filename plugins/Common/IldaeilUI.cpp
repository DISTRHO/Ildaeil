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

#include "DistrhoUI.hpp"
#include "DistrhoPlugin.hpp"
#include "PluginHostWindow.hpp"
#include "extra/Thread.hpp"

START_NAMESPACE_DISTRHO

class IldaeilPlugin : public Plugin
{
public:
    const NativePluginDescriptor* fCarlaPluginDescriptor;
    NativePluginHandle fCarlaPluginHandle;

    NativeHostDescriptor fCarlaHostDescriptor;
    CarlaHostHandle fCarlaHostHandle;

    // ...
};

// -----------------------------------------------------------------------------------------------------------

using namespace CarlaBackend;

// shared resource pointer
// carla_juce_init();

class IldaeilUI : public UI,
                  public Thread,
                  public PluginHostWindow::Callbacks
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
        kDrawingPluginGenericUI,
        kDrawingPluginPendingFromInit
    } fDrawingState;

    struct PluginInfoCache {
        char* name;
        char* label;

        PluginInfoCache()
            : name(nullptr),
              label(nullptr) {}

        ~PluginInfoCache()
        {
            std::free(name);
            std::free(label);
        }
    };

    IldaeilPlugin* const fPlugin;
    PluginHostWindow fPluginHostWindow;

    uint fPluginCount;
    uint fPluginSelected;
    bool fPluginScanningFinished;
    PluginInfoCache* fPlugins;

    bool fPluginSearchActive;
    char fPluginSearchString[0xff];

public:
    IldaeilUI()
        : UI(kInitialWidth, kInitialHeight),
          Thread("IldaeilScanner"),
          fDrawingState(kDrawingInit),
          fPlugin((IldaeilPlugin*)getPluginInstancePointer()),
          fPluginHostWindow(getWindow(), this),
          fPluginCount(0),
          fPluginSelected(0),
          fPluginScanningFinished(false),
          fPlugins(nullptr),
          fPluginSearchActive(false)
    {
        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
        {
            fDrawingState = kDrawingError;
            return;
        }

        std::strcpy(fPluginSearchString, "Search...");

        // fPlugin->setUI(this);

        const double scaleFactor = getScaleFactor();

        if (d_isNotEqual(scaleFactor, 1.0))
        {
            setSize(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);
            fPluginHostWindow.setPositionAndSize(0, kExtraHeight * scaleFactor,
                                                 kInitialWidth * scaleFactor, (kInitialHeight - kExtraHeight) * scaleFactor);
        }
        else
        {
            fPluginHostWindow.setPositionAndSize(0, kExtraHeight, kInitialWidth, kInitialHeight-kExtraHeight);
        }

        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        // carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, winIdStr);
        carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_UI_SCALE, getScaleFactor()*1000, nullptr);

        if (carla_get_current_plugin_count(handle) != 0)
            fDrawingState = kDrawingPluginPendingFromInit;
    }

    ~IldaeilUI() override
    {
        if (isThreadRunning())
            stopThread(-1);

        // fPlugin->fUI = nullptr;
        hidePluginUI();

        delete[] fPlugins;
    }

    void showPluginUI(const CarlaHostHandle handle)
    {
        const CarlaPluginInfo* const info = carla_get_plugin_info(handle, 0);

        if (info->hints & PLUGIN_HAS_CUSTOM_EMBED_UI)
        {
            fDrawingState = kDrawingPluginCustomUI;
            carla_embed_custom_ui(handle, 0, fPluginHostWindow.attachAndGetWindowHandle());
        }
        else
        {
            fDrawingState = kDrawingPluginGenericUI;
            // TODO query parameter information and store it
            const double scaleFactor = getScaleFactor();
            setSize(kGenericWidth * scaleFactor, (kGenericHeight + kExtraHeight) * scaleFactor);
        }

        repaint();
    }

    void hidePluginUI()
    {

        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
            return;

        if (fDrawingState == kDrawingPluginGenericUI || fDrawingState == kDrawingPluginCustomUI)
            carla_show_custom_ui(fPlugin->fCarlaHostHandle, 0, false);

        fPluginHostWindow.hide();
    }

protected:
    void pluginWindowResized(uint width, uint height) override
    {
        setSize(width, height + kExtraHeight * getScaleFactor());
    }

    void uiIdle() override
    {
        switch (fDrawingState)
        {
        case kDrawingInit:
            fDrawingState = kDrawingLoading;
            startThread();
            repaint();
            break;

        case kDrawingPluginPendingFromInit:
            showPluginUI(fPlugin->fCarlaHostHandle);
            startThread();
            break;

        case kDrawingPluginCustomUI:
            fPlugin->fCarlaPluginDescriptor->ui_idle(fPlugin->fCarlaPluginHandle);
            fPluginHostWindow.idle();
            break;

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
            fPlugins = new PluginInfoCache[count];

            if (fDrawingState == kDrawingLoading)
                fDrawingState = kDrawingPluginList;

            for (uint i=0, j; i < count && ! shouldThreadExit(); ++i)
            {
                const CarlaCachedPluginInfo* const info = carla_get_cached_plugin_info(PLUGIN_LV2, i);
                DISTRHO_SAFE_ASSERT_CONTINUE(info != nullptr);

                #if DISTRHO_PLUGIN_IS_SYNTH
                if (info->midiIns != 1 || info->audioOuts != 2)
                    continue;
                if ((info->hints & PLUGIN_IS_SYNTH) == 0x0)
                    continue;
                #elif DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
                if (info->midiIns != 1 || info->midiOuts != 1)
                    continue;
                if (info->audioIns != 0 || info->audioOuts != 0)
                    continue;
                #else
                if (info->audioIns != 2 || info->audioOuts != 2)
                    continue;
                #endif

                j = fPluginCount;
                fPlugins[j].name = strdup(info->name);
                fPlugins[j].label = strdup(info->label);
                ++fPluginCount;
            }
        }

        if (! shouldThreadExit())
            fPluginScanningFinished = true;
    }

    void onImGuiDisplay() override
    {
        switch (fDrawingState)
        {
        case kDrawingInit:
        case kDrawingLoading:
        case kDrawingPluginPendingFromInit:
            drawLoading();
            break;
        case kDrawingPluginList:
            drawPluginList();
            break;
        case kDrawingError:
            // TODO display error message
            break;
        case kDrawingPluginGenericUI:
            drawGenericUI();
            // fall-through
        case kDrawingPluginCustomUI:
            drawTopBar();
            break;
        }
    }

    void drawTopBar()
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), kExtraHeight * getScaleFactor()));

        if (ImGui::Begin("Current Plugin", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize))
        {
            if (ImGui::Button("Pick Another..."))
            {
                hidePluginUI();

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
        float y = 0;
        float height = getHeight();

        if (fDrawingState == kDrawingPluginGenericUI)
        {
            y = (kExtraHeight - 1) * getScaleFactor();
            height -= y;
        }

        ImGui::SetNextWindowPos(ImVec2(0, y));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), height));
    }

    void drawGenericUI()
    {
        setupMainWindowPos();

        if (ImGui::Begin("Plugin Parameters", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize))
        {
            ImGui::TextUnformatted("TODO :: here will go plugin parameters", nullptr);
        }

        ImGui::End();
    }

    void drawLoading()
    {
        setupMainWindowPos();

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize))
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

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize))
        {
            if (ImGui::InputText("", fPluginSearchString, sizeof(fPluginSearchString)-1, ImGuiInputTextFlags_CharsNoBlank|ImGuiInputTextFlags_AutoSelectAll))
                fPluginSearchActive = true;

            ImGui::BeginDisabled(!fPluginScanningFinished);

            if (ImGui::Button("Load Plugin"))
            {
                if (pluginIsRunning)
                {
                    hidePluginUI();
                    carla_replace_plugin(handle, 0);
                }

                do {
                    const PluginInfoCache& info(fPlugins[fPluginSelected]);

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

                        break;
                    }

                } while (false);
            }

            ImGui::EndDisabled();

            if (pluginIsRunning)
            {
                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    showPluginUI(handle);
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
                        const PluginInfoCache& info(fPlugins[i]);

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

protected:
   /* --------------------------------------------------------------------------------------------------------
    * DSP/Plugin Callbacks */

    void parameterChanged(uint32_t, float) override
    {
    }

    void stateChanged(const char* const key, const char* const) override
    {
        if (std::strcmp(key, "project") == 0)
            hidePluginUI();
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
