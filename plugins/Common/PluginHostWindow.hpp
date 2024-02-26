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

#pragma once

#include "Window.hpp"

START_NAMESPACE_DGL

class PluginHostWindow
{
    struct PrivateData;
    PrivateData* const pData;

public:
    struct Callbacks {
        virtual ~Callbacks() {}
        virtual void pluginWindowResized(uint width, uint height) = 0;
    };

    explicit PluginHostWindow(Window& parentWindow, Callbacks* cbs);
    ~PluginHostWindow();

    void* attachAndGetWindowHandle();
    bool hide();
    void idle();
    void setOffset(uint x, uint y);
    void setSize(uint width, uint height);
};

END_NAMESPACE_DGL
