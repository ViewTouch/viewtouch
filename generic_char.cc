#include "generic_char.hh"
#include "basic.hh"
#include "term/term_view.hh"  // For FONT_DEFAULT definition

// Add Xft support for scalable font rendering
#include <X11/Xft/Xft.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** Global Variables ****/
int is_wide_char = 0;   // zero for false, all else for true

// Forward declaration for GetFontInfo - get the current font
extern XftFont *GetFontInfo(int font_id);
extern Display *Dis;  // External display reference

void GenericDrawString(Display *display, Drawable d, GC gc, int x, int y, const genericChar* str, int length)
{
    if (is_wide_char == 0)
    {
        // Use Xft for scalable font rendering instead of deprecated XDrawString
        if (display && d && str && length > 0)
        {
            // Create XftDraw for rendering
            XftDraw *xftdraw = XftDrawCreate(display, d, 
                                           DefaultVisual(display, DefaultScreen(display)), 
                                           DefaultColormap(display, DefaultScreen(display)));
            
            if (xftdraw)
            {
                // Get current font (using default font)
                XftFont *font = GetFontInfo(FONT_DEFAULT);
                
                if (font)
                {
                    // Create XftColor from current GC foreground
                    XGCValues gcv;
                    XGetGCValues(display, gc, GCForeground, &gcv);
                    
                    // Convert pixel to XColor then to XftColor
                    XColor xcolor;
                    xcolor.pixel = gcv.foreground;
                    XQueryColor(display, DefaultColormap(display, DefaultScreen(display)), &xcolor);
                    
                    XRenderColor renderColor = {xcolor.red, xcolor.green, xcolor.blue, 0xffff};
                    XftColor xftColor;
                    XftColorAllocValue(display, DefaultVisual(display, DefaultScreen(display)), 
                                     DefaultColormap(display, DefaultScreen(display)), 
                                     &renderColor, &xftColor);
                    
                    // Draw the text using Xft
                    XftDrawStringUtf8(xftdraw, &xftColor, font, x, y,
                                    reinterpret_cast<const unsigned char*>(str), length);
                    
                    // Clean up
                    XftColorFree(display, DefaultVisual(display, DefaultScreen(display)), 
                               DefaultColormap(display, DefaultScreen(display)), &xftColor);
                }
                
                XftDrawDestroy(xftdraw);
            }
        }
    }
    else
    {
        // not specified yet - would handle wide character rendering
        // For now, fall back to Xft rendering as well
        GenericDrawString(display, d, gc, x, y, str, length);
    }
}

