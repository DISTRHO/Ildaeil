/*
 * DISTRHO Ildaeil Plugin
 * Copyright (C) 2021 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
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

#define DISTRHO_PLUGIN_BRAND "DISTRHO"
#define DISTRHO_PLUGIN_NAME  "Ildaeil-FX"
#define DISTRHO_PLUGIN_URI   "https://distrho.kx.studio/plugins/ildaeil#fx"

#define DISTRHO_PLUGIN_HAS_UI             1
#define DISTRHO_PLUGIN_IS_SYNTH           0
#define DISTRHO_PLUGIN_NUM_INPUTS         2
#define DISTRHO_PLUGIN_NUM_OUTPUTS        2
#define DISTRHO_PLUGIN_WANT_LATENCY       1
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT    0
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT   0
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
#define DISTRHO_PLUGIN_WANT_TIMEPOS       1
#define DISTRHO_UI_USE_CUSTOM             1
#define DISTRHO_UI_USER_RESIZABLE         0
#define DISTRHO_UI_CUSTOM_INCLUDE_PATH    "DearImGui.hpp"
#define DISTRHO_UI_CUSTOM_WIDGET_TYPE     DGL_NAMESPACE::ImGuiTopLevelWidget

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
