
#include "generic_char.hh"

#include <algorithm>
#include <cmath>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

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
                          [[maybe_unused]] Drawable /*drawable*/,
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
    XftColorAllocValue(display,
                       DefaultVisual(display, screen_number),
                       DefaultColormap(display, screen_number),
                       color,
                       &xft_color);
    XftDrawStringUtf8(draw, &xft_color, font, x, y, ToFcUtf8(text), ToInt(text));
    XftColorFree(display,
                 DefaultVisual(display, screen_number),
                 DefaultColormap(display, screen_number),
                 &xft_color);
}

void GenericDrawStringXftEmbossed(Display* display,
                                  [[maybe_unused]] Drawable /*drawable*/,
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

    XftColor xft_embossed{};
    XftColor xft_main{};

    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &embossed_color, &xft_embossed);
    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), color, &xft_main);

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

    // Draw embossed text behind main text - minimal single pixel outline
    if (is_black || is_dark_brown)
    {
        // For black or dark brown text, draw white at bottom and right (2 pixels right for widescreen)
        XftDrawStringUtf8(draw, &xft_embossed, font, x + 2, y + 1, fc_text, text_length);  // bottom-right, 2px right
    }
    else
    {
        // For other colors, draw white at top (original behavior)
        XftDrawStringUtf8(draw, &xft_embossed, font, x, y - 1, fc_text, text_length);     // top
    }

    // Draw main text on top
    XftDrawStringUtf8(draw, &xft_main, font, x, y, fc_text, text_length);

    XftColorFree(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_embossed);
    XftColorFree(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_main);
}

void GenericDrawStringXftWithShadow(Display* display,
                                    [[maybe_unused]] Drawable /*drawable*/,
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

    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &shadow_color, &xft_shadow);
    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), color, &xft_main);

    const FcChar8* fc_text = ToFcUtf8(text);
    const int text_length = ToInt(text);

    // Performance optimization: Limit blur iterations on slower hardware
    // On Raspberry Pi, reduce blur complexity to improve performance
    const int max_blur = (blur_radius > 2) ? 2 : blur_radius;  // Cap at 2 for performance
    
    // Draw shadow with reduced blur iterations for better performance
    for (int blur = 0; blur <= max_blur; ++blur)
    {
        const int blur_offset = blur * 2;
        // Draw only 2 positions instead of 4 for better performance
        XftDrawStringUtf8(draw, &xft_shadow, font, x + offset_x - blur_offset, y + offset_y - blur_offset, fc_text, text_length);
        XftDrawStringUtf8(draw, &xft_shadow, font, x + offset_x + blur_offset, y + offset_y + blur_offset, fc_text, text_length);
    }

    XftDrawStringUtf8(draw, &xft_main, font, x, y, fc_text, text_length);

    XftColorFree(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_shadow);
    XftColorFree(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_main);
}

void GenericDrawStringXftAntialiased(Display* display,
                                     [[maybe_unused]] Drawable /*drawable*/,
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
    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &enhanced_color, &xft_color);
    XftDrawStringUtf8(draw, &xft_color, font, x, y, ToFcUtf8(text), ToInt(text));
    XftColorFree(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_color);
}
