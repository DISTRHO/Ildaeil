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

#include "src/DistrhoDefines.h"

#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
# import <Cocoa/Cocoa.h>
#elif defined(DISTRHO_OS_WASM)
#elif defined(DISTRHO_OS_WINDOWS)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#else
# define ILDAEIL_X11
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <pthread.h>
#endif

#include "PluginHostWindow.hpp"

START_NAMESPACE_DGL

#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
#elif defined(DISTRHO_OS_WASM)
#elif defined(DISTRHO_OS_WINDOWS)
#else
static pthread_mutex_t gErrorMutex = PTHREAD_MUTEX_INITIALIZER;
static bool gErrorTriggered = false;
static int ildaeilErrorHandler(Display*, XErrorEvent*)
{
    gErrorTriggered = true;
    return 0;
}
#endif

struct PluginHostWindow::PrivateData
{
    void* const windowHandle;
    Callbacks* const pluginWindowCallbacks;

#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
    NSView* pluginView;
#elif defined(DISTRHO_OS_WASM)
#elif defined(DISTRHO_OS_WINDOWS)
    ::HWND pluginWindow;
#else
    ::Display* display;
    ::Window pluginWindow;
#endif
    uint xOffset, yOffset;

    bool brokenOffsetFactor;
    bool lookingForChildren;

    PrivateData(void* const wh, Callbacks* const cbs)
        : windowHandle(wh),
          pluginWindowCallbacks(cbs),
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
          pluginView(nullptr),
#elif defined(DISTRHO_OS_WASM)
#elif defined(DISTRHO_OS_WINDOWS)
          pluginWindow(nullptr),
#else
          display(nullptr),
          pluginWindow(0),
#endif
          xOffset(0),
          yOffset(0),
          brokenOffsetFactor(false),
          lookingForChildren(false)
    {
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
#elif defined(DISTRHO_OS_WASM)
#elif defined(DISTRHO_OS_WINDOWS)
#else
        display = XOpenDisplay(nullptr);
        DISTRHO_SAFE_ASSERT_RETURN(display != nullptr,)
#endif
    }

    ~PrivateData()
    {
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
#elif defined(DISTRHO_OS_WASM)
#elif defined(DISTRHO_OS_WINDOWS)
#else
        if (display != nullptr)
            XCloseDisplay(display);
#endif
    }

    void restart()
    {
        lookingForChildren = true;
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
        pluginView = nullptr;
#elif defined(DISTRHO_OS_WASM)
#elif defined(DISTRHO_OS_WINDOWS)
        pluginWindow = nullptr;
#else
        pluginWindow = 0;

        for (XEvent event; XPending(display) > 0;)
            XNextEvent(display, &event);
#endif
    }

    bool hide()
    {
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
        if (pluginView != nullptr)
        {
            [pluginView setHidden:YES];
            pluginView = nullptr;
            [NSOpenGLContext clearCurrentContext];
            return true;
        }
#elif defined(DISTRHO_OS_WASM)
#elif defined(DISTRHO_OS_WINDOWS)
        if (pluginWindow != nullptr)
        {
            ShowWindow(pluginWindow, SW_HIDE);
            pluginWindow = nullptr;
            return true;
        }
#else
        if (pluginWindow != 0)
        {
            XUnmapWindow(display, pluginWindow);
            XSync(display, True);
            pluginWindow = 0;
            return true;
        }
#endif
        return false;
    }

