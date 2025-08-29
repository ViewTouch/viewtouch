#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <iostream>
#include <cstring>

// FontName and FontValue arrays (from main/labels.cc)
const char* FontName[] = {
    "Default",
    "Times 14", "Times 14 Bold", "Times 18", "Times 18 Bold",
    "Times 20", "Times 20 Bold", "Times 24", "Times 24 Bold",
    "Times 34", "Times 34 Bold",
    "Courier 18", "Courier 18 Bold", "Courier 20", "Courier 20 Bold",
    "DejaVu Sans 14", "DejaVu Sans 16", "DejaVu Sans 18", "DejaVu Sans 20",
    "DejaVu Sans 24", "DejaVu Sans 28",
    "DejaVu Sans 14 Bold", "DejaVu Sans 16 Bold", "DejaVu Sans 18 Bold",
    "DejaVu Sans 20 Bold", "DejaVu Sans 24 Bold", "DejaVu Sans 28 Bold",
    "Monospace 14", "Monospace 16", "Monospace 18", "Monospace 20", "Monospace 24",
    "Monospace 14 Bold", "Monospace 16 Bold", "Monospace 18 Bold", "Monospace 20 Bold", "Monospace 24 Bold",
    "EB Garamond 14", "EB Garamond 16", "EB Garamond 18", "EB Garamond 20", "EB Garamond 24", "EB Garamond 28",
    "EB Garamond 14 Bold", "EB Garamond 16 Bold", "EB Garamond 18 Bold", "EB Garamond 20 Bold", "EB Garamond 24 Bold", "EB Garamond 28 Bold",
    "Bookman 14", "Bookman 16", "Bookman 18", "Bookman 20", "Bookman 24", "Bookman 28",
    "Bookman 14 Bold", "Bookman 16 Bold", "Bookman 18 Bold", "Bookman 20 Bold", "Bookman 24 Bold", "Bookman 28 Bold",
    "Nimbus Roman 14", "Nimbus Roman 16", "Nimbus Roman 18", "Nimbus Roman 20", "Nimbus Roman 24", "Nimbus Roman 28",
    "Nimbus Roman 14 Bold", "Nimbus Roman 16 Bold", "Nimbus Roman 18 Bold", "Nimbus Roman 20 Bold", "Nimbus Roman 24 Bold", "Nimbus Roman 28 Bold", nullptr
};

const int FontValue[] = {
    0,
    10, 11, 12, 13, 4, 7, 5, 8, 6, 9,
    14, 15, 16, 17,
    18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, -1
};

