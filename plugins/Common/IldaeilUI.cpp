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

// IDE helper
#include "DearImGui.hpp"

#include <vector>

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------
class IldaeilPlugin : public Plugin
{
public:
    const NativePluginDescriptor* fCarlaPluginDescriptor;
    NativePluginHandle fCarlaPluginHandle;

    NativeHostDescriptor fCarlaHostDescriptor;
    CarlaHostHandle fCarlaHostHandle;

    void* fUI;

    // ...
};

// --------------------------------------------------------------------------------------------------------------------

void ildaeilParameterChangeForUI(void* ui, uint32_t index, float value);

// --------------------------------------------------------------------------------------------------------------------
using namespace CarlaBackend;

// shared resource pointer
// carla_juce_init();

class IldaeilUI : public UI,
                  public Thread,
                  public PluginHostWindow::Callbacks
{
    static constexpr const uint kInitialWidth  = 1220;
    static constexpr const uint kInitialHeight = 640;
    static constexpr const uint kGenericWidth  = 300;
    static constexpr const uint kGenericHeight = 400;
    static constexpr const uint kExtraHeight   = 35;

    enum {
        kDrawingInit,
        kDrawingError,
        kDrawingLoading,
        kDrawingPluginList,
        kDrawingPluginEmbedUI,
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

    struct PluginGenericUI {
        char* title;
        uint parameterCount;
        struct Parameter {
            char* name;
            char* format;
            uint32_t rindex;
            bool boolean, bvalue;
            float min, max;
            Parameter()
                : name(nullptr),
                  format(nullptr),
                  rindex(0),
                  boolean(false),
                  bvalue(false),
                  min(0.0f),
                  max(1.0f) {}
            ~Parameter()
            {
                std::free(name);
                std::free(format);
            }
        }* parameters;
        float* values;

        PluginGenericUI()
            : title(nullptr),
              parameterCount(0),
              parameters(nullptr),
              values(nullptr) {}

        ~PluginGenericUI()
        {
            std::free(title);
            delete[] parameters;
            delete[] values;
        }
    };

    IldaeilPlugin* const fPlugin;
    PluginHostWindow fPluginHostWindow;

    uint fPluginCount;
    uint fPluginSelected;
    bool fPluginScanningFinished;
    bool fPluginHasCustomUI;
    bool fPluginHasEmbedUI;
    PluginInfoCache* fPlugins;
    ScopedPointer<PluginGenericUI> fPluginGenericUI;

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
          fPluginHasCustomUI(false),
          fPluginHasEmbedUI(false),
          fPlugins(nullptr),
          fPluginSearchActive(false)
    {
        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
        {
            fDrawingState = kDrawingError;
            return;
        }

        std::strcpy(fPluginSearchString, "Search...");

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


        char winIdStr[24];
        std::snprintf(winIdStr, sizeof(winIdStr), "%lx", (ulong)getWindow().getNativeWindowHandle());
        carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, winIdStr);
        carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_UI_SCALE, getScaleFactor()*1000, nullptr);

        if (carla_get_current_plugin_count(handle) != 0)
        {
            const uint hints = carla_get_plugin_info(handle, 0)->hints;
            fDrawingState = kDrawingPluginPendingFromInit;
            fPluginHasCustomUI = hints & PLUGIN_HAS_CUSTOM_UI;
            fPluginHasEmbedUI = hints & PLUGIN_HAS_CUSTOM_EMBED_UI;
        }

        fPlugin->fUI = this;
    }

    ~IldaeilUI() override
    {
        if (fPlugin != nullptr && fPlugin->fCarlaHostHandle != nullptr)
        {
            fPlugin->fUI = nullptr;
            carla_set_engine_option(fPlugin->fCarlaHostHandle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, "0");
        }

        if (isThreadRunning())
            stopThread(-1);

        hidePluginUI();

        fPluginGenericUI = nullptr;

        delete[] fPlugins;
    }

    void changeParameterFromDSP(const uint32_t index, const float value)
    {
        if (PluginGenericUI* const ui = fPluginGenericUI)
        {
            for (uint32_t i=0; i < ui->parameterCount; ++i)
            {
                if (ui->parameters[i].rindex != index)
                    continue;

                ui->values[i] = value;

                if (ui->parameters[i].boolean)
                    ui->parameters[i].bvalue = value > ui->parameters[i].min;

                break;
            }
        }
    }

    void showPluginUI(const CarlaHostHandle handle)
    {
        const CarlaPluginInfo* const info = carla_get_plugin_info(handle, 0);

        if (info->hints & PLUGIN_HAS_CUSTOM_EMBED_UI)
        {
            fDrawingState = kDrawingPluginEmbedUI;
            fPluginHasCustomUI = true;
            fPluginHasEmbedUI = true;
            carla_embed_custom_ui(handle, 0, fPluginHostWindow.attachAndGetWindowHandle());
        }
        else
        {
            fDrawingState = kDrawingPluginGenericUI;
            fPluginHasCustomUI = info->hints & PLUGIN_HAS_CUSTOM_UI;
            fPluginHasEmbedUI = false;
            if (fPluginGenericUI == nullptr)
                createPluginGenericUI(handle, info);
            else
                updatePluginGenericUI(handle);
            const double scaleFactor = getScaleFactor();
            setSize(kGenericWidth * scaleFactor, (kGenericHeight + kExtraHeight) * scaleFactor);
        }

        repaint();
    }

    void hidePluginUI()
    {
        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
            return;

        if (fDrawingState == kDrawingPluginGenericUI || fDrawingState == kDrawingPluginEmbedUI)
            carla_show_custom_ui(fPlugin->fCarlaHostHandle, 0, false);

        fPluginHostWindow.hide();
    }

    void createPluginGenericUI(const CarlaHostHandle handle, const CarlaPluginInfo* const info)
    {
        PluginGenericUI* const ui = new PluginGenericUI;

        String title(info->name);
        title += " by ";
        title += info->maker;
        ui->title = title.getAndReleaseBuffer();

        const uint32_t pcount = ui->parameterCount = carla_get_parameter_count(handle, 0);

        // make count of valid parameters
        for (uint32_t i=0; i < pcount; ++i)
        {
            const ParameterData* const pdata = carla_get_parameter_data(handle, 0, i);

            if (pdata->type != PARAMETER_INPUT ||
                (pdata->hints & PARAMETER_IS_ENABLED) == 0x0 ||
                (pdata->hints & PARAMETER_IS_READ_ONLY) != 0x0)
            {
                --ui->parameterCount;
                continue;
            }
        }

        ui->parameters = new PluginGenericUI::Parameter[ui->parameterCount];
        ui->values = new float[ui->parameterCount];

        // now safely fill in details
        for (uint32_t i=0, j=0; i < pcount; ++i)
        {
            const ParameterData* const pdata = carla_get_parameter_data(handle, 0, i);

            if (pdata->type != PARAMETER_INPUT ||
                (pdata->hints & PARAMETER_IS_ENABLED) == 0x0 ||
                (pdata->hints & PARAMETER_IS_READ_ONLY) != 0x0)
                continue;

            const CarlaParameterInfo* const pinfo = carla_get_parameter_info(handle, 0, i);
            const ::ParameterRanges* const pranges = carla_get_parameter_ranges(handle, 0, i);

            String format;

            if (pdata->hints & PARAMETER_IS_INTEGER)
                format = "%.0f ";
            else
                format = "%.3f ";

            format += pinfo->unit;

            PluginGenericUI::Parameter& param(ui->parameters[j]);
            param.name = strdup(pinfo->name);
            param.format = format.getAndReleaseBuffer();
            param.rindex = i;
            param.boolean = pdata->hints & PARAMETER_IS_BOOLEAN;
            param.min = pranges->min;
            param.max = pranges->max;
            ui->values[j] = carla_get_current_parameter_value(handle, 0, i);

            if (param.boolean)
                param.bvalue = ui->values[j] > param.min;
            else
                param.bvalue = false;

            ++j;
        }

        fPluginGenericUI = ui;
    }

    void updatePluginGenericUI(const CarlaHostHandle handle)
    {
        PluginGenericUI* const ui = fPluginGenericUI;
        DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr,);

        for (uint32_t i=0; i < ui->parameterCount; ++i)
        {
            ui->values[i] = carla_get_current_parameter_value(handle, 0, ui->parameters[i].rindex);

            if (ui->parameters[i].boolean)
                ui->parameters[i].bvalue = ui->values[i] > ui->parameters[i].min;
        }
    }

    bool loadPlugin(const CarlaHostHandle handle, const char* const label)
    {
        if (carla_get_current_plugin_count(handle) != 0)
        {
            hidePluginUI();
            carla_replace_plugin(handle, 0);
        }

        if (carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_LV2, nullptr, nullptr,
                             label, 0, 0x0, PLUGIN_OPTIONS_NULL))
        {
            fPluginGenericUI = nullptr;
            showPluginUI(handle);
            return true;
        }

        return false;
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

        case kDrawingPluginEmbedUI:
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

                if (! info->valid)
                    continue;

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
        case kDrawingPluginEmbedUI:
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
            const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

            if (ImGui::Button("Pick Another..."))
            {
                hidePluginUI();
                fDrawingState = kDrawingPluginList;

                const double scaleFactor = getScaleFactor();
                setSize(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);
            }

            ImGui::SameLine();

            if (ImGui::Button("Reset"))
            {
                loadPlugin(handle, carla_get_plugin_info(handle, 0)->label);
            }

            if (fDrawingState == kDrawingPluginGenericUI && fPluginHasCustomUI)
            {
                ImGui::SameLine();

                if (ImGui::Button("Show Custom GUI"))
                {
                    if (fPluginHasEmbedUI)
                    {
                        fDrawingState = kDrawingPluginEmbedUI;
                        carla_embed_custom_ui(handle, 0, fPluginHostWindow.attachAndGetWindowHandle());
                    }
                    else
                    {
                        carla_show_custom_ui(handle, 0, true);
                    }
                }
            }

            if (fDrawingState == kDrawingPluginEmbedUI)
            {
                ImGui::SameLine();

                if (ImGui::Button("Show Generic GUI"))
                {
                    hidePluginUI();
                    fDrawingState = kDrawingPluginGenericUI;

                    if (fPluginGenericUI == nullptr)
                        createPluginGenericUI(handle, carla_get_plugin_info(handle, 0));
                    else
                        updatePluginGenericUI(handle);

                    const double scaleFactor = getScaleFactor();
                    setSize(std::max(getWidth(), static_cast<uint>(kGenericWidth * scaleFactor + 0.5)),
                            (kGenericHeight + kExtraHeight) * scaleFactor);
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

        PluginGenericUI* const ui = fPluginGenericUI;
        DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr,);

        if (ImGui::Begin(ui->title, nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse))
        {
            const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

            for (uint32_t i=0; i < ui->parameterCount; ++i)
            {
                PluginGenericUI::Parameter& param(ui->parameters[i]);

                if (param.boolean)
                {
                    if (ImGui::Checkbox(param.name, &ui->parameters[i].bvalue))
                    {
                        if (ImGui::IsItemActivated())
                        {
                            carla_set_parameter_touch(handle, 0, param.rindex, true);
                            // editParameter(0, true);
                        }

                        ui->values[i] = ui->parameters[i].bvalue ? ui->parameters[i].max : ui->parameters[i].min;
                        carla_set_parameter_value(handle, 0, param.rindex, ui->values[i]);
                        // setParameterValue(0, ui->values[i]);
                    }
                }
                else
                {
                    if (ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.format))
                    {
                        if (ImGui::IsItemActivated())
                        {
                            carla_set_parameter_touch(handle, 0, param.rindex, true);
                            // editParameter(0, true);
                        }

                        carla_set_parameter_value(handle, 0, param.rindex, ui->values[i]);
                        // setParameterValue(0, ui->values[i]);
                    }
                }

                if (ImGui::IsItemDeactivated())
                {
                    carla_set_parameter_touch(handle, 0, param.rindex, false);
                    // editParameter(0, false);
                }
            }
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

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize))
        {
            if (ImGui::InputText("", fPluginSearchString, sizeof(fPluginSearchString)-1, ImGuiInputTextFlags_CharsNoBlank|ImGuiInputTextFlags_AutoSelectAll))
                fPluginSearchActive = true;

            ImGui::BeginDisabled(!fPluginScanningFinished);

            if (ImGui::Button("Load Plugin"))
            {
                do {
                    const PluginInfoCache& info(fPlugins[fPluginSelected]);

                    const char* const slash = std::strchr(info.label, DISTRHO_OS_SEP);
                    DISTRHO_SAFE_ASSERT_BREAK(slash != nullptr);

                    d_stdout("Loading %s...", info.name);

                    if (loadPlugin(handle, slash+1))
                    {
                        ImGui::EndDisabled();
                        ImGui::End();
                        return;
                    }
                } while (false);
            }

            if (carla_get_current_plugin_count(handle) != 0)
            {
                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                {
                    showPluginUI(handle);
                }
            }

            ImGui::EndDisabled();

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

// --------------------------------------------------------------------------------------------------------------------
void ildaeilParameterChangeForUI(void* const ui, const uint32_t index, const float value)
{
    DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr,);

    static_cast<IldaeilUI*>(ui)->changeParameterFromDSP(index, value);
}

/* --------------------------------------------------------------------------------------------------------------------
 * UI entry point, called by DPF to create a new UI instance. */

UI* createUI()
{
    return new IldaeilUI();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
