
#include "generic_char.hh"

#include <algorithm>
#include <cmath>

#ifdef DMALLOC
#include <dmalloc.h>
#endif
#include "x11_safe.hh"

namespace
{
[[maybe_unused]] bool g_isWideChar = false; // zero for false, all else for true

[[nodiscard]] constexpr int ToInt(std::span<const genericChar> text) noexcept
{
    return static_cast<int>(text.size());
}

[[nodiscard]] const FcChar8* ToFcUtf8(std::span<const genericChar> text) noexcept
{
    return reinterpret_cast<const FcChar8*>(text.data());
}

} // namespace

void GenericDrawString(Display* display,
                       Drawable drawable,
                       GC gc,
                       int x,
                       int y,
                       std::span<const genericChar> text)
{
    if (display == nullptr || gc == nullptr || text.empty())
    {
        return;
    }

    if (!g_isWideChar)
    {
        XDrawString(display, drawable, gc, x, y, text.data(), ToInt(text));
    }
    else
    {
        // Wide character mode is not yet specified.
    }
}

void GenericDrawStringXft(Display* display,
                          Drawable drawable,
                          XftDraw* draw,
                          XftFont* font,
                          XRenderColor* color,
                          int x,
                          int y,
                          std::span<const genericChar> text,
                          int screen_number)
{
    if (display == nullptr || draw == nullptr || font == nullptr || color == nullptr || text.empty())
    {
        return;
    }

    XftColor xft_color{};
    Bool alloc_ok = XftColorAllocValue(display,
                       DefaultVisual(display, screen_number),
                       DefaultColormap(display, screen_number),
                       color,
                       &xft_color);
    if (alloc_ok) {
        XftDrawStringUtf8(draw, &xft_color, font, x, y, ToFcUtf8(text), ToInt(text));
        XftColorFreeSafe(display,
                         DefaultVisual(display, screen_number),
                         DefaultColormap(display, screen_number),
                         &xft_color);
    } else {
        // Fallback to simple XDrawString if color allocation fails
        GC gc = XCreateGC(display, drawable, 0, nullptr);
        XSetForeground(display, gc, BlackPixel(display, screen_number));
        GenericDrawString(display, drawable, gc, x, y, text);
        XFreeGC(display, gc);
    }
}

