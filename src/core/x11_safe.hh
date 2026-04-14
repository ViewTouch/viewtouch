#pragma once

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Intrinsic.h>
#include <mutex>
// <X11/Xrender.h> is optional on some systems and not required for
// these safe Xft wrappers; avoid requiring it so the code builds
// on systems that only provide Xlib/Xft headers.

static int VTLocalIgnoreXError(Display* dpy, XErrorEvent* ev)
{
    (void)dpy; (void)ev; return 0;
}

// Use a single mutex to serialize temporary changes to the global
// X error handler and calls to XSync/X* that must not run concurrently
// with other code that modifies the handler. This prevents races where
// multiple threads override and restore the handler concurrently.
inline void XftDrawDestroySafe(Display* dpy, XftDraw* draw)
{
    if (!draw || !dpy) return;
    static std::mutex x11_error_mtx;
    std::lock_guard<std::mutex> guard(x11_error_mtx);
    XErrorHandler old = XSetErrorHandler(VTLocalIgnoreXError);
    XSync(dpy, False);
    XftDrawDestroy(draw);
    XSync(dpy, False);
    XSetErrorHandler(old);
}

inline void XftColorFreeSafe(Display* dpy, Visual* vis, Colormap cmap, XftColor* color)
{
    if (!color || !dpy) return;
    static std::mutex x11_error_mtx;
    std::lock_guard<std::mutex> guard(x11_error_mtx);
    XErrorHandler old = XSetErrorHandler(VTLocalIgnoreXError);
    XSync(dpy, False);
    XftColorFree(dpy, vis, cmap, color);
    XSync(dpy, False);
    XSetErrorHandler(old);
}

// Thread-safe display close helpers: protect against concurrent closes
// and temporarily ignore X errors during shutdown. These helpers lock
// a mutex to ensure only one thread attempts to close a given Display
// at a time and set the pointer to nullptr after closing.
inline void XCloseDisplaySafe(Display*& dpy)
{
    static std::mutex x11_error_mtx;
    std::lock_guard<std::mutex> lk(x11_error_mtx);
    if (!dpy) return;
    XErrorHandler old = XSetErrorHandler(VTLocalIgnoreXError);
    XSync(dpy, False);
    XCloseDisplay(dpy);
    XSetErrorHandler(old);
    dpy = nullptr;
}

inline void XtCloseDisplaySafe(Display*& dpy)
{
    static std::mutex x11_error_mtx;
    std::lock_guard<std::mutex> lk(x11_error_mtx);
    if (!dpy) return;
    XErrorHandler old = XSetErrorHandler(VTLocalIgnoreXError);
    XSync(dpy, False);
    XtCloseDisplay(dpy);
    XSetErrorHandler(old);
    dpy = nullptr;
}
