
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

