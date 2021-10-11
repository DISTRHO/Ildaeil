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

#include "DistrhoUI.hpp"
#include "ResizeHandle.hpp"

START_NAMESPACE_DISTRHO

// -----------------------------------------------------------------------------------------------------------

class IldaeilUI : public UI
{
    void* fContext;
    ResizeHandle fResizeHandle;

public:
    IldaeilUI()
        : UI(1280, 720),
          fContext(getPluginInstancePointer()),
          fResizeHandle(this)
    {
    }

    ~IldaeilUI() override
    {
    }

    void onImGuiDisplay() override
    {
        float width = getWidth();
        float height = getHeight();
        float margin = 20.0f;

        ImGui::SetNextWindowPos(ImVec2(margin, margin));
        ImGui::SetNextWindowSize(ImVec2(width - 2 * margin, height - 2 * margin));

        if (ImGui::Begin("Plugin List"))
        {
        }

        ImGui::End();
    }

    void uiIdle() override
    {
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
