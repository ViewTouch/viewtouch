#include <X11/Xlib.h>
#include "basic.hh"
#include <X11/Xft/Xft.h>

void GenericDrawString(Display *, Drawable, GC, int, int, const genericChar* , int);
void GenericDrawStringXft(Display *display, Drawable d, XftDraw *xftdraw, XftFont *xftfont, XRenderColor *color, int x, int y, const genericChar* str, int length, int screen_no);

//stuff