// GetScalableFontName (from main/manager.cc)
const char* GetScalableFontName(int font_id) {
    switch (font_id) {
    case 10:  return "Times New Roman-14:style=Regular";
    case 12:  return "Times New Roman-18:style=Regular";
    case 4:   return "Times New Roman-20:style=Regular";
    case 5:   return "Times New Roman-24:style=Regular";
    case 6:   return "Times New Roman-34:style=Regular";
    case 11:  return "Times New Roman-14:style=Bold";
    case 13:  return "Times New Roman-18:style=Bold";
    case 7:   return "Times New Roman-20:style=Bold";
    case 8:   return "Times New Roman-24:style=Bold";
    case 9:   return "Times New Roman-34:style=Bold";
    case 14:  return "Courier New-18:style=Regular";
    case 15:  return "Courier New-18:style=Bold";
    case 16:  return "Courier New-20:style=Regular";
    case 17:  return "Courier New-20:style=Bold";
    case 18:  return "DejaVu Sans-14:style=Book";
    case 19:  return "DejaVu Sans-16:style=Book";
    case 20:  return "DejaVu Sans-18:style=Book";
    case 21:  return "DejaVu Sans-20:style=Book";
    case 22:  return "DejaVu Sans-24:style=Book";
    case 23:  return "DejaVu Sans-28:style=Book";
    case 24:  return "DejaVu Sans-14:style=Bold";
    case 25:  return "DejaVu Sans-16:style=Bold";
    case 26:  return "DejaVu Sans-18:style=Bold";
    case 27:  return "DejaVu Sans-20:style=Bold";
    case 28:  return "DejaVu Sans-24:style=Bold";
    case 29:  return "DejaVu Sans-28:style=Bold";
    case 30:  return "DejaVu Sans Mono-14:style=Book";
    case 31:  return "DejaVu Sans Mono-16:style=Book";
    case 32:  return "DejaVu Sans Mono-18:style=Book";
    case 33:  return "DejaVu Sans Mono-20:style=Book";
    case 34:  return "DejaVu Sans Mono-24:style=Book";
    case 35:  return "DejaVu Sans Mono-14:style=Bold";
    case 36:  return "DejaVu Sans Mono-16:style=Bold";
    case 37:  return "DejaVu Sans Mono-18:style=Bold";
    case 38:  return "DejaVu Sans Mono-20:style=Bold";
    case 39:  return "DejaVu Sans Mono-24:style=Bold";
    case 40:  return "EB Garamond-14:style=Regular";
    case 41:  return "EB Garamond-16:style=Regular";
    case 42:  return "EB Garamond-18:style=Regular";
    case 43:  return "EB Garamond-20:style=Regular";
    case 44:  return "EB Garamond-24:style=Regular";
    case 45:  return "EB Garamond-28:style=Regular";
    case 46:  return "EB Garamond-14:style=Bold";
    case 47:  return "EB Garamond-16:style=Bold";
    case 48:  return "EB Garamond-18:style=Bold";
    case 49:  return "EB Garamond-20:style=Bold";
    case 50:  return "EB Garamond-24:style=Bold";
    case 51:  return "EB Garamond-28:style=Bold";
    case 52:  return "URW Bookman-14:style=Light";
    case 53:  return "URW Bookman-16:style=Light";
    case 54:  return "URW Bookman-18:style=Light";
    case 55:  return "URW Bookman-20:style=Light";
    case 56:  return "URW Bookman-24:style=Light";
    case 57:  return "URW Bookman-28:style=Light";
    case 58:  return "URW Bookman-14:style=Demi";
    case 59:  return "URW Bookman-16:style=Demi";
    case 60:  return "URW Bookman-18:style=Demi";
    case 61:  return "URW Bookman-20:style=Demi";
    case 62:  return "URW Bookman-24:style=Demi";
    case 63:  return "URW Bookman-28:style=Demi";
    case 64:  return "Nimbus Roman-14:style=Regular";
    case 65:  return "Nimbus Roman-16:style=Regular";
    case 66:  return "Nimbus Roman-18:style=Regular";
    case 67:  return "Nimbus Roman-20:style=Regular";
    case 68:  return "Nimbus Roman-24:style=Regular";
    case 69:  return "Nimbus Roman-28:style=Regular";
    case 70:  return "Nimbus Roman-14:style=Bold";
    case 71:  return "Nimbus Roman-16:style=Bold";
    case 72:  return "Nimbus Roman-18:style=Bold";
    case 73:  return "Nimbus Roman-20:style=Bold";
    case 74:  return "Nimbus Roman-24:style=Bold";
    case 75:  return "Nimbus Roman-28:style=Bold";
    default:  return "DejaVu Sans-18:style=Book";
    }
}

// Text enhancement settings (defaults for font checker)
static int use_embossed_text = 0;
static int use_text_antialiasing = 1;
static int use_drop_shadows = 0;
static int shadow_offset_x = 2;
static int shadow_offset_y = 2;
static int shadow_blur_radius = 1;