    void idle()
    {
        if (lookingForChildren)
        {
           #if defined(DISTRHO_OS_HAIKU)
           #elif defined(DISTRHO_OS_MAC)
            if (pluginView == nullptr)
            {
                bool first = true;
                for (NSView* view in [(NSView*)windowHandle subviews])
                {
                    if (first)
                    {
                        first = false;
                        continue;
                    }
                    pluginView = view;
                    break;
                }
            }
           #elif defined(DISTRHO_OS_WASM)
           #elif defined(DISTRHO_OS_WINDOWS)
            if (pluginWindow == nullptr)
                pluginWindow = FindWindowExA((::HWND)windowHandle, nullptr, nullptr, nullptr);
           #else
            if (display != nullptr && pluginWindow == 0)
            {
                ::Window rootWindow, parentWindow;
                ::Window* childWindows = nullptr;
                uint numChildren = 0;

                XQueryTree(display, (::Window)windowHandle, &rootWindow, &parentWindow, &childWindows, &numChildren);

                if (numChildren > 0 && childWindows != nullptr)
                {
                    // pick last child, needed for NTK based UIs which do not delete/remove previous windows.
                    //sadly this breaks ildaeil-within-ildaeil recursion.. :(
                    pluginWindow = childWindows[numChildren - 1];
                    XFree(childWindows);
                }
            }
           #endif
        }

       #if defined(DISTRHO_OS_HAIKU)
       #elif defined(DISTRHO_OS_MAC)
        if (pluginView != nullptr)
        {
            const double scaleFactor = [[[pluginView window] screen] backingScaleFactor];
            const NSSize size = [pluginView frame].size;
            const double width = size.width;
            const double height = size.height;

            if (lookingForChildren)
                d_stdout("child window bounds %f %f | offset %u %u", width, height, xOffset, yOffset);

            if (width > 1.0 && height > 1.0)
            {
                NSPoint origin = brokenOffsetFactor ? NSMakePoint(xOffset, yOffset)
                                                    : NSMakePoint(xOffset / scaleFactor, yOffset / scaleFactor);

                lookingForChildren = false;
                [pluginView setFrameOrigin:origin];
                pluginWindowCallbacks->pluginWindowResized(width * scaleFactor, height * scaleFactor);
            }
        }
       #elif defined(DISTRHO_OS_WASM)
       #elif defined(DISTRHO_OS_WINDOWS)
        if (pluginWindow != nullptr)
        {
            int width = 0;
            int height = 0;

            RECT rect;
            if (GetWindowRect(pluginWindow, &rect))
            {
                width = rect.right - rect.left;
                height = rect.bottom - rect.top;
            }

            if (lookingForChildren)
                d_stdout("child window bounds %i %i | offset %u %u", width, height, xOffset, yOffset);

            if (width > 1 && height > 1)
            {
                lookingForChildren = false;
                SetWindowPos(pluginWindow, 0, xOffset, yOffset, 0, 0,
                             SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER);
                pluginWindowCallbacks->pluginWindowResized(width, height);
            }
        }
       #else
        for (XEvent event; XPending(display) > 0;)
            XNextEvent(display, &event);

        if (pluginWindow != 0)
        {
            int width = 0;
            int height = 0;

            XWindowAttributes attrs;
            memset(&attrs, 0, sizeof(attrs));

            pthread_mutex_lock(&gErrorMutex);
            const XErrorHandler oldErrorHandler = XSetErrorHandler(ildaeilErrorHandler);
            gErrorTriggered = false;

            if (XGetWindowAttributes(display, pluginWindow, &attrs) && ! gErrorTriggered)
            {
                width = attrs.width;
                height = attrs.height;
            }

            XSetErrorHandler(oldErrorHandler);
            pthread_mutex_unlock(&gErrorMutex);

            if (width == 0 && height == 0)
            {
                XSizeHints sizeHints;
                memset(&sizeHints, 0, sizeof(sizeHints));

                if (XGetNormalHints(display, pluginWindow, &sizeHints))
                {
                    if (sizeHints.flags & PSize)
                    {
                        width = sizeHints.width;
                        height = sizeHints.height;
                    }
                    else if (sizeHints.flags & PBaseSize)
                    {
                        width = sizeHints.base_width;
                        height = sizeHints.base_height;
                    }
                }
            }

            if (lookingForChildren)
                d_stdout("child window bounds %i %i | offset %u %u", width, height, xOffset, yOffset);

            if (width > 1 && height > 1)
            {
                lookingForChildren = false;
                XMoveWindow(display, pluginWindow, xOffset, yOffset);
                XSync(display, True);
                pluginWindowCallbacks->pluginWindowResized(width, height);
            }
        }
       #endif
    }

    void setOffset(const uint x, const uint y)
    {
        xOffset = x;
        yOffset = y;
    }

    void setOffsetBroken(const bool broken)
    {
        brokenOffsetFactor = broken;
    }

    void setSize(const uint width, const uint height)
    {
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
        if (pluginView != nullptr)
            [pluginView setFrameSize:NSMakeSize(width, height)];
#elif defined(DISTRHO_OS_WASM)
#elif defined(DISTRHO_OS_WINDOWS)
        if (pluginWindow != nullptr)
            SetWindowPos(pluginWindow, 0, 0, 0, width, height,
                         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
#else
        if (pluginWindow != 0)
            XResizeWindow(display, pluginWindow, width, height);
#endif
    }
};

PluginHostWindow::PluginHostWindow(void* const windowHandle, Callbacks* const cbs)
  : pData(new PrivateData(windowHandle, cbs)) {}

PluginHostWindow::~PluginHostWindow()
{
    delete pData;
}

void PluginHostWindow::restart()
{
    pData->restart();
}

bool PluginHostWindow::hide()
{
    return pData->hide();
}

void PluginHostWindow::idle()
{
    pData->idle();
}

void PluginHostWindow::setOffset(const uint x, const uint y)
{
    pData->setOffset(x, y);
}

void PluginHostWindow::setOffsetBroken(bool brokenOffsetFactor)
{
    pData->setOffsetBroken(brokenOffsetFactor);
}

void PluginHostWindow::setSize(const uint width, const uint height)
{
    pData->setSize(width, height);
}

END_NAMESPACE_DGL
