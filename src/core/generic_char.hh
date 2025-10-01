#pragma once

#include <cstddef>
#include <span>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "basic.hh"

[[nodiscard]] inline std::span<const genericChar> MakeGenericCharSpan(const genericChar* str, int length) noexcept
{
    if (str == nullptr || length <= 0)
    {
        return {};
    }
    return std::span<const genericChar>(str, static_cast<std::size_t>(length));
}

void GenericDrawString(Display* display,
                       Drawable drawable,
                       GC gc,
                       int x,
                       int y,
                       std::span<const genericChar> text);

inline void GenericDrawString(Display* display,
                              Drawable drawable,
                              GC gc,
                              int x,
                              int y,
                              const genericChar* text,
                              int length)
{
    GenericDrawString(display, drawable, gc, x, y, MakeGenericCharSpan(text, length));
}

void GenericDrawStringXft(Display* display,
                          Drawable drawable,
                          XftDraw* draw,
                          XftFont* font,
                          XRenderColor* color,
                          int x,
                          int y,
                          std::span<const genericChar> text,
                          int screen_number);

inline void GenericDrawStringXft(Display* display,
                                 Drawable drawable,
                                 XftDraw* draw,
                                 XftFont* font,
                                 XRenderColor* color,
                                 int x,
                                 int y,
                                 const genericChar* text,
                                 int length,
                                 int screen_number)
{
    GenericDrawStringXft(display,
                         drawable,
                         draw,
                         font,
                         color,
                         x,
                         y,
                         MakeGenericCharSpan(text, length),
                         screen_number);
}

void GenericDrawStringXftEmbossed(Display* display,
                                  Drawable drawable,
                                  XftDraw* draw,
                                  XftFont* font,
                                  XRenderColor* color,
                                  int x,
                                  int y,
                                  std::span<const genericChar> text,
                                  int screen_number);

inline void GenericDrawStringXftEmbossed(Display* display,
                                         Drawable drawable,
                                         XftDraw* draw,
                                         XftFont* font,
                                         XRenderColor* color,
                                         int x,
                                         int y,
                                         const genericChar* text,
                                         int length,
                                         int screen_number)
{
    GenericDrawStringXftEmbossed(display,
                                 drawable,
                                 draw,
                                 font,
                                 color,
                                 x,
                                 y,
                                 MakeGenericCharSpan(text, length),
                                 screen_number);
}

void GenericDrawStringXftWithShadow(Display* display,
                                    Drawable drawable,
                                    XftDraw* draw,
                                    XftFont* font,
                                    XRenderColor* color,
                                    int x,
                                    int y,
                                    std::span<const genericChar> text,
                                    int screen_number,
                                    int offset_x,
                                    int offset_y,
                                    int blur_radius);

inline void GenericDrawStringXftWithShadow(Display* display,
                                           Drawable drawable,
                                           XftDraw* draw,
                                           XftFont* font,
                                           XRenderColor* color,
                                           int x,
                                           int y,
                                           const genericChar* text,
                                           int length,
                                           int screen_number,
                                           int offset_x,
                                           int offset_y,
                                           int blur_radius)
{
    GenericDrawStringXftWithShadow(display,
                                   drawable,
                                   draw,
                                   font,
                                   color,
                                   x,
                                   y,
                                   MakeGenericCharSpan(text, length),
                                   screen_number,
                                   offset_x,
                                   offset_y,
                                   blur_radius);
}

void GenericDrawStringXftAntialiased(Display* display,
                                     Drawable drawable,
                                     XftDraw* draw,
                                     XftFont* font,
                                     XRenderColor* color,
                                     int x,
                                     int y,
                                     std::span<const genericChar> text,
                                     int screen_number);

inline void GenericDrawStringXftAntialiased(Display* display,
                                            Drawable drawable,
                                            XftDraw* draw,
                                            XftFont* font,
                                            XRenderColor* color,
                                            int x,
                                            int y,
                                            const genericChar* text,
                                            int length,
                                            int screen_number)
{
    GenericDrawStringXftAntialiased(display,
                                    drawable,
                                    draw,
                                    font,
                                    color,
                                    x,
                                    y,
                                    MakeGenericCharSpan(text, length),
                                    screen_number);
}