// Enhanced text rendering function for font checker
void FontCheckDrawStringEnhanced(Display *display, XftDraw *xftdraw, XftFont *xftfont, XftColor *color, 
                                int x, int y, const char* str, int length)
{
    if (!xftdraw || !xftfont || !color || !str) return;
    
    if (use_embossed_text) {
        // Create frosted embossed effect
        XRenderColor shadow_color, frosted_color;
        XftColor xft_shadow, xft_frosted;
        
        // Shadow color (darker version maintaining color balance)
        shadow_color.red = (color->color.red * 3) / 5;     // 60% intensity - darker but balanced
        shadow_color.green = (color->color.green * 3) / 5; // 60% intensity - maintains color balance
        shadow_color.blue = (color->color.blue * 3) / 5;   // 60% intensity - maintains color balance
        shadow_color.alpha = color->color.alpha;
        
        // Frosted color (lighter version with subtle enhancement)
        frosted_color.red = color->color.red + ((65535 - color->color.red) * 2) / 5;     // Add 40% brightness
        frosted_color.green = color->color.green + ((65535 - color->color.green) * 2) / 5; // Add 40% brightness
        frosted_color.blue = color->color.blue + ((65535 - color->color.blue) * 2) / 5;   // Add 40% brightness
        frosted_color.alpha = (color->color.alpha * 9) / 10;
        
        XftColorAllocValue(display, DefaultVisual(display, DefaultScreen(display)), 
                          DefaultColormap(display, DefaultScreen(display)), &shadow_color, &xft_shadow);
        XftColorAllocValue(display, DefaultVisual(display, DefaultScreen(display)), 
                          DefaultColormap(display, DefaultScreen(display)), &frosted_color, &xft_frosted);
        
        // Draw shadow
        XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, x + 1, y + 1, (const FcChar8*)str, length);
        XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, x + 2, y + 1, (const FcChar8*)str, length);
        XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, x + 1, y + 2, (const FcChar8*)str, length);
        
        // Draw frosted highlight
        XftDrawStringUtf8(xftdraw, &xft_frosted, xftfont, x - 1, y - 1, (const FcChar8*)str, length);
        XftDrawStringUtf8(xftdraw, &xft_frosted, xftfont, x - 2, y - 1, (const FcChar8*)str, length);
        XftDrawStringUtf8(xftdraw, &xft_frosted, xftfont, x - 1, y - 2, (const FcChar8*)str, length);
        
        // Draw main text
        XftDrawStringUtf8(xftdraw, color, xftfont, x, y, (const FcChar8*)str, length);
        
        XftColorFree(display, DefaultVisual(display, DefaultScreen(display)), 
                    DefaultColormap(display, DefaultScreen(display)), &xft_shadow);
        XftColorFree(display, DefaultVisual(display, DefaultScreen(display)), 
                    DefaultColormap(display, DefaultScreen(display)), &xft_frosted);
    } else if (use_drop_shadows) {
        // Create drop shadow effect
        XRenderColor shadow_color;
        XftColor xft_shadow;
        
        shadow_color.red = (color->color.red * 1) / 4;
        shadow_color.green = (color->color.green * 1) / 4;
        shadow_color.blue = (color->color.blue * 1) / 4;
        shadow_color.alpha = color->color.alpha;
        
        XftColorAllocValue(display, DefaultVisual(display, DefaultScreen(display)), 
                          DefaultColormap(display, DefaultScreen(display)), &shadow_color, &xft_shadow);
        
        // Draw shadow with blur
        for (int blur = 0; blur <= shadow_blur_radius; blur++) {
            int blur_offset = blur * 2;
            XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, 
                             x + shadow_offset_x - blur_offset, y + shadow_offset_y - blur_offset, 
                             (const FcChar8*)str, length);
            XftDrawStringUtf8(xftdraw, &xft_shadow, xftfont, 
                             x + shadow_offset_x + blur_offset, y + shadow_offset_y + blur_offset, 
                             (const FcChar8*)str, length);
        }
        
        // Draw main text
        XftDrawStringUtf8(xftdraw, color, xftfont, x, y, (const FcChar8*)str, length);
        
        XftColorFree(display, DefaultVisual(display, DefaultScreen(display)), 
                    DefaultColormap(display, DefaultScreen(display)), &xft_shadow);
    } else if (use_text_antialiasing) {
        // Enhanced anti-aliased text
        XRenderColor enhanced_color;
        XftColor xft_enhanced;
        
        enhanced_color.red = (color->color.red * 95) / 100;
        enhanced_color.green = (color->color.green * 95) / 100;
        enhanced_color.blue = (color->color.blue * 95) / 100;
        enhanced_color.alpha = color->color.alpha;
        
        XftColorAllocValue(display, DefaultVisual(display, DefaultScreen(display)), 
                          DefaultColormap(display, DefaultScreen(display)), &enhanced_color, &xft_enhanced);
        XftDrawStringUtf8(xftdraw, &xft_enhanced, xftfont, x, y, (const FcChar8*)str, length);
        XftColorFree(display, DefaultVisual(display, DefaultScreen(display)), 
                    DefaultColormap(display, DefaultScreen(display)), &xft_enhanced);
    } else {
        // Standard rendering
        XftDrawStringUtf8(xftdraw, color, xftfont, x, y, (const FcChar8*)str, length);
    }
}

