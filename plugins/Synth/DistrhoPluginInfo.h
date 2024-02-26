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

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "DISTRHO"
#define DISTRHO_PLUGIN_NAME    "Ildaeil-Synth"
#define DISTRHO_PLUGIN_URI     "https://distrho.kx.studio/plugins/ildaeil#synth"
#define DISTRHO_PLUGIN_CLAP_ID "studio.kx.distrho.ildaeil#synth"

#define DISTRHO_PLUGIN_BRAND_ID       Dstr
#define DISTRHO_PLUGIN_UNIQUE_ID      ilda
#define DISTRHO_PLUGIN_CLAP_FEATURES   "instrument", "stereo"
#define DISTRHO_PLUGIN_LV2_CATEGORY    "lv2:InstrumentPlugin"
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Instrument|Stereo"

#define DISTRHO_PLUGIN_HAS_UI             1
#define DISTRHO_PLUGIN_IS_SYNTH           1
#define DISTRHO_PLUGIN_NUM_INPUTS         0
#define DISTRHO_PLUGIN_NUM_OUTPUTS        2
#define DISTRHO_PLUGIN_WANT_LATENCY       1
#define DISTRHO_PLUGIN_WANT_STATE         1
#define DISTRHO_PLUGIN_WANT_FULL_STATE    1
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT    1
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT   0
#define DISTRHO_PLUGIN_WANT_TIMEPOS       1
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
#define DISTRHO_UI_USE_CUSTOM             1
#define DISTRHO_UI_USER_RESIZABLE         0
#define DISTRHO_UI_CUSTOM_INCLUDE_PATH    "DearImGui.hpp"
#define DISTRHO_UI_CUSTOM_WIDGET_TYPE     DGL_NAMESPACE::ImGuiTopLevelWidget
#define DISTRHO_UI_DEFAULT_WIDTH          kInitialWidth
#define DISTRHO_UI_DEFAULT_HEIGHT         kInitialHeight

#define DPF_VST3_DONT_USE_BRAND_ID

#define ILDAEIL_STANDALONE 0

static constexpr const uint kInitialWidth  = 520;
static constexpr const uint kInitialHeight = 520;

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
