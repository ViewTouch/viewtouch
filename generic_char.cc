
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

