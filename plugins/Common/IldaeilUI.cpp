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
#include "DistrhoUI.hpp"

#include "CarlaBackendUtils.hpp"
#include "PluginHostWindow.hpp"
#include "extra/Thread.hpp"

// IDE helper
#include "DearImGui.hpp"

#include <vector>

// strcasestr
#ifdef DISTRHO_OS_WINDOWS
# include <shlwapi.h>
namespace ildaeil {
    inline const char* strcasestr(const char* const haystack, const char* const needle)
    {
        return StrStrIA(haystack, needle);
    }
    // using strcasestr = StrStrIA;
}
#else
namespace ildaeil {
    using ::strcasestr;
}
#endif

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

using namespace CARLA_BACKEND_NAMESPACE;

class IldaeilUI : public UI,
                  public Thread,
                  public PluginHostWindow::Callbacks
{
    static constexpr const uint kInitialWidth  = 520;
    static constexpr const uint kInitialHeight = 520;
    static constexpr const uint kGenericWidth  = 380;
    static constexpr const uint kGenericHeight = 400;
    static constexpr const uint kButtonHeight  = 20;

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
            char* printformat;
            uint32_t rindex;
            bool boolean, bvalue, log, readonly;
            float min, max, power;
            Parameter()
                : name(nullptr),
                  printformat(nullptr),
                  rindex(0),
                  boolean(false),
                  bvalue(false),
                  log(false),
                  min(0.0f),
                  max(1.0f) {}
            ~Parameter()
            {
                std::free(name);
                std::free(printformat);
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

    enum {
        kDrawingLoading,
        kDrawingPluginError,
        kDrawingPluginList,
        kDrawingPluginEmbedUI,
        kDrawingPluginGenericUI,
        kDrawingErrorInit,
        kDrawingErrorDraw
    } fDrawingState;

    enum {
        kIdleInit,
        kIdleInitPluginAlreadyLoaded,
        kIdleLoadSelectedPlugin,
        kIdlePluginLoadedFromDSP,
        kIdleResetPlugin,
        kIdleShowCustomUI,
        kIdleHideEmbedAndShowGenericUI,
        kIdleHidePluginUI,
        kIdleGiveIdleToUI,
        kIdleChangePluginType,
        kIdleNothing
    } fIdleState = kIdleInit;

    IldaeilBasePlugin* const fPlugin;
    PluginHostWindow fPluginHostWindow;

    PluginType fPluginType;
    PluginType fNextPluginType;
    uint fPluginCount;
    int fPluginSelected;
    bool fPluginScanningFinished;
    bool fPluginHasCustomUI;
    bool fPluginHasEmbedUI;
    bool fPluginHasOutputParameters;
    bool fPluginRunning;
    bool fPluginWillRunInBridgeMode;
    PluginInfoCache* fPlugins;
    ScopedPointer<PluginGenericUI> fPluginGenericUI;

    bool fPluginSearchActive;
    bool fPluginSearchFirstShow;
    char fPluginSearchString[0xff];

    String fPopupError;

    Size<uint> fNextSize;

public:
    IldaeilUI()
        : UI(kInitialWidth, kInitialHeight),
          Thread("IldaeilScanner"),
          fDrawingState(kDrawingLoading),
          fIdleState(kIdleInit),
          fPlugin((IldaeilBasePlugin*)getPluginInstancePointer()),
          fPluginHostWindow(getWindow(), this),
          fPluginType(PLUGIN_LV2),
          fNextPluginType(fPluginType),
          fPluginCount(0),
          fPluginSelected(-1),
          fPluginScanningFinished(false),
          fPluginHasCustomUI(false),
          fPluginHasEmbedUI(false),
          fPluginHasOutputParameters(false),
          fPluginRunning(false),
          fPluginWillRunInBridgeMode(false),
          fPlugins(nullptr),
          fPluginSearchActive(false),
          fPluginSearchFirstShow(false)
    {
        const double scaleFactor = getScaleFactor();

        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
        {
            fDrawingState = kDrawingErrorInit;
            fIdleState = kIdleNothing;
            fPopupError = "Ildaeil backend failed to init properly, cannot continue.";
            setSize(kInitialWidth * scaleFactor * 0.5, kInitialHeight * scaleFactor * 0.5);
            return;
        }

        std::strcpy(fPluginSearchString, "Search...");

        ImGuiStyle& style(ImGui::GetStyle());
        style.FrameRounding = 4;

        const double padding = style.WindowPadding.y * 2;

        if (d_isNotEqual(scaleFactor, 1.0))
        {
            setSize(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);
            fPluginHostWindow.setPositionAndSize(0, kButtonHeight * scaleFactor + padding,
                                                 kInitialWidth * scaleFactor,
                                                 (kInitialHeight - kButtonHeight) * scaleFactor - padding);
        }
        else
        {
            fPluginHostWindow.setPositionAndSize(0, kButtonHeight + padding,
                                                 kInitialWidth, kInitialHeight - kButtonHeight - padding);
        }

        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        char winIdStr[24];
        std::snprintf(winIdStr, sizeof(winIdStr), "%lx", (ulong)getWindow().getNativeWindowHandle());
        carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, winIdStr);
        carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_UI_SCALE, scaleFactor*1000, nullptr);

        if (checkIfPluginIsLoaded())
            fIdleState = kIdleInitPluginAlreadyLoaded;

        fPlugin->fUI = this;
    }

    ~IldaeilUI() override
    {
        if (fPlugin != nullptr && fPlugin->fCarlaHostHandle != nullptr)
        {
            fPlugin->fUI = nullptr;

            if (fPluginRunning)
                hidePluginUI(fPlugin->fCarlaHostHandle);

            carla_set_engine_option(fPlugin->fCarlaHostHandle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, "0");
        }

        if (isThreadRunning())
            stopThread(-1);

        fPluginGenericUI = nullptr;

        delete[] fPlugins;
    }

    bool checkIfPluginIsLoaded()
    {
        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        if (carla_get_current_plugin_count(handle) != 0)
        {
            const uint hints = carla_get_plugin_info(handle, 0)->hints;
            fPluginHasCustomUI = hints & PLUGIN_HAS_CUSTOM_UI;
            fPluginHasEmbedUI = hints & PLUGIN_HAS_CUSTOM_EMBED_UI;
            fPluginRunning = true;
            return true;
        }

        return false;
    }

    void projectLoadedFromDSP()
    {
        if (checkIfPluginIsLoaded())
            fIdleState = kIdlePluginLoadedFromDSP;
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

        repaint();
    }

    const char* openFileFromDSP(const bool /*isDir*/, const char* const title, const char* const /*filter*/)
    {
        DISTRHO_SAFE_ASSERT_RETURN(fPluginType == PLUGIN_INTERNAL || fPluginType == PLUGIN_LV2, nullptr);

        FileBrowserOptions opts;
        opts.title = title;
        openFileBrowser(opts);
        return nullptr;
    }

    void showPluginUI(const CarlaHostHandle handle, const bool showIfNotEmbed)
    {
        const CarlaPluginInfo* const info = carla_get_plugin_info(handle, 0);

        if (info->hints & PLUGIN_HAS_CUSTOM_EMBED_UI)
        {
            fDrawingState = kDrawingPluginEmbedUI;
            fIdleState = kIdleGiveIdleToUI;
            fPluginHasCustomUI = true;
            fPluginHasEmbedUI = true;

            carla_embed_custom_ui(handle, 0, fPluginHostWindow.attachAndGetWindowHandle());
        }
        else
        {
            createOrUpdatePluginGenericUI(handle);

            if (showIfNotEmbed && fPluginHasCustomUI)
            {
                fIdleState = kIdleGiveIdleToUI;
                carla_show_custom_ui(handle, 0, true);
            }
        }

        repaint();
    }

    void hidePluginUI(const CarlaHostHandle handle)
    {
        DISTRHO_SAFE_ASSERT_RETURN(fPluginRunning,);

        fPluginHostWindow.hide();
        carla_show_custom_ui(handle, 0, false);
    }

    void createOrUpdatePluginGenericUI(const CarlaHostHandle handle, const CarlaPluginInfo* info = nullptr)
    {
        if (info == nullptr)
            info = carla_get_plugin_info(handle, 0);

        fDrawingState = kDrawingPluginGenericUI;
        fPluginHasCustomUI = info->hints & PLUGIN_HAS_CUSTOM_UI;
        fPluginHasEmbedUI = info->hints & PLUGIN_HAS_CUSTOM_EMBED_UI;

        if (fPluginGenericUI == nullptr)
            createPluginGenericUI(handle, info);
        else
            updatePluginGenericUI(handle);

        ImGuiStyle& style(ImGui::GetStyle());
        const double scaleFactor = getScaleFactor();
        fNextSize = Size<uint>(kGenericWidth * scaleFactor, (kGenericHeight + style.FramePadding.x) * scaleFactor);
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

            if ((pdata->hints & PARAMETER_IS_ENABLED) == 0x0)
            {
                --ui->parameterCount;
                continue;
            }

            if (pdata->type == PARAMETER_OUTPUT)
                fPluginHasOutputParameters = true;
        }

        ui->parameters = new PluginGenericUI::Parameter[ui->parameterCount];
        ui->values = new float[ui->parameterCount];

        // now safely fill in details
        for (uint32_t i=0, j=0; i < pcount; ++i)
        {
            const ParameterData* const pdata = carla_get_parameter_data(handle, 0, i);

            if ((pdata->hints & PARAMETER_IS_ENABLED) == 0x0)
                continue;

            const CarlaParameterInfo* const pinfo = carla_get_parameter_info(handle, 0, i);
            const ::ParameterRanges* const pranges = carla_get_parameter_ranges(handle, 0, i);

            String printformat;

            if (pdata->hints & PARAMETER_IS_INTEGER)
                printformat = "%.0f ";
            else
                printformat = "%.3f ";

            printformat += pinfo->unit;

            PluginGenericUI::Parameter& param(ui->parameters[j]);
            param.name = strdup(pinfo->name);
            param.printformat = printformat.getAndReleaseBuffer();
            param.rindex = i;
            param.boolean = pdata->hints & PARAMETER_IS_BOOLEAN;
            param.log = pdata->hints & PARAMETER_IS_LOGARITHMIC;
            param.readonly = pdata->type != PARAMETER_INPUT || (pdata->hints & PARAMETER_IS_READ_ONLY);
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

    void loadPlugin(const CarlaHostHandle handle, const char* const label)
    {
        if (fPluginRunning)
        {
            hidePluginUI(handle);
            carla_replace_plugin(handle, 0);
        }

        carla_set_engine_option(handle, ENGINE_OPTION_PREFER_PLUGIN_BRIDGES, fPluginWillRunInBridgeMode, nullptr);

        const MutexLocker cml(fPlugin->sPluginInfoLoadMutex);

        if (carla_add_plugin(handle, BINARY_NATIVE, fPluginType, nullptr, nullptr,
                             label, 0, 0x0, PLUGIN_OPTIONS_NULL))
        {
            fPluginRunning = true;
            fPluginGenericUI = nullptr;
            showPluginUI(handle, false);
        }
        else
        {
            fPopupError = carla_get_last_error(handle);
            d_stdout("got error: %s", fPopupError.buffer());
            fDrawingState = kDrawingPluginError;
        }

        repaint();
    }

protected:
    void pluginWindowResized(const uint width, const uint height) override
    {
        const uint extraHeight = kButtonHeight * getScaleFactor() + ImGui::GetStyle().WindowPadding.y * 2;

        fNextSize = Size<uint>(width, height + extraHeight);
    }

    void uiIdle() override
    {
        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;
        DISTRHO_SAFE_ASSERT_RETURN(handle != nullptr,);

        // carla_juce_idle();

        if (fDrawingState == kDrawingPluginGenericUI && fPluginGenericUI != nullptr && fPluginHasOutputParameters)
        {
            updatePluginGenericUI(handle);
            repaint();
        }

        if (fNextSize.isValid())
        {
            setSize(fNextSize);
            fNextSize = Size<uint>();
        }

        switch (fIdleState)
        {
        case kIdleInit:
            fIdleState = kIdleNothing;
            startThread();
            break;

        case kIdleInitPluginAlreadyLoaded:
            fIdleState = kIdleNothing;
            showPluginUI(handle, false);
            startThread();
            break;

        case kIdlePluginLoadedFromDSP:
            fIdleState = kIdleNothing;
            showPluginUI(handle, false);
            break;

        case kIdleLoadSelectedPlugin:
            fIdleState = kIdleNothing;
            loadSelectedPlugin(handle);
            break;

        case kIdleResetPlugin:
            fIdleState = kIdleNothing;
            loadPlugin(handle, carla_get_plugin_info(handle, 0)->label);
            break;

        case kIdleShowCustomUI:
            fIdleState = kIdleNothing;
            showPluginUI(handle, true);
            break;

        case kIdleHideEmbedAndShowGenericUI:
            fIdleState = kIdleNothing;
            hidePluginUI(handle);
            createOrUpdatePluginGenericUI(handle);
            break;

        case kIdleHidePluginUI:
            fIdleState = kIdleNothing;
            carla_show_custom_ui(handle, 0, false);
            break;

        case kIdleGiveIdleToUI:
            fPlugin->fCarlaPluginDescriptor->ui_idle(fPlugin->fCarlaPluginHandle);
            fPluginHostWindow.idle();
            break;

        case kIdleChangePluginType:
            fIdleState = kIdleNothing;
            if (fPluginRunning)
                hidePluginUI(handle);
            fPluginSelected = -1;
            if (isThreadRunning())
                stopThread(-1);
            fPluginType = fNextPluginType;
            startThread();
            break;

        case kIdleNothing:
            break;
        }
    }

    void loadSelectedPlugin(const CarlaHostHandle handle)
    {
        DISTRHO_SAFE_ASSERT_RETURN(fPluginSelected >= 0,);

        const PluginInfoCache& info(fPlugins[fPluginSelected]);

        const char* label = nullptr;

        switch (fPluginType)
        {
        case PLUGIN_INTERNAL:
        case PLUGIN_AU:
        // case PLUGIN_JSFX:
        case PLUGIN_SFZ:
            label = info.label;
            break;
        case PLUGIN_LV2: {
            const char* const slash = std::strchr(info.label, DISTRHO_OS_SEP);
            DISTRHO_SAFE_ASSERT_RETURN(slash != nullptr,);
            label = slash+1;
            break;
        }
        default:
            break;
        }

        DISTRHO_SAFE_ASSERT_RETURN(label != nullptr,);

        d_stdout("Loading %s...", info.name);
        loadPlugin(handle, label);
    }

    void uiFileBrowserSelected(const char* const filename) override
    {
        if (fPlugin != nullptr && fPlugin->fCarlaHostHandle != nullptr && filename != nullptr)
            carla_set_custom_data(fPlugin->fCarlaHostHandle, 0, CUSTOM_DATA_TYPE_STRING, "file", filename);
    }

    void run() override
    {
        const char* path;
        switch (fPluginType)
        {
        case PLUGIN_LV2:
            path = std::getenv("LV2_PATH");
            break;
        default:
            path = nullptr;
            break;
        }

        if (path != nullptr)
            carla_set_engine_option(fPlugin->fCarlaHostHandle, ENGINE_OPTION_PLUGIN_PATH, fPluginType, path);

        fPluginCount = 0;
        delete[] fPlugins;

        uint count;

        {
            const MutexLocker cml(fPlugin->sPluginInfoLoadMutex);

            d_stdout("Will scan plugins now...");
            count = carla_get_cached_plugin_count(fPluginType, path);
            d_stdout("Scanning found %u plugins", count);
        }

        if (fDrawingState == kDrawingLoading)
        {
            fDrawingState = kDrawingPluginList;
            fPluginSearchFirstShow = true;
        }

        if (count != 0)
        {
            fPlugins = new PluginInfoCache[count];

            for (uint i=0, j; i < count && ! shouldThreadExit(); ++i)
            {
                const MutexLocker cml(fPlugin->sPluginInfoLoadMutex);

                const CarlaCachedPluginInfo* const info = carla_get_cached_plugin_info(fPluginType, i);
                DISTRHO_SAFE_ASSERT_CONTINUE(info != nullptr);

                if (! info->valid)
                    continue;

                if (info->cvIns != 0 || info->cvOuts != 0)
                    continue;

                #if DISTRHO_PLUGIN_IS_SYNTH
                if (info->midiIns != 1 && info->audioIns != 0)
                    continue;
                if ((info->hints & PLUGIN_IS_SYNTH) == 0x0 && info->audioIns != 0)
                    continue;
                if (info->audioOuts != 1 && info->audioOuts != 2)
                    continue;
                #elif DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
                if ((info->midiIns != 1 && info->audioIns != 0 && info->audioOuts != 0) || info->midiOuts != 1)
                    continue;
                if (info->audioIns != 0 || info->audioOuts != 0)
                    continue;
                #else
                if (info->audioIns != 1 && info->audioIns != 2)
                    continue;
                if (info->audioOuts != 1 && info->audioOuts != 2)
                    continue;
                #endif

                if (fPluginType == PLUGIN_INTERNAL)
                {
                    if (std::strcmp(info->label, "audiogain_s") == 0)
                        continue;
                    if (std::strcmp(info->label, "cv2audio") == 0)
                        continue;
                    if (std::strcmp(info->label, "lfo") == 0)
                        continue;
                    if (std::strcmp(info->label, "midi2cv") == 0)
                        continue;
                    if (std::strcmp(info->label, "midithrough") == 0)
                        continue;
                    if (std::strcmp(info->label, "3bandsplitter") == 0)
                        continue;
                }

                j = fPluginCount;
                fPlugins[j].name = strdup(info->name);
                fPlugins[j].label = strdup(info->label);
                ++fPluginCount;
            }
        }
        else
        {
            fPlugins = nullptr;
        }

        if (! shouldThreadExit())
            fPluginScanningFinished = true;
    }

    void onImGuiDisplay() override
    {
        switch (fDrawingState)
        {
        case kDrawingLoading:
            drawLoading();
            break;
        case kDrawingPluginError:
            ImGui::OpenPopup("Plugin Error");
            // call ourselves again with the plugin list
            fDrawingState = kDrawingPluginList;
            onImGuiDisplay();
            break;
        case kDrawingPluginList:
            drawPluginList();
            break;
        case kDrawingPluginGenericUI:
            drawTopBar();
            drawGenericUI();
            break;
        case kDrawingPluginEmbedUI:
            drawTopBar();
            break;
        case kDrawingErrorInit:
            fDrawingState = kDrawingErrorDraw;
            drawError(true);
            break;
        case kDrawingErrorDraw:
            drawError(false);
            break;
        }
    }

    void drawError(const bool open)
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), getHeight()));

        const int flags = ImGuiWindowFlags_NoSavedSettings
                        | ImGuiWindowFlags_NoTitleBar
                        | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoCollapse
                        | ImGuiWindowFlags_NoScrollbar
                        | ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin("Error Window", nullptr, flags))
        {
            if (open)
                ImGui::OpenPopup("Engine Error");

            const int pflags = ImGuiWindowFlags_NoSavedSettings
                             | ImGuiWindowFlags_NoResize
                             | ImGuiWindowFlags_NoCollapse
                             | ImGuiWindowFlags_NoScrollbar
                             | ImGuiWindowFlags_NoScrollWithMouse
                             | ImGuiWindowFlags_AlwaysAutoResize
                             | ImGuiWindowFlags_AlwaysUseWindowPadding;

            if (ImGui::BeginPopupModal("Engine Error", nullptr, pflags))
            {
                ImGui::TextUnformatted(fPopupError.buffer(), nullptr);
                ImGui::EndPopup();
            }
        }

        ImGui::End();
    }

    void drawTopBar()
    {
        const float padding = ImGui::GetStyle().WindowPadding.y * 2;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), kButtonHeight * getScaleFactor() + padding));

        const int flags = ImGuiWindowFlags_NoSavedSettings
                        | ImGuiWindowFlags_NoTitleBar
                        | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoCollapse
                        | ImGuiWindowFlags_NoScrollbar
                        | ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin("Current Plugin", nullptr, flags))
        {
            if (ImGui::Button("Pick Another..."))
            {
                fIdleState = kIdleHidePluginUI;
                fDrawingState = kDrawingPluginList;

                const double scaleFactor = getScaleFactor();
                fNextSize = Size<uint>(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);
            }

            ImGui::SameLine();

            if (ImGui::Button("Reset"))
                fIdleState = kIdleResetPlugin;

            if (fDrawingState == kDrawingPluginGenericUI && fPluginHasCustomUI)
            {
                ImGui::SameLine();

                if (ImGui::Button("Show Custom GUI"))
                    fIdleState = kIdleShowCustomUI;
            }

            if (fDrawingState == kDrawingPluginEmbedUI)
            {
                ImGui::SameLine();

                if (ImGui::Button("Show Generic GUI"))
                    fIdleState = kIdleHideEmbedAndShowGenericUI;
            }
        }

        ImGui::End();
    }

    void setupMainWindowPos()
    {
        const float scaleFactor = getScaleFactor();

        float y = 0;
        float height = getHeight();

        if (fDrawingState == kDrawingPluginGenericUI)
        {
            y = kButtonHeight * scaleFactor + ImGui::GetStyle().WindowPadding.y * 2 - scaleFactor;
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

        const int pflags = ImGuiWindowFlags_NoSavedSettings
                         | ImGuiWindowFlags_NoResize
                         | ImGuiWindowFlags_NoCollapse
                         | ImGuiWindowFlags_AlwaysAutoResize;

        if (ImGui::Begin(ui->title, nullptr, pflags))
        {
            const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

            for (uint32_t i=0; i < ui->parameterCount; ++i)
            {
                PluginGenericUI::Parameter& param(ui->parameters[i]);

                if (param.readonly)
                {
                    ImGui::BeginDisabled();
                    ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.printformat, ImGuiSliderFlags_NoInput);
                    ImGui::EndDisabled();
                    continue;
                }

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
                    const bool ret = param.log
                                   ? ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.printformat, 2.0f)
                                   : ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.printformat);
                    if (ret)
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
            ImGui::TextUnformatted("Loading...", nullptr);

        ImGui::End();
    }

    void drawPluginList()
    {
        static const char* pluginTypes[] = {
            getPluginTypeAsString(PLUGIN_INTERNAL),
            getPluginTypeAsString(PLUGIN_LV2),
        };

        setupMainWindowPos();

        if (ImGui::Begin("Plugin List", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize))
        {
            const int pflags = ImGuiWindowFlags_NoSavedSettings
                             | ImGuiWindowFlags_NoResize
                             | ImGuiWindowFlags_NoCollapse
                             | ImGuiWindowFlags_NoScrollbar
                             | ImGuiWindowFlags_NoScrollWithMouse
                             | ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGui::BeginPopupModal("Plugin Error", nullptr, pflags))
            {
                ImGui::TextWrapped("Failed to load plugin, error was:\n%s", fPopupError.buffer());

                ImGui::Separator();

                if (ImGui::Button("Ok"))
                    ImGui::CloseCurrentPopup();

                ImGui::SameLine();
                ImGui::Dummy(ImVec2(500 * getScaleFactor(), 1));
                ImGui::EndPopup();
            }
            else if (fPluginSearchFirstShow)
            {
                fPluginSearchFirstShow = false;
                ImGui::SetKeyboardFocusHere();
            }

            if (ImGui::InputText("##pluginsearch", fPluginSearchString, sizeof(fPluginSearchString)-1,
                                 ImGuiInputTextFlags_CharsNoBlank|ImGuiInputTextFlags_AutoSelectAll))
                fPluginSearchActive = true;

            if (ImGui::IsKeyDown(ImGuiKey_Escape))
                fPluginSearchActive = false;

            ImGui::SameLine();
            ImGui::PushItemWidth(-1.0f);

            int current;
            switch (fPluginType)
            {
            case PLUGIN_LV2:
                current = 1;
                break;
            default:
                current = 0;
                break;
            }

            if (ImGui::Combo("##plugintypes", &current, pluginTypes, ARRAY_SIZE(pluginTypes)))
            {
                fIdleState = kIdleChangePluginType;
                switch (current)
                {
                case 0:
                    fNextPluginType = PLUGIN_INTERNAL;
                    break;
                case 1:
                    fNextPluginType = PLUGIN_LV2;
                    break;
                }
            }

            ImGui::BeginDisabled(!fPluginScanningFinished || fPluginSelected < 0);

            if (ImGui::Button("Load Plugin"))
                fIdleState = kIdleLoadSelectedPlugin;

            // xx cardinal
            if (fPluginType != PLUGIN_INTERNAL /*&& module->canUseBridges*/)
            {
                ImGui::SameLine();
                ImGui::Checkbox("Run in bridge mode", &fPluginWillRunInBridgeMode);
            }

            ImGui::EndDisabled();

            if (fPluginRunning)
            {
                ImGui::SameLine();

                if (ImGui::Button("Cancel"))
                    fIdleState = kIdleShowCustomUI;
            }

            if (ImGui::BeginChild("pluginlistwindow"))
            {
                if (ImGui::BeginTable("pluginlist", 2, ImGuiTableFlags_NoSavedSettings))
                {
                    const char* const search = fPluginSearchActive && fPluginSearchString[0] != '\0' ? fPluginSearchString : nullptr;

                    switch (fPluginType)
                    {
                    case PLUGIN_INTERNAL:
                    case PLUGIN_AU:
                    case PLUGIN_SFZ:
                    // case PLUGIN_JSFX:
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("Label");
                        ImGui::TableHeadersRow();
                        break;
                    case PLUGIN_LV2:
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("URI");
                        ImGui::TableHeadersRow();
                        break;
                    default:
                        break;
                    }

                    for (uint i=0; i<fPluginCount; ++i)
                    {
                        const PluginInfoCache& info(fPlugins[i]);

                        if (search != nullptr && ildaeil::strcasestr(info.name, search) == nullptr)
                            continue;

                        bool selected = fPluginSelected >= 0 && static_cast<uint>(fPluginSelected) == i;

                        switch (fPluginType)
                        {
                        case PLUGIN_INTERNAL:
                        case PLUGIN_AU:
                        // case PLUGIN_JSFX:
                        case PLUGIN_SFZ:
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Selectable(info.name, &selected);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Selectable(info.label, &selected);
                            break;
                        case PLUGIN_LV2: {
                            const char* const slash = std::strchr(info.label, DISTRHO_OS_SEP);
                            DISTRHO_SAFE_ASSERT_CONTINUE(slash != nullptr);
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Selectable(info.name, &selected);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Selectable(slash+1, &selected);
                            break;
                        }
                        default:
                            break;
                        }

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
            hidePluginUI(fPlugin->fCarlaHostHandle);
    }

    // -------------------------------------------------------------------------------------------------------

private:
   /**
      Set our UI class as non-copyable and add a leak detector just in case.
    */
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IldaeilUI)
};

// --------------------------------------------------------------------------------------------------------------------

void ildaeilProjectLoadedFromDSP(void* const ui)
{
    DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr,);

    static_cast<IldaeilUI*>(ui)->projectLoadedFromDSP();
}

void ildaeilParameterChangeForUI(void* const ui, const uint32_t index, const float value)
{
    DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr,);

    static_cast<IldaeilUI*>(ui)->changeParameterFromDSP(index, value);
}

const char* ildaeilOpenFileForUI(void* const ui, const bool isDir, const char* const title, const char* const filter)
{
    DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr, nullptr);

    return static_cast<IldaeilUI*>(ui)->openFileFromDSP(isDir, title, filter);
}

/* --------------------------------------------------------------------------------------------------------------------
 * UI entry point, called by DPF to create a new UI instance. */

UI* createUI()
{
    return new IldaeilUI();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