int main() {
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "Cannot open display" << std::endl;
        return 1;
    }
    int screen = DefaultScreen(display);
    int win_width = 700, win_height = 200;
    Window win = XCreateSimpleWindow(display, RootWindow(display, screen), 100, 100, win_width, win_height, 1, BlackPixel(display, screen), WhitePixel(display, screen));
    XSelectInput(display, win, ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(display, win);
    XFlush(display);

    XftDraw* draw = nullptr;
    XftColor xft_color;
    XRenderColor render_color = {0, 0, 0, 65535}; // black
    XftColorAllocValue(display, DefaultVisual(display, screen), DefaultColormap(display, screen), &render_color, &xft_color);

    for (int i = 0; FontValue[i] != -1; ++i) {
        int font_id = FontValue[i];
        const char* font_label = FontName[i];
        const char* font_xft = GetScalableFontName(font_id);
        XftFont *font = XftFontOpenName(display, screen, font_xft);
        bool success = false;
        if (!font) {
            // Show error visually
            XClearWindow(display, win);
            if (!draw) draw = XftDrawCreate(display, win, DefaultVisual(display, screen), DefaultColormap(display, screen));
            FontCheckDrawStringEnhanced(display, draw, font, &xft_color, 20, 80, "FAILED TO LOAD FONT", 19);
            FontCheckDrawStringEnhanced(display, draw, font, &xft_color, 20, 120, font_xft, strlen(font_xft));
            XFlush(display);
        } else {
            XClearWindow(display, win);
            if (!draw) draw = XftDrawCreate(display, win, DefaultVisual(display, screen), DefaultColormap(display, screen));
            // Draw font label and Xft string
            std::string label = std::string(font_label) + " (" + font_xft + ")";
            FontCheckDrawStringEnhanced(display, draw, font, &xft_color, 20, 40, label.c_str(), label.length());
            // Draw sample text
            const char* sample = "The quick brown fox jumps over 1234567890";
            FontCheckDrawStringEnhanced(display, draw, font, &xft_color, 20, 100, sample, strlen(sample));
            XFlush(display);
            XftFontClose(display, font);
            success = true;
        }
        // Wait for key or mouse press
        XEvent ev;
        while (1) {
            XNextEvent(display, &ev);
            if (ev.type == KeyPress || ev.type == ButtonPress) break;
        }
        // Print result to terminal
        if (success) {
            std::cout << "\u2713 " << font_label << " (" << font_xft << ") - DISPLAYED" << std::endl;
        } else {
            std::cout << "\u2717 " << font_label << " (" << font_xft << ") - FAILED" << std::endl;
        }
    }
    if (draw) XftDrawDestroy(draw);
    XDestroyWindow(display, win);
    XCloseDisplay(display);
    return 0;
} 