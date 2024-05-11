/*
 * DISTRHO Ildaeil Plugin
 * Copyright (C) 2021-2024 Filipe Coelho <falktx@falktx.com>
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

#include "water/files/File.h"
#include "water/files/FileInputStream.h"
#include "water/files/FileOutputStream.h"
#include "water/memory/MemoryBlock.h"

#include <string>
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
        BinaryType btype;
        uint64_t uniqueId;
        std::string filename;
        std::string name;
        std::string label;
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
                  readonly(false),
                  min(0.0f),
                  max(1.0f) {}
            ~Parameter()
            {
                std::free(name);
                std::free(printformat);
            }
        }* parameters;
        float* values;

        uint presetCount;
        struct Preset {
            uint32_t index;
            char* name;
            ~Preset()
            {
                std::free(name);
            }
        }* presets;
        int currentPreset;
        const char** presetStrings;

        PluginGenericUI()
            : title(nullptr),
              parameterCount(0),
              parameters(nullptr),
              values(nullptr),
              presetCount(0),
              presets(nullptr),
              currentPreset(-1),
              presetStrings(nullptr) {}

        ~PluginGenericUI()
        {
            std::free(title);
            delete[] parameters;
            delete[] values;
            delete[] presets;
            delete[] presetStrings;
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
    void* const fNativeWindowHandle;
    PluginHostWindow fPluginHostWindow;

    BinaryType fBinaryType;
    PluginType fPluginType;
    PluginType fNextPluginType;
    uint fPluginId;
    int fPluginSelected;
    bool fPluginHasCustomUI;
    bool fPluginHasEmbedUI;
    bool fPluginHasFileOpen;
    bool fPluginHasOutputParameters;
    bool fPluginIsBridge;
    bool fPluginIsIdling;
    bool fPluginRunning;
    bool fPluginWillRunInBridgeMode;
    Mutex fPluginsMutex;
    PluginInfoCache fCurrentPluginInfo;
    std::vector<PluginInfoCache> fPlugins;
    ScopedPointer<PluginGenericUI> fPluginGenericUI;

    bool fPluginSearchActive;
    bool fPluginSearchFirstShow;
    char fPluginSearchString[0xff];

    String fPopupError, fPluginFilename, fDiscoveryTool;
    Size<uint> fCurrentConstraintSize, fLastSize, fNextSize;
    bool fIgnoreNextHostWindowResize;
    bool fShowingHostWindow;
    bool fUpdateGeometryConstraints;

    struct RunnerData {
        bool needsReinit;
        CarlaPluginDiscoveryHandle handle;

        RunnerData()
          : needsReinit(true),
            handle(nullptr) {}
        
        void init()
        {
            needsReinit = true;

            if (handle != nullptr)
            {
                carla_plugin_discovery_stop(handle);
                handle = nullptr;
            }
        }
    } fRunnerData;

public:
    IldaeilUI()
        : UI(kInitialWidth, kInitialHeight),
          Runner("IldaeilScanner"),
          fDrawingState(kDrawingLoading),
          fIdleState(kIdleInit),
          fPlugin((IldaeilBasePlugin*)getPluginInstancePointer()),
          fNativeWindowHandle(reinterpret_cast<void*>(getWindow().getNativeWindowHandle())),
          fPluginHostWindow(fNativeWindowHandle, this),
          fBinaryType(BINARY_NATIVE),
          fPluginType(PLUGIN_LV2),
          fNextPluginType(fPluginType),
          fPluginId(0),
          fPluginSelected(-1),
          fPluginHasCustomUI(false),
          fPluginHasEmbedUI(false),
          fPluginHasFileOpen(false),
          fPluginHasOutputParameters(false),
          fPluginIsBridge(false),
          fPluginIsIdling(false),
          fPluginRunning(false),
          fPluginWillRunInBridgeMode(false),
          fCurrentPluginInfo(),
          fPluginSearchActive(false),
          fPluginSearchFirstShow(false),
          fIgnoreNextHostWindowResize(false),
          fShowingHostWindow(false),
          fUpdateGeometryConstraints(false),
          fRunnerData()
    {
        const double scaleFactor = getScaleFactor();

        if (fPlugin == nullptr || fPlugin->fCarlaHostHandle == nullptr)
        {
            fDrawingState = kDrawingErrorInit;
            fIdleState = kIdleNothing;
            fPopupError = "Ildaeil backend failed to init properly, cannot continue.";
            setGeometryConstraints(kInitialWidth * scaleFactor * 0.5, kInitialHeight * scaleFactor * 0.5);
            setSize(kInitialWidth * scaleFactor * 0.5, kInitialHeight * scaleFactor * 0.5);
            return;
        }

        std::strcpy(fPluginSearchString, "Search...");

        ImGuiStyle& style(ImGui::GetStyle());
        style.FrameRounding = 4 * scaleFactor;

        const double paddingY = style.WindowPadding.y * 2;

        if (d_isNotEqual(scaleFactor, 1.0))
        {
            fPluginHostWindow.setOffset(0, kButtonHeight * scaleFactor + paddingY);
            setGeometryConstraints(kInitialWidth * scaleFactor, kInitialWidth * scaleFactor);
            setSize(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);
        }
        else
        {
            fPluginHostWindow.setOffset(0, kButtonHeight + paddingY);
            setGeometryConstraints(kInitialWidth, kInitialWidth);
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

        fPluginIsBridge = hints & PLUGIN_IS_BRIDGE;
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

    void resizeUI(const uint32_t width, const uint32_t height)
    {
        if (fDrawingState != kDrawingPluginEmbedUI)
            return;

        const uint extraHeight = kButtonHeight * getScaleFactor() + ImGui::GetStyle().WindowPadding.y * 2;

        fShowingHostWindow = true;
        fNextSize = Size<uint>(width, height + extraHeight);
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
       #ifndef DISTRHO_OS_WASM
        const uint hints = carla_get_plugin_info(handle, fPluginId)->hints;

        if (hints & PLUGIN_HAS_CUSTOM_EMBED_UI)
        {
            fDrawingState = kDrawingPluginEmbedUI;
            fIdleState = kIdleGiveIdleToUI;
            fPluginHasCustomUI = true;
            fPluginHasEmbedUI = true;
            fPluginHasFileOpen = false;

            fIgnoreNextHostWindowResize = false;
            fShowingHostWindow = true;

            fPluginHostWindow.restart();
            carla_embed_custom_ui(handle, fPluginId, fNativeWindowHandle);
            fPluginHostWindow.idle();
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

       #ifndef DISTRHO_OS_WASM
        const double scaleFactor = getScaleFactor();
        fNextSize = Size<uint>(kGenericWidth * scaleFactor,
                               (kGenericHeight + ImGui::GetStyle().WindowPadding.y) * scaleFactor);
        fLastSize = Size<uint>();
        fUpdateGeometryConstraints = true;
       #endif
    }

    void createPluginGenericUI(const CarlaHostHandle handle, const CarlaPluginInfo* const info)
    {
        PluginGenericUI* const ui = new PluginGenericUI;

        String title(info->name);
        title += " by ";
        title += info->maker;
        ui->title = title.getAndReleaseBuffer();

        fPluginHasOutputParameters = false;

        const uint32_t parameterCount = ui->parameterCount = carla_get_parameter_count(handle, fPluginId);

        // make count of valid parameters
        for (uint32_t i=0; i < parameterCount; ++i)
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
        for (uint32_t i=0, j=0; i < parameterCount; ++i)
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

        // handle presets too
        const uint32_t presetCount = ui->presetCount = carla_get_program_count(handle, fPluginId);

        for (uint32_t i=0; i < presetCount; ++i)
        {
            const char* const pname = carla_get_program_name(handle, fPluginId, i);

            if (pname[0] == '\0')
            {
                --ui->presetCount;
                continue;
            }
        }

        ui->presets = new PluginGenericUI::Preset[ui->presetCount];
        ui->presetStrings = new const char*[ui->presetCount];

        for (uint32_t i=0, j=0; i < presetCount; ++i)
        {
            const char* const pname = carla_get_program_name(handle, fPluginId, i);

            if (pname[0] == '\0')
                continue;

            PluginGenericUI::Preset& preset(ui->presets[j]);
            preset.index = i;
            preset.name = strdup(pname);

            ui->presetStrings[j] = preset.name;

            ++j;
        }

        ui->currentPreset = -1;

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

    bool loadPlugin(const CarlaHostHandle handle, const PluginInfoCache& info)
    {
        if (fPluginRunning || fPluginId != 0)
        {
            hidePluginUI(handle);
            carla_replace_plugin(handle, fPluginId);
        }

        carla_set_engine_option(handle, ENGINE_OPTION_PREFER_PLUGIN_BRIDGES, fPluginWillRunInBridgeMode, nullptr);

        const MutexLocker cml(fPlugin->sPluginInfoLoadMutex);

        const bool ok = carla_add_plugin(handle,
                                         info.btype,
                                         fPluginType,
                                         info.filename.c_str(),
                                         info.name.c_str(),
                                         info.label.c_str(),
                                         info.uniqueId,
                                         nullptr,
                                         PLUGIN_OPTIONS_NULL);

        if (ok)
        {
            d_debug("loadeded a plugin with label '%s' and name '%s' %lu",
                    info.name.c_str(), info.label.c_str(), info.uniqueId);

            fPluginRunning = true;
            fPluginGenericUI = nullptr;
            fPluginFilename.clear();

           #ifdef DISTRHO_OS_MAC
            const bool brokenOffset = fPluginType == PLUGIN_VST2
                && info.name == "Renoise Redux"
                && info.uniqueId == d_cconst('R', 'R', 'D', 'X');
            fPluginHostWindow.setOffsetBroken(brokenOffset);
           #endif

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
        return ok;
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
        if (fIgnoreNextHostWindowResize)
        {
            fIgnoreNextHostWindowResize = false;
            return;
        }

        if (fShowingHostWindow)
        {
            fShowingHostWindow = false;
            fIgnoreNextHostWindowResize = true;
            fUpdateGeometryConstraints = true;
        }

        const uint extraHeight = kButtonHeight * getScaleFactor() + ImGui::GetStyle().WindowPadding.y * 2;

        fNextSize = Size<uint>(width, height + extraHeight);

        // reduce geometry constraint if needed
        if (fIgnoreNextHostWindowResize)
            return;
        if (width < fCurrentConstraintSize.getWidth() || height + extraHeight < fCurrentConstraintSize.getHeight())
            fUpdateGeometryConstraints = true;

        if (fPluginIsIdling && fLastSize != fNextSize)
        {
            fLastSize = fNextSize;

            if (fUpdateGeometryConstraints)
            {
                fUpdateGeometryConstraints = false;
                fCurrentConstraintSize = fNextSize;
                setGeometryConstraints(fNextSize.getWidth(), fNextSize.getHeight());
            }

            setSize(fNextSize);
        }
    }

   #if DISTRHO_UI_USER_RESIZABLE
    void onResize(const ResizeEvent& ev) override
    {
        UI::onResize(ev);

        if (fIgnoreNextHostWindowResize)
            return;
        if (fShowingHostWindow)
            return;
        if (fDrawingState != kDrawingPluginEmbedUI)
            return;

        const double scaleFactor = getScaleFactor();
        const uint extraHeight = kButtonHeight * scaleFactor + ImGui::GetStyle().WindowPadding.y * 2;
        uint width = ev.size.getWidth();
        uint height = ev.size.getHeight() - extraHeight;
       #ifdef DISTRHO_OS_MAC
        width /= scaleFactor;
        height /= scaleFactor;
       #endif
        fPluginHostWindow.setSize(width, height);
    }
   #endif

    void uiIdle() override
    {
        const CarlaHostHandle handle = fPlugin->fCarlaHostHandle;
        DISTRHO_SAFE_ASSERT_RETURN(handle != nullptr,);

        if (fDrawingState == kDrawingPluginGenericUI && fPluginGenericUI != nullptr && fPluginHasOutputParameters)
        {
            updatePluginGenericUI(handle);
            repaint();
        }

        if (fNextSize.isValid() && fLastSize != fNextSize)
        {
            fLastSize = fNextSize;

            if (fUpdateGeometryConstraints)
            {
                fUpdateGeometryConstraints = false;
                fCurrentConstraintSize = fNextSize;
                setGeometryConstraints(fNextSize.getWidth(), fNextSize.getHeight());
            }

            setSize(fNextSize);
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
                loadPlugin(handle, fCurrentPluginInfo);
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

            fPluginIsIdling = true;
            fPluginHostWindow.idle();
            fPluginIsIdling = false;
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

        PluginInfoCache info;
        {
            const MutexLocker cml(fPluginsMutex);
            info = fPlugins[fPluginSelected];
        }

        d_stdout("Loading %s...", info.name.c_str());

        if (loadPlugin(handle, info))
            fCurrentPluginInfo = info;
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

            {
                const MutexLocker cml(fPluginsMutex);
                fPlugins.clear();
            }

            d_stdout("Will scan plugins now...");

            const String& binaryPath(fPlugin->fBinaryPath);

            if (binaryPath.isNotEmpty())
            {
                fBinaryType = BINARY_NATIVE;

                fDiscoveryTool  = binaryPath;
                fDiscoveryTool += DISTRHO_OS_SEP_STR "carla-discovery-native";
               #ifdef CARLA_OS_WIN
                fDiscoveryTool += ".exe";
               #endif

                fRunnerData.handle = carla_plugin_discovery_start(fDiscoveryTool,
                                                                  fBinaryType,
                                                                  fPluginType,
                                                                  IldaeilBasePlugin::getPluginPath(fPluginType),
                                                                  _binaryPluginSearchCallback,
                                                                  _binaryPluginCheckCacheCallback,
                                                                  this);

            }

            if (fDrawingState == kDrawingLoading)
            {
                fDrawingState = kDrawingPluginList;
                fPluginSearchFirstShow = true;
            }

            if (binaryPath.isEmpty() || (fRunnerData.handle == nullptr && !startNextDiscovery()))
            {
                d_stdout("Nothing found!");
                return false;
            }
        }

        DISTRHO_SAFE_ASSERT_RETURN(fRunnerData.handle != nullptr, false);

        if (carla_plugin_discovery_idle(fRunnerData.handle))
            return true;

        // stop here
        carla_plugin_discovery_stop(fRunnerData.handle);
        fRunnerData.handle = nullptr;

        if (startNextDiscovery())
            return true;

        d_stdout("Found %lu plugins!", (ulong)fPlugins.size());
        return false;
    }

    bool startNextDiscovery()
    {
        if (! setNextDiscoveryTool())
            return false;

        fRunnerData.handle = carla_plugin_discovery_start(fDiscoveryTool,
                                                          fBinaryType,
                                                          fPluginType,
                                                          IldaeilBasePlugin::getPluginPath(fPluginType),
                                                          _binaryPluginSearchCallback,
                                                          _binaryPluginCheckCacheCallback,
                                                          this);

        if (fRunnerData.handle == nullptr)
            return startNextDiscovery();

        return true;
    }

    bool setNextDiscoveryTool()
    {
        switch (fPluginType)
        {
        case PLUGIN_VST2:
        case PLUGIN_VST3:
        case PLUGIN_CLAP:
            break;
        default:
            return false;
        }

      #ifdef CARLA_OS_WIN
        #ifdef CARLA_OS_WIN64
        // look for win32 plugins on win64
        if (fBinaryType == BINARY_NATIVE)
        {
            fBinaryType = BINARY_WIN32;
            fDiscoveryTool = fPlugin->fBinaryPath;
            fDiscoveryTool += CARLA_OS_SEP_STR "carla-discovery-win32.exe";

            if (water::File(fDiscoveryTool.buffer()).existsAsFile())
                return true;
        }
       #endif

        // no other types to try
        return false;
      #else // CARLA_OS_WIN

       #ifndef CARLA_OS_MAC
        // try 32bit plugins on 64bit systems, skipping macOS where 32bit is no longer supported
        if (fBinaryType == BINARY_NATIVE)
        {
            fBinaryType = BINARY_POSIX32;
            fDiscoveryTool = fPlugin->fBinaryPath;
            fDiscoveryTool += CARLA_OS_SEP_STR "carla-discovery-posix32";

            if (water::File(fDiscoveryTool.buffer()).existsAsFile())
                return true;
        }
       #endif

        // try wine bridges
       #ifdef CARLA_OS_64BIT
        if (fBinaryType == BINARY_NATIVE || fBinaryType == BINARY_POSIX32)
        {
            fBinaryType = BINARY_WIN64;
            fDiscoveryTool = fPlugin->fBinaryPath;
            fDiscoveryTool += CARLA_OS_SEP_STR "carla-discovery-win64.exe";

            if (water::File(fDiscoveryTool.buffer()).existsAsFile())
                return true;
        }
       #endif

        if (fBinaryType != BINARY_WIN32)
        {
            fBinaryType = BINARY_WIN32;
            fDiscoveryTool = fPlugin->fBinaryPath;
            fDiscoveryTool += CARLA_OS_SEP_STR "carla-discovery-win32.exe";

            if (water::File(fDiscoveryTool.buffer()).existsAsFile())
                return true;
        }

        return false;
      #endif // CARLA_OS_WIN
    }

    void binaryPluginSearchCallback(const CarlaPluginDiscoveryInfo* const info, const char* const sha1sum)
    {
        // save plugin info into cache
        if (sha1sum != nullptr)
        {
            const water::String configDir(ildaeilConfigDir());
            const water::File cacheFile((configDir + CARLA_OS_SEP_STR "cache" CARLA_OS_SEP_STR + sha1sum).toRawUTF8());

            if (cacheFile.create().ok())
            {
                water::FileOutputStream stream(cacheFile);

                if (stream.openedOk())
                {
                    if (info != nullptr)
                    {
                        stream.writeString(getBinaryTypeAsString(info->btype));
                        stream.writeString(getPluginTypeAsString(info->ptype));
                        stream.writeString(info->filename);
                        stream.writeString(info->label);
                        stream.writeInt64(info->uniqueId);
                        stream.writeString(info->metadata.name);
                        stream.writeString(info->metadata.maker);
                        stream.writeString(getPluginCategoryAsString(info->metadata.category));
                        stream.writeInt(info->metadata.hints);
                        stream.writeCompressedInt(info->io.audioIns);
                        stream.writeCompressedInt(info->io.audioOuts);
                        stream.writeCompressedInt(info->io.cvIns);
                        stream.writeCompressedInt(info->io.cvOuts);
                        stream.writeCompressedInt(info->io.midiIns);
                        stream.writeCompressedInt(info->io.midiOuts);
                        stream.writeCompressedInt(info->io.parameterIns);
                        stream.writeCompressedInt(info->io.parameterOuts);
                    }
                }
                else
                {
                    d_stderr("Failed to write cache file for %s%s%s",
                            ildaeilConfigDir(), CARLA_OS_SEP_STR "cache" CARLA_OS_SEP_STR, sha1sum);
                }
            }
            else
            {
                d_stderr("Failed to write cache file directories for %s%s%s",
                         ildaeilConfigDir(), CARLA_OS_SEP_STR "cache" CARLA_OS_SEP_STR, sha1sum);
            }
        }

        if (info == nullptr)
            return;

        if (info->io.cvIns != 0 || info->io.cvOuts != 0)
            return;
        if (info->io.midiIns != 0 && info->io.midiIns != 1)
            return;
        if (info->io.midiOuts != 0 && info->io.midiOuts != 1)
            return;

       #if ILDAEIL_STANDALONE
        if (fPluginType == PLUGIN_INTERNAL)
        {
            if (std::strcmp(info->label, "audiogain") == 0)
                return;
            if (std::strcmp(info->label, "midichanfilter") == 0)
                return;
            if (std::strcmp(info->label, "midichannelize") == 0)
                return;
        }
       #elif DISTRHO_PLUGIN_IS_SYNTH
        if (info->io.midiIns != 1)
            return;
        if (info->io.audioOuts == 0)
            return;
       #elif DISTRHO_PLUGIN_WANT_MIDI_OUTPUT
        if ((info->io.midiIns != 1 && info->io.audioIns != 0 && info->io.audioOuts != 0) || info->io.midiOuts != 1)
            return;
        if (info->io.audioIns != 0 || info->io.audioOuts != 0)
            return;
       #else
        if (info->io.audioIns != 1 && info->io.audioIns != 2)
            return;
        if (info->io.audioOuts != 1 && info->io.audioOuts != 2)
            return;
       #endif

        if (fPluginType == PLUGIN_INTERNAL)
        {
           #if !ILDAEIL_STANDALONE
            if (std::strcmp(info->label, "audiogain_s") == 0)
                return;
           #endif
            if (std::strcmp(info->label, "lfo") == 0)
                return;
            if (std::strcmp(info->label, "midi2cv") == 0)
                return;
            if (std::strcmp(info->label, "midithrough") == 0)
                return;
            if (std::strcmp(info->label, "3bandsplitter") == 0)
                return;
        }

        const PluginInfoCache pinfo = {
            info->btype,
            info->uniqueId,
            info->filename,
            info->metadata.name,
            info->label,
        };

        const MutexLocker cml(fPluginsMutex);
        fPlugins.push_back(pinfo);
    }

    static void _binaryPluginSearchCallback(void* const ptr,
                                            const CarlaPluginDiscoveryInfo* const info,
                                            const char* const sha1sum)
    {
        static_cast<IldaeilUI*>(ptr)->binaryPluginSearchCallback(info, sha1sum);
    }

    bool binaryPluginCheckCacheCallback(const char* const filename, const char* const sha1sum)
    {
        if (sha1sum == nullptr)
            return false;

        const water::String configDir(ildaeilConfigDir());
        const water::File cacheFile((configDir + CARLA_OS_SEP_STR "cache" CARLA_OS_SEP_STR + sha1sum).toRawUTF8());

        if (cacheFile.existsAsFile())
        {
            water::FileInputStream stream(cacheFile);

            if (stream.openedOk())
            {
                while (! stream.isExhausted())
                {
                    CarlaPluginDiscoveryInfo info = {};

                    // read back everything the same way and order as we wrote it
                    info.btype = getBinaryTypeFromString(stream.readString().toRawUTF8());
                    info.ptype = getPluginTypeFromString(stream.readString().toRawUTF8());
                    const water::String pfilename(stream.readString());
                    const water::String label(stream.readString());
                    info.uniqueId = stream.readInt64();
                    const water::String name(stream.readString());
                    const water::String maker(stream.readString());
                    info.metadata.category = getPluginCategoryFromString(stream.readString().toRawUTF8());
                    info.metadata.hints = stream.readInt();
                    info.io.audioIns = stream.readCompressedInt();
                    info.io.audioOuts = stream.readCompressedInt();
                    info.io.cvIns = stream.readCompressedInt();
                    info.io.cvOuts = stream.readCompressedInt();
                    info.io.midiIns = stream.readCompressedInt();
                    info.io.midiOuts = stream.readCompressedInt();
                    info.io.parameterIns = stream.readCompressedInt();
                    info.io.parameterOuts = stream.readCompressedInt();

                    // string stuff
                    info.filename = pfilename.toRawUTF8();
                    info.label = label.toRawUTF8();
                    info.metadata.name = name.toRawUTF8();
                    info.metadata.maker = maker.toRawUTF8();

                    // check sha1 collisions
                    if (pfilename != filename)
                    {
                        d_stderr("Cache hash collision for %s: \"%s\" vs \"%s\"",
                                sha1sum, pfilename.toRawUTF8(), filename);
                        return false;
                    }

                    // purposefully not passing sha1sum, to not override cache file
                    binaryPluginSearchCallback(&info, nullptr);
                }

                return true;
            }
            else
            {
                d_stderr("Failed to read cache file for %s%s%s",
                         ildaeilConfigDir(), CARLA_OS_SEP_STR "cache" CARLA_OS_SEP_STR, sha1sum);
            }
        }

        return false;
    }

    static bool _binaryPluginCheckCacheCallback(void* const ptr, const char* const filename, const char* const sha1)
    {
        return static_cast<IldaeilUI*>(ptr)->binaryPluginCheckCacheCallback(filename, sha1);
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
               #ifndef DISTRHO_OS_WASM
                fNextSize = Size<uint>(kInitialWidth * scaleFactor, kInitialHeight * scaleFactor);
                fLastSize = Size<uint>();
                fUpdateGeometryConstraints = true;
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

                if (fDrawingState != kDrawingPluginEmbedUI && supportsBufferSizeChanges())
                {
                    ImGui::SameLine();
                    ImGui::Spacing();

                    ImGui::SameLine();
                    ImGui::Text("Buffer Size:");

                    static constexpr uint bufferSizes_i[] = {
                       #ifndef DISTRHO_OS_WASM
                        128,
                       #endif
                        256, 512, 1024, 2048, 4096, 8192,
                       #ifdef DISTRHO_OS_WASM
                        16384,
                       #endif
                    };
                    static constexpr const char* bufferSizes_s[] = {
                       #ifndef DISTRHO_OS_WASM
                        "128",
                       #endif
                        "256", "512", "1024", "2048", "4096", "8192",
                       #ifdef DISTRHO_OS_WASM
                        "16384",
                       #endif
                    };
                    uint buffersize = getBufferSize();
                    int current = -1;
                    for (uint i=0; i<ARRAY_SIZE(bufferSizes_i); ++i)
                    {
                        if (bufferSizes_i[i] == buffersize)
                        {
                            current = i;
                            break;
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::Combo("##buffersize", &current, bufferSizes_s, ARRAY_SIZE(bufferSizes_s)))
                    {
                        const uint next = bufferSizes_i[current];
                        d_stdout("requesting new buffer size: %u -> %u", buffersize, next);
                        requestBufferSizeChange(next);
                    }
                }
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

            if (fPluginIsBridge)
            {
                const bool active = carla_get_internal_parameter_value(handle, 0, PARAMETER_ACTIVE) > 0.5f;

                if (active)
                {
                    ImGui::BeginDisabled();
                    ImGui::Button("Reload bridge");
                    ImGui::EndDisabled();
                }
                else
                {
                    if (ImGui::Button("Reload bridge"))
                        carla_set_active(handle, 0, true);
                }
            }

            if (ui->presetCount != 0)
            {
                ImGui::Text("Preset:");
                ImGui::SameLine();

                if (ImGui::Combo("##presets", &ui->currentPreset, ui->presetStrings, ui->presetCount))
                {
                    PluginGenericUI::Preset& preset(ui->presets[ui->currentPreset]);

                    carla_set_program(handle, fPluginId, preset.index);
                }
            }

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
            getPluginTypeAsString(PLUGIN_LADSPA),
            getPluginTypeAsString(PLUGIN_DSSI),
            getPluginTypeAsString(PLUGIN_LV2),
            getPluginTypeAsString(PLUGIN_VST2),
            getPluginTypeAsString(PLUGIN_VST3),
            getPluginTypeAsString(PLUGIN_CLAP),
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
            case PLUGIN_JSFX: current = 7; break;
            case PLUGIN_CLAP: current = 6; break;
            case PLUGIN_VST3: current = 5; break;
            case PLUGIN_VST2: current = 4; break;
            case PLUGIN_LV2: current = 3; break;
            case PLUGIN_DSSI: current = 2; break;
            case PLUGIN_LADSPA: current = 1; break;
            default: current = 0; break;
            }

            if (ImGui::Combo("##plugintypes", &current, pluginTypes, ARRAY_SIZE(pluginTypes)))
            {
                fIdleState = kIdleChangePluginType;
                switch (current)
                {
                case 0: fNextPluginType = PLUGIN_INTERNAL; break;
                case 1: fNextPluginType = PLUGIN_LADSPA; break;
                case 2: fNextPluginType = PLUGIN_DSSI; break;
                case 3: fNextPluginType = PLUGIN_LV2; break;
                case 4: fNextPluginType = PLUGIN_VST2; break;
                case 5: fNextPluginType = PLUGIN_VST3; break;
                case 6: fNextPluginType = PLUGIN_CLAP; break;
                case 7: fNextPluginType = PLUGIN_JSFX; break;
                case 8: fNextPluginType = PLUGIN_TYPE_COUNT; break;
                }
            }

            ImGui::BeginDisabled(fPluginSelected < 0);

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
                        ImGui::TableSetupColumn("Name");
                        ImGui::TableSetupColumn("Filename");
                        ImGui::TableHeadersRow();
                        break;
                    }

                    const MutexLocker cml(fPluginsMutex);

                    for (uint i=0; i<fPlugins.size(); ++i)
                    {
                        const PluginInfoCache& info(fPlugins[i]);

                        if (search != nullptr && ildaeil::strcasestr(info.name.c_str(), search) == nullptr)
                            continue;

                        bool selected = fPluginSelected >= 0 && static_cast<uint>(fPluginSelected) == i;

                        switch (fPluginType)
                        {
                        case PLUGIN_INTERNAL:
                        case PLUGIN_AU:
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Selectable(info.name.c_str(), &selected);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Selectable(info.label.c_str(), &selected);
                            break;
                        case PLUGIN_LV2:
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Selectable(info.name.c_str(), &selected);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Selectable(info.label.c_str(), &selected);
                            break;
                        default:
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Selectable(info.name.c_str(), &selected);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Selectable(info.filename.c_str(), &selected);
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

void ildaeilResizeUI(void* const ui, const uint32_t width, const uint32_t height)
{
    DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr,);

    static_cast<IldaeilUI*>(ui)->resizeUI(width, height);
}

void ildaeilCloseUI(void* const ui)
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
