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

#pragma once

#include "CarlaNativePlugin.h"
#include "DistrhoPlugin.hpp"

// generates a warning if this is defined as anything else
#define CARLA_API

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

// static Mutex sPluginInfoLoadMutex;

// --------------------------------------------------------------------------------------------------------------------

class IldaeilBasePlugin : public Plugin
{
    // SharedResourcePointer<JuceInitializer> juceInitializer;

public:
    const NativePluginDescriptor* fCarlaPluginDescriptor;
    NativePluginHandle fCarlaPluginHandle;

    NativeHostDescriptor fCarlaHostDescriptor;
    CarlaHostHandle fCarlaHostHandle;

    void* fUI;

    IldaeilBasePlugin()
        : Plugin(0, 0, 1),
          fCarlaPluginDescriptor(nullptr),
          fCarlaPluginHandle(nullptr),
          fCarlaHostHandle(nullptr),
          fUI(nullptr)
    {
        memset(&fCarlaHostDescriptor, 0, sizeof(fCarlaHostDescriptor));
    }
};

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
