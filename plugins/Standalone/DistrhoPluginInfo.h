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
#define DISTRHO_PLUGIN_NAME    "Ildaeil"
#define DISTRHO_PLUGIN_URI     "https://distrho.kx.studio/plugins/ildaeil"

#define DISTRHO_PLUGIN_HAS_UI             1
#define DISTRHO_PLUGIN_IS_SYNTH           0
#define DISTRHO_PLUGIN_NUM_INPUTS         2
#define DISTRHO_PLUGIN_NUM_OUTPUTS        2
#define DISTRHO_PLUGIN_WANT_LATENCY       1
#define DISTRHO_PLUGIN_WANT_STATE         1
#define DISTRHO_PLUGIN_WANT_FULL_STATE    1
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT    1
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT   1
#define DISTRHO_PLUGIN_WANT_TIMEPOS       1
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
#define DISTRHO_UI_USE_CUSTOM             1
#define DISTRHO_UI_CUSTOM_INCLUDE_PATH    "DearImGui.hpp"
#define DISTRHO_UI_CUSTOM_WIDGET_TYPE     DGL_NAMESPACE::ImGuiTopLevelWidget
#define DISTRHO_UI_DEFAULT_WIDTH          kInitialWidth
#define DISTRHO_UI_DEFAULT_HEIGHT         kInitialHeight
#define DISTRHO_UI_FILE_BROWSER           1
#define DISTRHO_UI_USER_RESIZABLE         1

#define ILDAEIL_STANDALONE 1

static constexpr const uint kInitialWidth  = 640;
static constexpr const uint kInitialHeight = 480;

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
