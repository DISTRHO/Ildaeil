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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../FX/DistrhoPluginInfo.h"

#include "DistrhoUI.hpp"
#include "DistrhoPlugin.hpp"
#include "ResizeHandle.hpp"

START_NAMESPACE_DISTRHO

class IldaeilPlugin : public Plugin
{
public:
    const NativePluginDescriptor* fCarlaPluginDescriptor;
    NativePluginHandle fCarlaPluginHandle;

    NativeHostDescriptor fCarlaHostDescriptor;
    CarlaHostHandle fCarlaHostHandle;

    UI* fUI;

    // ...
};

// -----------------------------------------------------------------------------------------------------------

// shared resource pointer
// carla_juce_init();

class IldaeilUI : public UI
{
    IldaeilPlugin* const fPlugin;
    // ResizeHandle fResizeHandle;

    uint fPluginCount;
    uint fPluginSelected;

    ::Window fHostWindowLookingToResize;

public:
    IldaeilUI()
        : UI(1280, 720),
          fPlugin((IldaeilPlugin*)getPluginInstancePointer()),
          // fResizeHandle(this),
          fPluginCount(0),
          fPluginSelected(0),
          fHostWindowLookingToResize(0)
    {
        using namespace CarlaBackend;

        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
            return;

        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        if (carla_get_current_plugin_count(handle) != 0)
        {
            const CarlaPluginInfo* const info = carla_get_plugin_info(handle, 0);

            if (info->hints & PLUGIN_HAS_CUSTOM_UI) // FIXME use PLUGIN_HAS_CUSTOM_EMBED_UI
            {
                const uintptr_t winId = getWindow().getNativeWindowHandle();
                carla_embed_custom_ui(handle, 0, (void*)winId);

                fHostWindowLookingToResize = (::Window)winId;
                tryResizingToChildWindowContent();
            }
        }

        // start cache/lookup, maybe spawn thread for this?
        fPluginCount = carla_get_cached_plugin_count(PLUGIN_LV2, nullptr);
        for (uint i=0; i<fPluginCount; ++i)
            carla_get_cached_plugin_info(PLUGIN_LV2, i);
    }

    ~IldaeilUI() override
    {
        if (fPlugin != nullptr)
        {
            fPlugin->fUI = nullptr;

            if (fPlugin->fCarlaHostHandle != nullptr)
                carla_show_custom_ui(fPlugin->fCarlaHostHandle, 0, false);
        }
    }

