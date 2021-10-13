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

#include "src/DistrhoDefines.h"

#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
# import <Cocoa/Cocoa.h>
#elif defined(DISTRHO_OS_WINDOWS)
#else
# include <X11/Xlib.h>
# include <X11/Xutil.h>
#endif

#include "PluginHostWindow.hpp"

#ifdef DISTRHO_OS_MAC
@interface IldaeilPluginView : NSView
- (void)resizeWithOldSuperviewSize:(NSSize)oldSize;
@end
@implementation IldaeilPluginView
- (void)resizeWithOldSuperviewSize:(NSSize)oldSize
{
    [super resizeWithOldSuperviewSize:oldSize];
}
@end
#endif

START_NAMESPACE_DGL

#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
#elif defined(DISTRHO_OS_WINDOWS)
#else
#endif

struct PluginHostWindow::PrivateData
{
    Window& parentWindow;
    const uintptr_t parentWindowId;
    Callbacks* const pluginWindowCallbacks;

#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
    IldaeilPluginView* view;
#elif defined(DISTRHO_OS_WINDOWS)
#else
#endif

    bool lookingForChildren;

    PrivateData(Window& pw, Callbacks* const cbs)
      : parentWindow(pw),
        parentWindowId(pw.getNativeWindowHandle()),
        pluginWindowCallbacks(cbs),
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
        view(nullptr),
#elif defined(DISTRHO_OS_WINDOWS)
#else
#endif
        lookingForChildren(false)
    {
        view = [[IldaeilPluginView new]retain];
        DISTRHO_SAFE_ASSERT_RETURN(view != nullptr,)

        [view setAutoresizingMask:NSViewNotSizable];
        [view setAutoresizesSubviews:YES];
        [view setHidden:YES];
        [(NSView*)parentWindowId addSubview:view];
    }

    ~PrivateData()
    {
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
        if (view != nullptr)
        {
            [view release];
            view = nullptr;
        }
#elif defined(DISTRHO_OS_WINDOWS)
#else
#endif
    }

    void* attachAndGetWindowHandle()
    {
        lookingForChildren = true;
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
        return view;
#elif defined(DISTRHO_OS_WINDOWS)
#else
#endif
    }

    void detach()
    {
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
        if (view != nullptr)
            [view setHidden:YES];
#elif defined(DISTRHO_OS_WINDOWS)
#else
#endif
    }

    void idle()
    {
        if (lookingForChildren)
        {
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
            if (view == nullptr)
                return;

            for (NSView* subview in [view subviews])
            {
                const double scaleFactor = [[[view window] screen] backingScaleFactor];
                const NSSize size = [subview frame].size;
                const double width = size.width;
                const double height = size.height;

                d_stdout("found subview %f %f", width, height);

                if (width <= 1 || height <= 1)
                    break;

                lookingForChildren = false;
                [view setFrameSize:size];
                [view setHidden:NO];
                [view setNeedsDisplay:YES];
                pluginWindowCallbacks->pluginWindowResized(width * scaleFactor, height * scaleFactor);
                break;
            }
#elif defined(DISTRHO_OS_WINDOWS)
#else
#endif
        }
    }

    void setPositionAndSize(const uint x, const uint y, const uint width, const uint height)
    {
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
        if (view != nullptr)
        {
            const double scaleFactor = [[[view window] screen] backingScaleFactor];
            [view setFrame:NSMakeRect(x / scaleFactor, y / scaleFactor, width / scaleFactor, height / scaleFactor)];
        }
#elif defined(DISTRHO_OS_WINDOWS)
#else
#endif
    }
};

PluginHostWindow::PluginHostWindow(Window& parentWindow, Callbacks* const cbs)
  : pData(new PrivateData(parentWindow, cbs)) {}

PluginHostWindow::~PluginHostWindow()
{
    delete pData;
}

void* PluginHostWindow::attachAndGetWindowHandle()
{
    return pData->attachAndGetWindowHandle();
}

void PluginHostWindow::detach()
{
    pData->detach();
}

void PluginHostWindow::idle()
{
    pData->idle();
}

void PluginHostWindow::setPositionAndSize(const uint x, const uint y, const uint width, const uint height)
{
    pData->setPositionAndSize(x, y, width, height);
}

END_NAMESPACE_DGL
