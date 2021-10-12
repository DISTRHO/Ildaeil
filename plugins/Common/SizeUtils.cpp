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

#include "SizeUtils.hpp"

START_NAMESPACE_DGL

#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
#elif defined(DISTRHO_OS_WINDOWS)
#else
static ::Window getChildWindow(::Display* const display, const ::Window ourWindow)
{
    ::Window rootWindow, parentWindow, ret = 0;
    ::Window* childWindows = nullptr;
    uint numChildren = 0;

    XQueryTree(display, ourWindow, &rootWindow, &parentWindow, &childWindows, &numChildren);

    if (numChildren > 0 && childWindows != nullptr)
    {
        ret = childWindows[0];
        XFree(childWindows);
    }

    return ret;
}
#endif

Size<uint> getChildWindowSize(const uintptr_t winId)
{
#if defined(DISTRHO_OS_HAIKU)
#elif defined(DISTRHO_OS_MAC)
    for (NSView* subview in [(NSView*)winId subviews])
    {
        // [subview setFrame:NSMakeRect(0, 0, width, height)];
        d_stdout("found subview");
        return Size<uint>(subview.frame.size.width, subview.frame.size.height);
    }
#elif defined(DISTRHO_OS_WINDOWS)
#else
    if (::Display* const display = XOpenDisplay(nullptr))
    {
        if (const ::Window childWindow = getChildWindow(display, (::Window)winId))
        {
            d_stdout("found child window");

            XSizeHints sizeHints;
            memset(&sizeHints, 0, sizeof(sizeHints));

            if (XGetNormalHints(display, childWindow, &sizeHints))
            {
                int width = 0;
                int height = 0;

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
                else if (sizeHints.flags & PMinSize)
                {
                    width = sizeHints.min_width;
                    height = sizeHints.min_height;
                }

                d_stdout("child window bounds %u %u", width, height);

                if (width > 1 && height > 1)
                {
                    // XMoveWindow(display, (::Window)winId, 0, 40);
                    // XResizeWindow(display, (::Window)winId, width, height);
                    // XMoveWindow(display, childWindow, 0, 40);
                    // XMoveResizeWindow(display, childWindow, 0, 40, width, height);
                    return Size<uint>(static_cast<uint>(width), static_cast<uint>(height));
                }
            }
            else
                d_stdout("child window without bounds");
        }

        XCloseDisplay(display);
    }
#endif
    return Size<uint>(0, 0);
}

END_NAMESPACE_DGL