    void onImGuiDisplay() override
    {
        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
            return;

        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        if (carla_get_current_plugin_count(handle) != 0)
            return;

        float width = getWidth();
        float height = getHeight();
        float margin = 20.0f;

        ImGui::SetNextWindowPos(ImVec2(margin, margin));
        ImGui::SetNextWindowSize(ImVec2(width - 2 * margin, height - 2 * margin));

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoResize))
        {
            static char searchBuf[0xff] = "Search...";
            ImGui::InputText("", searchBuf, sizeof(searchBuf)-1, ImGuiInputTextFlags_CharsNoBlank|ImGuiInputTextFlags_AutoSelectAll);

            using namespace CarlaBackend;

            if (ImGui::Button("Load Plugin"))
            {
                do {
                    const CarlaCachedPluginInfo* info = carla_get_cached_plugin_info(PLUGIN_LV2, fPluginSelected);
                    DISTRHO_SAFE_ASSERT_BREAK(info != nullptr);

                    const char* const slash = std::strchr(info->label, DISTRHO_OS_SEP);
                    DISTRHO_SAFE_ASSERT_BREAK(slash != nullptr);

                    d_stdout("Loading %s...", info->name);

                    if (carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_LV2, nullptr, nullptr,
                                         slash+1, 0, 0x0, PLUGIN_OPTIONS_NULL))
                    {
                        const CarlaPluginInfo* const info = carla_get_plugin_info(handle, 0);

                        if (info->hints & PLUGIN_HAS_CUSTOM_UI) // FIXME use PLUGIN_HAS_CUSTOM_EMBED_UI
                        {
                            const uintptr_t winId = getWindow().getNativeWindowHandle();
                            carla_embed_custom_ui(handle, 0, (void*)winId);

                            fHostWindowLookingToResize = (::Window)winId;
                            tryResizingToChildWindowContent();
                        }

                        repaint();
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

                    const char* const search = searchBuf[0] != 0 && std::strcmp(searchBuf, "Search...") != 0 ? searchBuf : nullptr;

                    if (fPluginCount != 0)
                    {
                        for (uint i=0; i<fPluginCount; ++i)
                        {
                            const CarlaCachedPluginInfo* info = carla_get_cached_plugin_info(PLUGIN_LV2, i);
                            DISTRHO_SAFE_ASSERT_CONTINUE(info != nullptr);

                           #if DISTRHO_PLUGIN_IS_SYNTH
                            if (info->midiIns != 1 || info->audioOuts != 2)
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

                            const char* const slash = std::strchr(info->label, DISTRHO_OS_SEP);
                            DISTRHO_SAFE_ASSERT_CONTINUE(slash != nullptr);

                            if (search != nullptr && strcasestr(info->name, search) == nullptr)
                                continue;

                            bool selected = fPluginSelected == i;
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Selectable(info->name, &selected);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Selectable(slash+1, &selected);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextUnformatted(info->label, slash);

                            if (selected)
                                fPluginSelected = i;
                        }
                    }

                    ImGui::EndTable();
                }
                ImGui::EndChild();
            }
        }

        ImGui::End();
    }

    void uiIdle() override
    {
        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
            return;

        fPlugin->fCarlaPluginDescriptor->ui_idle(fPlugin->fCarlaPluginHandle);

        if (fHostWindowLookingToResize == 0)
            return;
        tryResizingToChildWindowContent();
    }

private:
    void tryResizingToChildWindowContent()
    {
        if (::Display* const display = XOpenDisplay(nullptr))
        {
            if (const ::Window childWindow = getChildWindow(display, fHostWindowLookingToResize))
            {
                d_stdout("found child window");

                XSizeHints sizeHints;
                memset(&sizeHints, 0, sizeof(sizeHints));

                if (XGetNormalHints(display, childWindow, &sizeHints))
                {
                    int width = 0;
                    int height = 0;

                    if (sizeHints.flags & PSize)
                    {
                        width = sizeHints.width;
                        height = sizeHints.height;
                    }
                    else if (sizeHints.flags & PBaseSize)
                    {
                        width = sizeHints.base_width;
                        height = sizeHints.base_height;
                    }
                    else if (sizeHints.flags & PMinSize)
                    {
                        width = sizeHints.min_width;
                        height = sizeHints.min_height;
                    }

                    d_stdout("child window bounds %u %u", width, height);

                    if (width > 1 && height > 1)
                    {
                        fHostWindowLookingToResize = 0;
                        setSize(static_cast<uint>(width), static_cast<uint>(height));
                    }
                }
                else
                    d_stdout("child window without bounds");
            }

            XCloseDisplay(display);
        }
    }

    ::Window getChildWindow(::Display* const display, const ::Window hostWindow) const
    {
        ::Window rootWindow, parentWindow, ret = 0;
        ::Window* childWindows = nullptr;
        uint numChildren = 0;

        XQueryTree(display, hostWindow, &rootWindow, &parentWindow, &childWindows, &numChildren);

        if (numChildren > 0 && childWindows != nullptr)
        {
            ret = childWindows[0];
            XFree(childWindows);
        }

        return ret;
    }

protected:
   /* --------------------------------------------------------------------------------------------------------
    * DSP/Plugin Callbacks */

   /**
      A parameter has changed on the plugin side.
      This is called by the host to inform the UI about parameter changes.
    */
    void parameterChanged(uint32_t index, float value) override
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
