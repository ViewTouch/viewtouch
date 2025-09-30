
#include "generic_char.hh"

#include <algorithm>

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

    const auto make_color = [](const XRenderColor& base, float multiplier, float offset = 0.0f) {
        XRenderColor result{};
        result.red = static_cast<unsigned short>(std::clamp(base.red * multiplier + offset, 0.0f, 65535.0f));
        result.green = static_cast<unsigned short>(std::clamp(base.green * multiplier + offset, 0.0f, 65535.0f));
        result.blue = static_cast<unsigned short>(std::clamp(base.blue * multiplier + offset, 0.0f, 65535.0f));
        result.alpha = base.alpha;
        return result;
    };

    const XRenderColor shadow_color = make_color(*color, 0.6f);
    const XRenderColor frosted_color = {
        static_cast<unsigned short>(color->red + ((65535 - color->red) * 2) / 5),
        static_cast<unsigned short>(color->green + ((65535 - color->green) * 2) / 5),
        static_cast<unsigned short>(color->blue + ((65535 - color->blue) * 2) / 5),
        static_cast<unsigned short>((color->alpha * 9) / 10)
    };

    XftColor xft_shadow{};
    XftColor xft_frosted{};
    XftColor xft_main{};

    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &shadow_color, &xft_shadow);
    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &frosted_color, &xft_frosted);
    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), color, &xft_main);

    const FcChar8* fc_text = ToFcUtf8(text);
    const int text_length = ToInt(text);

    XftDrawStringUtf8(draw, &xft_shadow, font, x + 1, y + 1, fc_text, text_length);
    XftDrawStringUtf8(draw, &xft_shadow, font, x + 2, y + 1, fc_text, text_length);
    XftDrawStringUtf8(draw, &xft_shadow, font, x + 1, y + 2, fc_text, text_length);

    XftDrawStringUtf8(draw, &xft_frosted, font, x - 1, y - 1, fc_text, text_length);
    XftDrawStringUtf8(draw, &xft_frosted, font, x - 2, y - 1, fc_text, text_length);
    XftDrawStringUtf8(draw, &xft_frosted, font, x - 1, y - 2, fc_text, text_length);

    XftDrawStringUtf8(draw, &xft_main, font, x, y, fc_text, text_length);

    XftColorFree(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_shadow);
    XftColorFree(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &xft_frosted);
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

    const XRenderColor shadow_color{
        static_cast<unsigned short>((color->red * 1) / 4),
        static_cast<unsigned short>((color->green * 1) / 4),
        static_cast<unsigned short>((color->blue * 1) / 4),
        color->alpha
    };

    XftColor xft_shadow{};
    XftColor xft_main{};

    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), &shadow_color, &xft_shadow);
    XftColorAllocValue(display, DefaultVisual(display, screen_number), DefaultColormap(display, screen_number), color, &xft_main);

    const FcChar8* fc_text = ToFcUtf8(text);
    const int text_length = ToInt(text);

    for (int blur = 0; blur <= blur_radius; ++blur)
    {
        const int blur_offset = blur * 2;
        XftDrawStringUtf8(draw, &xft_shadow, font, x + offset_x - blur_offset, y + offset_y - blur_offset, fc_text, text_length);
        XftDrawStringUtf8(draw, &xft_shadow, font, x + offset_x + blur_offset, y + offset_y + blur_offset, fc_text, text_length);
        XftDrawStringUtf8(draw, &xft_shadow, font, x + offset_x - blur_offset, y + offset_y + blur_offset, fc_text, text_length);
        XftDrawStringUtf8(draw, &xft_shadow, font, x + offset_x + blur_offset, y + offset_y - blur_offset, fc_text, text_length);
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
