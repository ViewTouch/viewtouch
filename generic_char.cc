
#include "generic_char.hh"
#include "basic.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** Global Variables ****/
int is_wide_char = 0;   // zero for false, all else for true


void GenericDrawString(Display *display, Drawable d, GC gc, int x, int y, const genericChar* str, int length)
{
    if (is_wide_char == 0)
    {
        XDrawString(display, d, gc, x, y, str, length);
    }
    else
    {
        // not specified yet 
    }
}

void GenericDrawStringXft(Display *display, Drawable d, XftDraw *xftdraw, XftFont *xftfont, XRenderColor *color, int x, int y, const genericChar* str, int length, int screen_no)
{
    if (!xftdraw || !xftfont || !color || !str) return;
    XftColor xft_color;
    XftColorAllocValue(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), color, &xft_color);
    XftDrawStringUtf8(xftdraw, &xft_color, xftfont, x, y, (const FcChar8*)str, length);
    XftColorFree(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &xft_color);
}

void GenericDrawStringXftEmbossed(Display *display, Drawable d, XftDraw *xftdraw, XftFont *xftfont, XRenderColor *color, int x, int y, const genericChar* str, int length, int screen_no)
{
    if (!xftdraw || !xftfont || !color || !str) return;
    
    // Create shadow color (darker version for bottom/right edges)
    XRenderColor shadow_color;
    shadow_color.red = (color->red * 2) / 5;   // 40% of original red - darker shadow
    shadow_color.green = (color->green * 2) / 5; // 40% of original green
    shadow_color.blue = (color->blue * 2) / 5;   // 40% of original blue
    shadow_color.alpha = color->alpha;
    
    // Create frosted highlight color (lighter, more translucent effect for top/left edges)
    XRenderColor frosted_color;
    // Create a frosted effect by adding more white and reducing opacity slightly
    frosted_color.red = color->red + ((65535 - color->red) * 3) / 4;   // Add 75% of remaining white
    frosted_color.green = color->green + ((65535 - color->green) * 3) / 4; // Add 75% of remaining white
    frosted_color.blue = color->blue + ((65535 - color->blue) * 3) / 4;   // Add 75% of remaining white
    frosted_color.alpha = (color->alpha * 9) / 10; // Slightly more translucent for frosted effect
    
    // Allocate Xft colors
    XftColor xft_shadow, xft_frosted, xft_main;
    XftColorAllocValue(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &shadow_color, &xft_shadow);
    XftColorAllocValue(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &frosted_color, &xft_frosted);
    XftColorAllocValue(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), color, &xft_main);
    
    // Draw shadow on bottom and right edges (bottom-right offset)
    XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, x + 1, y + 1, (const FcChar8*)str, length);
    XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, x + 2, y + 1, (const FcChar8*)str, length);
    XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, x + 1, y + 2, (const FcChar8*)str, length);
    
    // Draw frosted highlight on top and left edges (top-left offset)
    XftDrawStringUtf8(xftdraw, &xft_frosted, xftfont, x - 1, y - 1, (const FcChar8*)str, length);
    XftDrawStringUtf8(xftdraw, &xft_frosted, xftfont, x - 2, y - 1, (const FcChar8*)str, length);
    XftDrawStringUtf8(xftdraw, &xft_frosted, xftfont, x - 1, y - 2, (const FcChar8*)str, length);
    
    // Draw main text on top
    XftDrawStringUtf8(xftdraw, &xft_main, xftfont, x, y, (const FcChar8*)str, length);
    
    // Free allocated colors
    XftColorFree(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &xft_shadow);
    XftColorFree(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &xft_frosted);
    XftColorFree(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &xft_main);
}

void GenericDrawStringXftWithShadow(Display *display, Drawable d, XftDraw *xftdraw, XftFont *xftfont, XRenderColor *color, int x, int y, const genericChar* str, int length, int screen_no, int offset_x, int offset_y, int blur_radius)
{
    if (!xftdraw || !xftfont || !color || !str) return;
    
    // Create shadow color (darker version of the text color)
    XRenderColor shadow_color;
    shadow_color.red = (color->red * 1) / 4;   // 25% of original red
    shadow_color.green = (color->green * 1) / 4; // 25% of original green
    shadow_color.blue = (color->blue * 1) / 4;   // 25% of original blue
    shadow_color.alpha = color->alpha;
    
    // Allocate Xft colors
    XftColor xft_shadow, xft_main;
    XftColorAllocValue(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &shadow_color, &xft_shadow);
    XftColorAllocValue(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), color, &xft_main);
    
    // Draw shadow with blur effect (multiple passes for blur)
    for (int blur = 0; blur <= blur_radius; blur++) {
        int blur_offset = blur * 2;
        XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, 
                         x + offset_x - blur_offset, y + offset_y - blur_offset, 
                         (const FcChar8*)str, length);
        XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, 
                         x + offset_x + blur_offset, y + offset_y + blur_offset, 
                         (const FcChar8*)str, length);
        XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, 
                         x + offset_x - blur_offset, y + offset_y + blur_offset, 
                         (const FcChar8*)str, length);
        XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, 
                         x + offset_x + blur_offset, y + offset_y - blur_offset, 
                         (const FcChar8*)str, length);
    }
    
    // Draw main text
    XftDrawStringUtf8(xftdraw, &xft_main, xftfont, x, y, (const FcChar8*)str, length);
    
    // Free allocated colors
    XftColorFree(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &xft_shadow);
    XftColorFree(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &xft_main);
}

void GenericDrawStringXftAntialiased(Display *display, Drawable d, XftDraw *xftdraw, XftFont *xftfont, XRenderColor *color, int x, int y, const genericChar* str, int length, int screen_no)
{
    if (!xftdraw || !xftfont || !color || !str) return;
    
    // Configure Xft for better anti-aliasing
    // Note: Xft automatically uses subpixel rendering when available
    // We can enhance this by using a slightly modified color for better contrast
    
    // Create a slightly enhanced color for better anti-aliasing
    XRenderColor enhanced_color;
    enhanced_color.red = (color->red * 95) / 100;   // Slightly darker for better contrast
    enhanced_color.green = (color->green * 95) / 100;
    enhanced_color.blue = (color->blue * 95) / 100;
    enhanced_color.alpha = color->alpha;
    
    // Allocate Xft color
    XftColor xft_color;
    XftColorAllocValue(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &enhanced_color, &xft_color);
    
    // Draw text with enhanced anti-aliasing
    XftDrawStringUtf8(xftdraw, &xft_color, xftfont, x, y, (const FcChar8*)str, length);
    
    // Free allocated color
    XftColorFree(display, DefaultVisual(display, screen_no), DefaultColormap(display, screen_no), &xft_color);
}