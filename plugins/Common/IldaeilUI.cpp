/*
 * DISTRHO Ildaeil Plugin
 * Copyright (C) 2021-2023 Filipe Coelho <falktx@falktx.com>
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

#if ILDAEIL_STANDALONE
#include "DistrhoStandaloneUtils.hpp"
#endif

#include "CarlaBackendUtils.hpp"
#include "PluginHostWindow.hpp"
#include "extra/Runner.hpp"

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

// #define WASM_TESTING

START_NAMESPACE_DISTRHO

using namespace CARLA_BACKEND_NAMESPACE;

// --------------------------------------------------------------------------------------------------------------------

class IldaeilUI : public UI,
                  public Runner,
                  public PluginHostWindow::Callbacks
{
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
        kIdleOpenFileUI,
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
    uint fPluginId;
    int fPluginSelected;
    bool fPluginScanningFinished;
    bool fPluginHasCustomUI;
    bool fPluginHasEmbedUI;
    bool fPluginHasFileOpen;
    bool fPluginHasOutputParameters;
    bool fPluginRunning;
    bool fPluginWillRunInBridgeMode;
    PluginInfoCache* fPlugins;
    ScopedPointer<PluginGenericUI> fPluginGenericUI;

    bool fPluginSearchActive;
    bool fPluginSearchFirstShow;
    char fPluginSearchString[0xff];

    String fPopupError, fPluginFilename;
    Size<uint> fNextSize;

    struct RunnerData {
        bool needsReinit;
        uint pluginCount;
        uint pluginIndex;

        RunnerData()
          : needsReinit(true),
            pluginCount(0),
            pluginIndex(0) {}
        
        void init()
        {
            needsReinit = true;
            pluginCount = 0;
            pluginIndex = 0;
        }
    } fRunnerData;

public:
    IldaeilUI()
        : UI(kInitialWidth, kInitialHeight),
          Runner("IldaeilScanner"),
          fDrawingState(kDrawingLoading),
          fIdleState(kIdleInit),
          fPlugin((IldaeilBasePlugin*)getPluginInstancePointer()),
          fPluginHostWindow(getWindow(), this),
          fPluginType(PLUGIN_LV2),
          fNextPluginType(fPluginType),
          fPluginCount(0),
          fPluginId(0),
          fPluginSelected(-1),
          fPluginScanningFinished(false),
          fPluginHasCustomUI(false),
          fPluginHasEmbedUI(false),
          fPluginHasFileOpen(false),
          fPluginHasOutputParameters(false),
          fPluginRunning(false),
          fPluginWillRunInBridgeMode(false),
          fPlugins(nullptr),
          fPluginSearchActive(false),
          fPluginSearchFirstShow(false),
          fRunnerData()
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

        const double paddingY = style.WindowPadding.y * 2;

        if (d_isNotEqual(scaleFactor, 1.0))
        {
            setSize(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);
            fPluginHostWindow.setPositionAndSize(0, kButtonHeight * scaleFactor + paddingY,
                                                 kInitialWidth * scaleFactor,
                                                 (kInitialHeight - kButtonHeight) * scaleFactor - paddingY);
        }
        else
        {
            fPluginHostWindow.setPositionAndSize(0, kButtonHeight + paddingY,
                                                 kInitialWidth, kInitialHeight - kButtonHeight - paddingY);
        }

        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        char winIdStr[24];
        std::snprintf(winIdStr, sizeof(winIdStr), "%lx", (ulong)getWindow().getNativeWindowHandle());
        carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_WIN_ID, 0, winIdStr);
        carla_set_engine_option(handle, ENGINE_OPTION_FRONTEND_UI_SCALE, scaleFactor*1000, nullptr);

        if (checkIfPluginIsLoaded())
            fIdleState = kIdleInitPluginAlreadyLoaded;

        fPlugin->fUI = this;

#ifdef WASM_TESTING
        if (carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_INTERNAL, nullptr, nullptr,
                             "midifile", 0, 0x0, PLUGIN_OPTIONS_NULL))
        {
            d_stdout("Special hack for MIDI file playback activated");
            carla_set_custom_data(handle, 0, CUSTOM_DATA_TYPE_PATH, "file", "/furelise.mid");
            carla_set_parameter_value(handle, 0, 0, 1.0f);
            carla_set_parameter_value(handle, 0, 1, 0.0f);
            fPluginId = 2;
        }
        carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_INTERNAL, nullptr, nullptr, "miditranspose", 0, 0x0, PLUGIN_OPTIONS_NULL);
        carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_INTERNAL, nullptr, nullptr, "bypass", 0, 0x0, PLUGIN_OPTIONS_NULL);
        carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_INTERNAL, nullptr, nullptr, "3bandeq", 0, 0x0, PLUGIN_OPTIONS_NULL);
        carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_INTERNAL, nullptr, nullptr, "pingpongpan", 0, 0x0, PLUGIN_OPTIONS_NULL);
            carla_set_parameter_value(handle, 4, 1, 0.0f);
        carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_INTERNAL, nullptr, nullptr, "audiogain_s", 0, 0x0, PLUGIN_OPTIONS_NULL);
        for (uint i=0; i<5; ++i)
            carla_add_plugin(handle, BINARY_NATIVE, PLUGIN_INTERNAL, nullptr, nullptr, "bypass", 0, 0x0, PLUGIN_OPTIONS_NULL);
#endif
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

        stopRunner();
        fPluginGenericUI = nullptr;

        delete[] fPlugins;
    }

    bool checkIfPluginIsLoaded()
    {
        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;

        if (carla_get_current_plugin_count(handle) == 0)
            return false;

        const uint hints = carla_get_plugin_info(handle, fPluginId)->hints;
        updatePluginFlags(hints);

        fPluginRunning = true;
        return true;
    }

    void updatePluginFlags(const uint hints) noexcept
    {
        if (hints & PLUGIN_HAS_CUSTOM_UI_USING_FILE_OPEN)
        {
            fPluginHasCustomUI = false;
            fPluginHasEmbedUI = false;
            fPluginHasFileOpen = true;
        }
        else
        {
            fPluginHasCustomUI = hints & PLUGIN_HAS_CUSTOM_UI;
           #ifndef DISTRHO_OS_WASM
            fPluginHasEmbedUI = hints & PLUGIN_HAS_CUSTOM_EMBED_UI;
           #endif
            fPluginHasFileOpen = false;
        }
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

    void closeUI()
    {
        if (fIdleState == kIdleGiveIdleToUI)
            fIdleState = kIdleNothing;
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
        const uint hints = carla_get_plugin_info(handle, fPluginId)->hints;

       #ifndef DISTRHO_OS_WASM
        if (hints & PLUGIN_HAS_CUSTOM_EMBED_UI)
        {
            fDrawingState = kDrawingPluginEmbedUI;
            fIdleState = kIdleGiveIdleToUI;
            fPluginHasCustomUI = true;
            fPluginHasEmbedUI = true;
            fPluginHasFileOpen = false;

            carla_embed_custom_ui(handle, fPluginId, fPluginHostWindow.attachAndGetWindowHandle());
        }
        else
       #endif
        {
            // fPluginHas* flags are updated in the next function
            createOrUpdatePluginGenericUI(handle);

            if (showIfNotEmbed && fPluginHasCustomUI)
            {
                fIdleState = kIdleGiveIdleToUI;
                carla_show_custom_ui(handle, fPluginId, true);
            }
        }

        repaint();
    }

    void hidePluginUI(const CarlaHostHandle handle)
    {
        DISTRHO_SAFE_ASSERT_RETURN(fPluginRunning,);

        if (fPluginHostWindow.hide())
            carla_show_custom_ui(handle, fPluginId, false);
    }

    void createOrUpdatePluginGenericUI(const CarlaHostHandle handle, const CarlaPluginInfo* info = nullptr)
    {
        if (info == nullptr)
            info = carla_get_plugin_info(handle, fPluginId);

        fDrawingState = kDrawingPluginGenericUI;
        updatePluginFlags(info->hints);

        if (fPluginGenericUI == nullptr)
            createPluginGenericUI(handle, info);
        else
            updatePluginGenericUI(handle);

       #if !ILDAEIL_STANDALONE
        const double scaleFactor = getScaleFactor();
        fNextSize = Size<uint>(kGenericWidth * scaleFactor,
                               (kGenericHeight + ImGui::GetStyle().WindowPadding.y) * scaleFactor);
       #endif
    }

    void createPluginGenericUI(const CarlaHostHandle handle, const CarlaPluginInfo* const info)
    {
        PluginGenericUI* const ui = new PluginGenericUI;

        String title(info->name);
        title += " by ";
        title += info->maker;
        ui->title = title.getAndReleaseBuffer();

        const uint32_t pcount = ui->parameterCount = carla_get_parameter_count(handle, fPluginId);

        // make count of valid parameters
        for (uint32_t i=0; i < pcount; ++i)
        {
            const ParameterData* const pdata = carla_get_parameter_data(handle, fPluginId, i);

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
            const ParameterData* const pdata = carla_get_parameter_data(handle, fPluginId, i);

            if ((pdata->hints & PARAMETER_IS_ENABLED) == 0x0)
                continue;

            const CarlaParameterInfo* const pinfo = carla_get_parameter_info(handle, fPluginId, i);
            const ::ParameterRanges* const pranges = carla_get_parameter_ranges(handle, fPluginId, i);

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

            ui->values[j] = carla_get_current_parameter_value(handle, fPluginId, i);

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
            ui->values[i] = carla_get_current_parameter_value(handle, fPluginId, ui->parameters[i].rindex);

            if (ui->parameters[i].boolean)
                ui->parameters[i].bvalue = ui->values[i] > ui->parameters[i].min;
        }
    }

    void loadPlugin(const CarlaHostHandle handle, const char* const label)
    {
        if (fPluginRunning || fPluginId != 0)
        {
            hidePluginUI(handle);
            carla_replace_plugin(handle, fPluginId);
        }

        carla_set_engine_option(handle, ENGINE_OPTION_PREFER_PLUGIN_BRIDGES, fPluginWillRunInBridgeMode, nullptr);

        const MutexLocker cml(fPlugin->sPluginInfoLoadMutex);

        if (carla_add_plugin(handle, BINARY_NATIVE, fPluginType, nullptr, nullptr,
                             label, 0, 0x0, PLUGIN_OPTIONS_NULL))
        {
            fPluginRunning = true;
            fPluginGenericUI = nullptr;
            fPluginFilename.clear();
            showPluginUI(handle, false);

#ifdef WASM_TESTING
            d_stdout("loaded a plugin with label '%s'", label);

            if (std::strcmp(label, "audiofile") == 0)
            {
                d_stdout("Loading mp3 file into audiofile plugin");
                carla_set_custom_data(handle, fPluginId, CUSTOM_DATA_TYPE_PATH, "file", "/foolme.mp3");
                carla_set_parameter_value(handle, fPluginId, 1, 0.0f);
                fPluginGenericUI->values[1] = 0.0f;
            }
#endif
        }
        else
        {
            fPopupError = carla_get_last_error(handle);
            d_stdout("got error: %s", fPopupError.buffer());
            fDrawingState = kDrawingPluginError;
        }

        repaint();
    }

    void loadFileAsPlugin(const CarlaHostHandle handle, const char* const filename)
    {
        if (fPluginRunning || fPluginId != 0)
        {
            hidePluginUI(handle);
            carla_replace_plugin(handle, fPluginId);
        }

        carla_set_engine_option(handle, ENGINE_OPTION_PREFER_PLUGIN_BRIDGES, fPluginWillRunInBridgeMode, nullptr);

        const MutexLocker cml(fPlugin->sPluginInfoLoadMutex);

        if (carla_load_file(handle, filename))
        {
            fPluginRunning = true;
            fPluginGenericUI = nullptr;
            fPluginFilename = filename;
            showPluginUI(handle, false);
        }
        else
        {
            fPopupError = carla_get_last_error(handle);
            d_stdout("got error: %s", fPopupError.buffer());
            fDrawingState = kDrawingPluginError;
            fPluginFilename.clear();
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
            initAndStartRunner();
            break;

        case kIdleInitPluginAlreadyLoaded:
            fIdleState = kIdleNothing;
            showPluginUI(handle, false);
            initAndStartRunner();
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
            if (fPluginFilename.isNotEmpty())
                loadFileAsPlugin(handle, fPluginFilename.buffer());
            else
                loadPlugin(handle, carla_get_plugin_info(handle, fPluginId)->label);
            break;

        case kIdleOpenFileUI:
            fIdleState = kIdleNothing;
            carla_show_custom_ui(handle, fPluginId, true);
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
            hidePluginUI(handle);
            break;

        case kIdleGiveIdleToUI:
            if (fPlugin->fCarlaPluginDescriptor->ui_idle != nullptr)
                fPlugin->fCarlaPluginDescriptor->ui_idle(fPlugin->fCarlaPluginHandle);
            fPluginHostWindow.idle();
            break;

        case kIdleChangePluginType:
            fIdleState = kIdleNothing;
            if (fPluginRunning)
                hidePluginUI(handle);
            if (fNextPluginType == PLUGIN_TYPE_COUNT)
            {
                FileBrowserOptions opts;
                opts.title = "Load from file";
                openFileBrowser(opts);
            }
            else
            {
                fPluginSelected = -1;
                stopRunner();
                fPluginType = fNextPluginType;
                initAndStartRunner();
            }
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
        case PLUGIN_JSFX:
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
        {
            if (fNextPluginType == PLUGIN_TYPE_COUNT)
                loadFileAsPlugin(fPlugin->fCarlaHostHandle, filename);
            else
                carla_set_custom_data(fPlugin->fCarlaHostHandle, fPluginId, CUSTOM_DATA_TYPE_PATH, "file", filename);
        }
    }

    bool initAndStartRunner()
    {
        if (isRunnerActive())
            stopRunner();

        fRunnerData.init();
        return startRunner();
    }

    bool run() override
    {
        if (fRunnerData.needsReinit)
        {
            fRunnerData.needsReinit = false;

            const char* path;
            switch (fPluginType)
            {
            case PLUGIN_LV2:
                path = std::getenv("LV2_PATH");
                break;
            case PLUGIN_JSFX:
                path = fPlugin->getPathForJSFX();
                break;
            default:
                path = nullptr;
                break;
            }

            fPluginCount = 0;
            delete[] fPlugins;

            {
                const MutexLocker cml(fPlugin->sPluginInfoLoadMutex);

                d_stdout("Will scan plugins now...");
                fRunnerData.pluginCount = carla_get_cached_plugin_count(fPluginType, path);
                d_stdout("Scanning found %u plugins", fRunnerData.pluginCount);
            }

            if (fDrawingState == kDrawingLoading)
            {
                fDrawingState = kDrawingPluginList;
                fPluginSearchFirstShow = true;
            }

            if (fRunnerData.pluginCount != 0)
            {
                fPlugins = new PluginInfoCache[fRunnerData.pluginCount];
                fPluginScanningFinished = false;
                return true;
            }
            else
            {
                fPlugins = nullptr;
                fPluginScanningFinished = true;
                return false;
            }
        }

        const uint index = fRunnerData.pluginIndex++;
        DISTRHO_SAFE_ASSERT_UINT2_RETURN(index < fRunnerData.pluginCount,
                                         index, fRunnerData.pluginCount, false);

        do {
            const MutexLocker cml(fPlugin->sPluginInfoLoadMutex);

            const CarlaCachedPluginInfo* const info = carla_get_cached_plugin_info(fPluginType, index);
            DISTRHO_SAFE_ASSERT_RETURN(info != nullptr, true);

            if (! info->valid)
                break;
            if (info->cvIns != 0 || info->cvOuts != 0)
                break;

           #if ILDAEIL_STANDALONE
            if (info->midiIns != 0 && info->midiIns != 1)
                break;
            if (info->midiOuts != 0 && info->midiOuts != 1)
                break;
            if (info->audioIns > 2 || info->audioOuts > 2)
                break;
            if (fPluginType == PLUGIN_INTERNAL)
            {
                if (std::strcmp(info->label, "audiogain") == 0)
                    break;
                if (std::strcmp(info->label, "midichanfilter") == 0)
                    break;
                if (std::strcmp(info->label, "midichannelize") == 0)
                    break;
            }
           #elif DISTRHO_PLUGIN_IS_SYNTH
            if (info->midiIns != 1 && info->audioIns != 0)
                break;
            if ((info->hints & PLUGIN_IS_SYNTH) == 0x0 && info->audioIns != 0)
                break;
            if (info->audioOuts != 1 && info->audioOuts != 2)
                break;
           #elif DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
            if ((info->midiIns != 1 && info->audioIns != 0 && info->audioOuts != 0) || info->midiOuts != 1)
                break;
            if (info->audioIns != 0 || info->audioOuts != 0)
                break;
           #else
            if (info->audioIns != 1 && info->audioIns != 2)
                break;
            if (info->audioOuts != 1 && info->audioOuts != 2)
                break;
           #endif

            if (fPluginType == PLUGIN_INTERNAL)
            {
               #if !ILDAEIL_STANDALONE
                if (std::strcmp(info->label, "audiogain_s") == 0)
                    break;
               #endif
                if (std::strcmp(info->label, "lfo") == 0)
                    break;
                if (std::strcmp(info->label, "midi2cv") == 0)
                    break;
                if (std::strcmp(info->label, "midithrough") == 0)
                    break;
                if (std::strcmp(info->label, "3bandsplitter") == 0)
                    break;
            }

            const uint pindex = fPluginCount;
            fPlugins[pindex].name = strdup(info->name);
            fPlugins[pindex].label = strdup(info->label);
            ++fPluginCount;
        } while (false);

        // run again
        if (fRunnerData.pluginIndex != fRunnerData.pluginCount)
            return true;

        // stop here
        fPluginScanningFinished = true;
        return false;
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
        const double scaleFactor = getScaleFactor();
        const float padding = ImGui::GetStyle().WindowPadding.y * 2;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(getWidth(), kButtonHeight * scaleFactor + padding));

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
               #if !ILDAEIL_STANDALONE
                fNextSize = Size<uint>(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);
               #endif
            }

            ImGui::SameLine();

            if (ImGui::Button("Reset"))
                fIdleState = kIdleResetPlugin;

            if (fDrawingState == kDrawingPluginGenericUI)
            {
                if (fPluginHasCustomUI)
                {
                    ImGui::SameLine();

                    if (ImGui::Button("Show Custom GUI"))
                        fIdleState = kIdleShowCustomUI;
                }

                if (fPluginHasFileOpen)
                {
                    ImGui::SameLine();

                    if (ImGui::Button("Open File..."))
                        fIdleState = kIdleOpenFileUI;
                }

#ifdef WASM_TESTING
                ImGui::SameLine();
                ImGui::TextUnformatted("    Plugin to control:");
                for (uint i=1; i<10; ++i)
                {
                    char txt[8];
                    sprintf(txt, "%d", i);
                    ImGui::SameLine();
                    if (ImGui::Button(txt))
                    {
                        fPluginId = i;
                        fPluginGenericUI = nullptr;
                        fIdleState = kIdleHideEmbedAndShowGenericUI;
                    }
                }
#endif
            }

            if (fDrawingState == kDrawingPluginEmbedUI)
            {
                ImGui::SameLine();

                if (ImGui::Button("Show Generic GUI"))
                    fIdleState = kIdleHideEmbedAndShowGenericUI;
            }

           #if ILDAEIL_STANDALONE
            if (isUsingNativeAudio())
            {
                ImGui::SameLine();
                ImGui::Spacing();

                ImGui::SameLine();
                if (supportsAudioInput() && !isAudioInputEnabled() && ImGui::Button("Enable Input"))
                    requestAudioInput();

                ImGui::SameLine();
                if (supportsMIDI() && !isMIDIEnabled() && ImGui::Button("Enable MIDI"))
                    requestMIDI();
            }
           #endif
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
                    ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.printformat,
                                       ImGuiSliderFlags_NoInput | (param.log ? ImGuiSliderFlags_Logarithmic : 0x0));
                    ImGui::EndDisabled();
                    continue;
                }

                if (param.boolean)
                {
                    if (ImGui::Checkbox(param.name, &ui->parameters[i].bvalue))
                    {
                        if (ImGui::IsItemActivated())
                        {
                            carla_set_parameter_touch(handle, fPluginId, param.rindex, true);
                            // editParameter(0, true);
                        }

                        ui->values[i] = ui->parameters[i].bvalue ? ui->parameters[i].max : ui->parameters[i].min;
                        carla_set_parameter_value(handle, fPluginId, param.rindex, ui->values[i]);
                        // setParameterValue(0, ui->values[i]);
                    }
                }
                else
                {
                    const bool ret = param.log
                                   ? ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.printformat, ImGuiSliderFlags_Logarithmic)
                                   : ImGui::SliderFloat(param.name, &ui->values[i], param.min, param.max, param.printformat);
                    if (ret)
                    {
                        if (ImGui::IsItemActivated())
                        {
                            carla_set_parameter_touch(handle, fPluginId, param.rindex, true);
                            // editParameter(0, true);
                        }

                        carla_set_parameter_value(handle, fPluginId, param.rindex, ui->values[i]);
                        // setParameterValue(0, ui->values[i]);
                    }
                }

                if (ImGui::IsItemDeactivated())
                {
                    carla_set_parameter_touch(handle, fPluginId, param.rindex, false);
                    // editParameter(0, false);
                }
            }
        }

        ImGui::End();
    }

    void drawLoading()
    {
        setupMainWindowPos();

        constexpr const int plflags = ImGuiWindowFlags_NoSavedSettings
                                    | ImGuiWindowFlags_NoDecoration;

        if (ImGui::Begin("Plugin List", nullptr, plflags))
            ImGui::TextUnformatted("Loading...", nullptr);

        ImGui::End();
    }

    void drawPluginList()
    {
        static const char* pluginTypes[] = {
            getPluginTypeAsString(PLUGIN_INTERNAL),
            getPluginTypeAsString(PLUGIN_LV2),
            getPluginTypeAsString(PLUGIN_JSFX),
            "Load from file..."
        };

        setupMainWindowPos();

        constexpr const int plflags = ImGuiWindowFlags_NoSavedSettings
                                    | ImGuiWindowFlags_NoDecoration;

        if (ImGui::Begin("Plugin List", nullptr, plflags))
        {
            constexpr const int errflags = ImGuiWindowFlags_NoSavedSettings
                                         | ImGuiWindowFlags_NoResize
                                         | ImGuiWindowFlags_AlwaysAutoResize
                                         | ImGuiWindowFlags_NoCollapse
                                         | ImGuiWindowFlags_NoScrollbar
                                         | ImGuiWindowFlags_NoScrollWithMouse;

            if (ImGui::BeginPopupModal("Plugin Error", nullptr, errflags))
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
            case PLUGIN_JSFX:
                current = 2;
                break;
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
                case 2:
                    fNextPluginType = PLUGIN_JSFX;
                    break;
                case 3:
                    fNextPluginType = PLUGIN_TYPE_COUNT;
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
                    case PLUGIN_JSFX:
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
                        case PLUGIN_JSFX:
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

    void stateChanged(const char* /* const key */, const char*) override
    {
        /*
        if (std::strcmp(key, "project") == 0)
            hidePluginUI(fPlugin->fCarlaHostHandle);
        */
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

void ildaeilCloseUI(void* ui)
{
    DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr,);

    static_cast<IldaeilUI*>(ui)->closeUI();
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