void GenericDrawStringXftEmbossed(Display* display,
                                  Drawable drawable,
                                  XftDraw* draw,
                                  XftFont* font,
                                  XRenderColor* color,
                                  int x,
                                  int y,
                                  std::span<const genericChar> text,
                                  int screen_number)
{
    if (display == nullptr || draw == nullptr || font == nullptr || color == nullptr || text.empty())
    {
        return;
    }

    // Embossed text uses white color barely visible around the edges (drawn behind main text)
    const XRenderColor embossed_color{65535, 65535, 65535, color->alpha}; // White with full opacity
    // Black color for edges on White and Yellow text
    const XRenderColor black_color{0, 0, 0, color->alpha}; // Black with full opacity

    XftColor xft_embossed{};
    XftColor xft_black{};
    XftColor xft_main{};

    Bool ok_embossed = XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &embossed_color, &xft_embossed);
    Bool ok_black = XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &black_color, &xft_black);
    Bool ok_main = XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), color, &xft_main);

    const FcChar8* fc_text = ToFcUtf8(text);
    const int text_length = ToInt(text);

    // Check if text color is black (or very dark) or dark brown
    // Black is considered when all RGB components are very low (< 1000 out of 65535)
    // Dark brown RGB is {80, 45, 25} in 8-bit, which scales to {20480, 11520, 6400} in 16-bit
    // Use a range check to account for slight variations
    bool is_black = (color->red < 1000 && color->green < 1000 && color->blue < 1000);
    bool is_dark_brown = (color->red >= 20000 && color->red <= 21000 && 
                          color->green >= 11000 && color->green <= 12000 && 
                          color->blue >= 6000 && color->blue <= 7000);

    // Check if text color is white or yellow
    // White RGB is {255, 255, 255} in 8-bit, which scales to {65535, 65535, 65535} in 16-bit
    // Yellow RGB is {255, 255, 0} in 8-bit, which scales to {65535, 65535, 0} in 16-bit
    bool is_white = (color->red >= 60000 && color->green >= 60000 && color->blue >= 60000);
    bool is_yellow = (color->red >= 60000 && color->green >= 60000 && color->blue < 1000);

    // If we couldn't allocate a main color, fall back to a simple draw
    if (!ok_main) {
        GC gc = XCreateGC(display, drawable, 0, nullptr);
        XSetForeground(display, gc, BlackPixel(display, screen_number));
        GenericDrawString(display, drawable, gc, x, y, text);
        XFreeGC(display, gc);
        // Free any partial allocations
        if (ok_black) XftColorFreeSafe(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_black);
        if (ok_embossed) XftColorFreeSafe(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_embossed);
        return;
    }

    // Choose fallback pointers for decorative colors when they failed to allocate
    XftColor* embossed_ptr = ok_embossed ? &xft_embossed : &xft_main;
    XftColor* black_ptr = ok_black ? &xft_black : &xft_main;

    // Draw embossed text behind main text - minimal single pixel outline
    if (is_black || is_dark_brown)
    {
        XftDrawStringUtf8(draw, embossed_ptr, font, x + 2, y + 1, fc_text, text_length);  // bottom-right, 2px right
    }
    else if (is_white || is_yellow)
    {
        XftDrawStringUtf8(draw, black_ptr, font, x + 1, y + 1, fc_text, text_length);  // bottom-right edge
    }
    else
    {
        XftDrawStringUtf8(draw, embossed_ptr, font, x, y - 1, fc_text, text_length);     // top
    }

    // Draw main text on top
    XftDrawStringUtf8(draw, &xft_main, font, x, y, fc_text, text_length);

    if (ok_black) XftColorFreeSafe(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_black);
    if (ok_embossed) XftColorFreeSafe(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_embossed);
    XftColorFreeSafe(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_main);
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
                                    int blur_radius)
{
    if (display == nullptr || draw == nullptr || font == nullptr || color == nullptr || text.empty())
    {
        return;
    }

    // For very dark colors (like black), use white shadows for better readability
    const bool is_dark_color = (color->red < 1000 && color->green < 1000 && color->blue < 1000);
    const XRenderColor shadow_color = is_dark_color ?
        XRenderColor{65535, 65535, 65535, color->alpha} : // White shadow for dark text
        XRenderColor{static_cast<unsigned short>((color->red * 1) / 4),
                     static_cast<unsigned short>((color->green * 1) / 4),
                     static_cast<unsigned short>((color->blue * 1) / 4),
                     color->alpha}; // Darker shadow for light text

    XftColor xft_shadow{};
    XftColor xft_main{};

    Bool ok_shadow = XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &shadow_color, &xft_shadow);
    Bool ok_main = XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), color, &xft_main);

    const FcChar8* fc_text = ToFcUtf8(text);
    const int text_length = ToInt(text);

    // Performance optimization: Limit blur iterations on slower hardware
    // On Raspberry Pi, reduce blur complexity to improve performance
    const int max_blur = (blur_radius > 2) ? 2 : blur_radius;  // Cap at 2 for performance
    
    // If we couldn't allocate a main color, fall back to simple draw
    if (!ok_main) {
        GC gc = XCreateGC(display, drawable, 0, nullptr);
        XSetForeground(display, gc, BlackPixel(display, screen_number));
        GenericDrawString(display, drawable, gc, x, y, text);
        XFreeGC(display, gc);
        if (ok_shadow) XftColorFreeSafe(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_shadow);
        return;
    }

    // Draw shadow with reduced blur iterations for better performance
    XftColor* shadow_ptr = ok_shadow ? &xft_shadow : &xft_main;
    for (int blur = 0; blur <= max_blur; ++blur)
    {
        const int blur_offset = blur * 2;
        // Draw only 2 positions instead of 4 for better performance
        XftDrawStringUtf8(draw, shadow_ptr, font, x + offset_x - blur_offset, y + offset_y - blur_offset, fc_text, text_length);
        XftDrawStringUtf8(draw, shadow_ptr, font, x + offset_x + blur_offset, y + offset_y + blur_offset, fc_text, text_length);
    }

    XftDrawStringUtf8(draw, &xft_main, font, x, y, fc_text, text_length);

    if (ok_shadow) XftColorFreeSafe(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_shadow);
    XftColorFreeSafe(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_main);
}

void GenericDrawStringXftAntialiased(Display* display,
                                     Drawable drawable,
                                     XftDraw* draw,
                                     XftFont* font,
                                     XRenderColor* color,
                                     int x,
                                     int y,
                                     std::span<const genericChar> text,
                                     int screen_number)
{
    if (display == nullptr || draw == nullptr || font == nullptr || color == nullptr || text.empty())
    {
        return;
    }

    const XRenderColor enhanced_color{
        static_cast<unsigned short>((color->red * 95) / 100),
        static_cast<unsigned short>((color->green * 95) / 100),
        static_cast<unsigned short>((color->blue * 95) / 100),
        color->alpha
    };

    XftColor xft_color{};
    Bool ok = XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &enhanced_color, &xft_color);
    if (ok) {
        XftDrawStringUtf8(draw, &xft_color, font, x, y, ToFcUtf8(text), ToInt(text));
        XftColorFreeSafe(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_color);
    } else {
        GC gc = XCreateGC(display, drawable, 0, nullptr);
        XSetForeground(display, gc, BlackPixel(display, screen_number));
        GenericDrawString(display, drawable, gc, x, y, text);
        XFreeGC(display, gc);
    }
}
