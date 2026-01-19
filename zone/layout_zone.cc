/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
 *   This program is free software: you can redistribute it and/or modify 
 *   it under the terms of the GNU General Public License as published by 
 *   the Free Software Foundation, either version 3 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful, 
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *   GNU General Public License for more details. 
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * layout_zone.cc - revision 67 (8/31/98)
 * Implementation of LayoutZone base class
 */

#include "layout_zone.hh"
#include "terminal.hh"
#include "image_data.hh"
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** LayoutZone Class ****/
// Constructor
LayoutZone::LayoutZone()
{
    selected_x   = -1;
    selected_y   = -1;
    size_x       =  0.0;
    min_size_x   =  1.0;
    max_size_x   =  0.0;
    size_y       =  0.0;
    min_size_y   =  1.0;
    max_size_y   =  0.0;
    font_width   =  0;
    font_height  =  0;
    left_margin  =  0;
    right_margin =  0;
}

// Member Functions
RenderResult LayoutZone::Render(Terminal *term, int update_flag)
{
    FnTrace("LayoutZone::Render()");
    RenderZone(term, nullptr, update_flag);
    term->FontSize(font, font_width, font_height);

    int lw = w - (border * 2) - left_margin - right_margin;
    int lh = h - (border * 2) - header      - footer;
    size_x = (Flt) lw / (Flt) font_width;
    size_y = (Flt) lh / (Flt) font_height;
    max_size_x = (Flt) (w - (border * 2)) / (Flt) font_width;
    max_size_y = (Flt) (h - (border * 2)) / (Flt) font_height;

    return RENDER_OKAY;
}

SignalResult LayoutZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("LayoutZone::Touch()");
    selected_x = (Flt) (tx - (x + border + left_margin)) / (Flt) font_width;
    selected_y = (Flt) (ty - (y + border + header))      / (Flt) font_height;

    return SIGNAL_OKAY;
}

int LayoutZone::SetSize(Terminal *t, int width, int height)
{
    FnTrace("LayoutZone::SetSize()");
    int mw = (int) (min_size_x * (Flt) font_width) +
        (border * 2) + left_margin + right_margin;
    mw -= mw % t->grid_x;
    if (width < mw)
        width = mw;

    int mh = (int) (min_size_y * (Flt) font_height) +
        (border * 2) + header + footer;
    mh -= mh % t->grid_y;
    if (height < mh)
        height = mh;

    w = width;
    h = height;
    return 0;
}

Flt LayoutZone::TextWidth(Terminal *t, const genericChar* str, int len)
{
    if (str == nullptr)
        return 0.0;

    if (len <= 0)
        len = strlen(str);
    return (Flt) t->TextWidth(str, len, font) / (Flt) font_width;
}

int LayoutZone::SetMargins(int left, int right)
{
    FnTrace("LayoutZone::SetMargins()");
    left_margin  = left;
    right_margin = right;
    int lw = w - (border * 2) - left_margin - right_margin;
    int lh = h - (border * 2) - header      - footer;
    size_x = (Flt) lw / (Flt) font_width;
    size_y = (Flt) lh / (Flt) font_height;
    return 0;
}

int LayoutZone::TextL(Terminal *t, Flt line, const genericChar* text, int my_color, int mode)
{
    FnTrace("LayoutZone::TextL()");
    if (line < 0.0 || line >= size_y || text == nullptr)
        return 1;  // Can't draw text

    if (my_color == COLOR_DEFAULT)
        my_color = t->TextureTextColor(texture[0]);

    int pw = w - (border * 2) - left_margin - right_margin;
    int px = x + border + left_margin;
    int py = y + border + header + (int) (line * (Flt) font_height);
    t->RenderText(text, px, py, my_color, font, ALIGN_LEFT, pw, mode);
    return 0;
}

int LayoutZone::TextC(Terminal *t, Flt line, const genericChar* text, int my_color, int mode)
{
    FnTrace("LayoutZone::TextC()");
    if (line < 0.0 || line >= size_y || text == nullptr)
        return 1;  // Can't draw text

    if (my_color == COLOR_DEFAULT)
        my_color = t->TextureTextColor(texture[0]);

    int pw = w - (border * 2) - left_margin - right_margin;
    int px = x + border + left_margin + (pw / 2);
    int py = y + border + header + (int) (line * (Flt) font_height);
    t->RenderText(text, px, py, my_color, font, ALIGN_CENTER, pw, mode);
    return 0;
}

int LayoutZone::TextR(Terminal *t, Flt line, const genericChar* text, int my_color, int mode)
{
    FnTrace("LayoutZone::TextR()");
    if (line < 0.0 || line >= size_y || text == nullptr)
        return 1;  // Can't draw text

    if (my_color == COLOR_DEFAULT)
        my_color = t->TextureTextColor(texture[0]);

    int len = strlen(text), pos = 0;
    if (size_x < len)
    {
        pos = len - (int) size_x;
        len = (int) size_x;
    }

    int px = x + w - border - right_margin;
    int py = y + border + header + (int) (line * (Flt) font_height);
    t->RenderTextLen(&text[pos], len, px, py, my_color, font, ALIGN_RIGHT, 0, mode);
    return 0;
}

int LayoutZone::TextPosL(Terminal *t, Flt row, Flt line, const genericChar* text,
                         int my_color, int mode)
{
    FnTrace("LayoutZone::TextPosL()");
    if (line < 0.0 || line >= size_y || text == nullptr)
        return 1;  // Can't draw text

    if (my_color == COLOR_DEFAULT)
        my_color = t->TextureTextColor(texture[0]);

    int offset = (int) (row * (Flt) font_width) + border;
    int px = x + left_margin + offset;
    int py = y + border + header + (int) (line * (Flt) font_height);
    int pw = w - border - offset - left_margin - right_margin;
    if (pw <= 0)
        return 1;

    t->RenderText(text, px, py, my_color, font, ALIGN_LEFT, pw, mode);
    return 0;
}

int LayoutZone::TextPosC(Terminal *t, Flt row, Flt line, const genericChar* text,
                         int my_color, int mode)
{
    FnTrace("LayoutZone::TextPosC()");
    if (line < 0.0 || line >= size_y || text == nullptr)
        return 1;

    if (my_color == COLOR_DEFAULT)
        my_color = t->TextureTextColor(texture[0]);

    int px = x + border + left_margin + (int) (row  * (Flt) font_width);
    int py = y + border + header      + (int) (line * (Flt) font_height);

    t->RenderText(text, px, py, my_color, font, ALIGN_CENTER, 0, mode);
    return 0;
}

int LayoutZone::TextPosR(Terminal *t, Flt row, Flt line, const genericChar* text,
                         int my_color, int mode)
{
    FnTrace("LayoutZone::TextPosR()");
    if (line < 0.0 || line >= size_y || text == nullptr)
        return 1;

    if (my_color == COLOR_DEFAULT)
        my_color = t->TextureTextColor(texture[0]);

    int px = x + border + left_margin + (int) (row  * (Flt) font_width);
    int py = y + border + header      + (int) (line * (Flt) font_height);

    t->RenderText(text, px, py, my_color, font, ALIGN_RIGHT, 0, mode);
    return 0;
}

int LayoutZone::TextLR(Terminal *t, Flt line,
                       const genericChar* l_text, int l_color, const genericChar* r_text, int r_color)
{
    FnTrace("LayoutZone::TextLR()");
    if (line < 0.0 || line >= size_y)
        return 1;  // Can't draw text

    int l_len = 0, l_width = 0, r_len = 0, r_width = 0;
    if (l_text)
    {
        l_len   = strlen(l_text);
        l_width = t->TextWidth(l_text, l_len, font);
    }
    if (r_text)
    {
        r_len   = strlen(r_text);
        r_width = t->TextWidth(r_text, r_len, font);
    }

    int lw = w - (border * 2) - left_margin - right_margin;

    if (l_text)
    {
        if (l_color == COLOR_DEFAULT)
            l_color = t->TextureTextColor(texture[0]);

        int px = x + border + left_margin;
        int py = y + border + (int) (line * (Flt) font_height);
        t->RenderTextLen(l_text, l_len, px, py, l_color, font, ALIGN_LEFT,
                         lw - r_width - font_width);
    }

    if (r_text)
    {
        if (r_color == COLOR_DEFAULT)
            r_color = t->TextureTextColor(texture[0]);

        int px = x + w - border - right_margin;
        int py = y + border + (int) (line * (Flt) font_height);
        t->RenderTextLen(r_text, r_len, px, py, r_color, font, ALIGN_RIGHT, lw);
    }
    return 0;
}

int LayoutZone::Line(Terminal *t, Flt line, int my_color)
{
    FnTrace("LayoutZone::Line()");
    if (line < 0.0 || line >= size_y)
        return 1;  // Can't draw line

    int lw = w - (border * 2) + 3 - left_margin - right_margin;
    if (lw <= 0)
        return 1;  // zone not wide enough

    if (my_color == COLOR_DEFAULT)
        my_color = t->TextureTextColor(texture[0]);

    int x1 = x + border - 2 + left_margin;
    int y1 = y + border + header + (int) (line * (Flt) font_height) +
        (font_height / 2) - 1;

    t->RenderHLine(x1, y1, lw, my_color);
    return 0;
}

int LayoutZone::LinePos(Terminal *t, Flt row, Flt line, Flt len, int my_color)
{
    FnTrace("LayoutZone::LinePos()");
    return 1;
}

int LayoutZone::Entry(Terminal *t, Flt px, Flt py, Flt len, RegionInfo *place)
{
    FnTrace("LayoutZone::Entry()");
    if (px >= size_x || py >= size_y)
        return 1;

    int xx = (int) (px * (Flt) font_width);
    int sx = x + border - 2 + left_margin + xx;
    int sy = y + border - 2 + header + (int) (py * (Flt) font_height);
    int w1 = (int) (len * (Flt) font_width) + 6;
    int w2 = w - xx - (border * 2) - left_margin - right_margin;
    int sw = Min(w1, w2);
    int sh = font_height + 5;
    if (sw <= 0)
        return 1;

    int tex = texture[0], ft;
    if (tex == IMAGE_DEFAULT)
        tex = t->page->default_texture[0];
    if (tex == IMAGE_CLEAR)
        tex = t->page->image;

    switch(tex)
    {
    case IMAGE_LITE_WOOD:
    case IMAGE_WOOD:
    case IMAGE_GRAY_PARCHMENT:
        ft = IMAGE_DARK_WOOD; break;
    case IMAGE_DARK_SAND:
        ft = IMAGE_SAND; break;
    case IMAGE_DARK_WOOD:
        ft = IMAGE_WOOD; break;
    default:
        ft = IMAGE_DARK_SAND; break;
    }

    t->RenderFilledFrame(sx, sy, sw, sh, 2, ft, FRAME_INSET | FRAME_2COLOR);

    if (place != nullptr)
    {
        place->x = sx;
        place->y = sy;
        place->w = sw;
        place->h = sh;
    }

    return 0;
}

int LayoutZone::Button(Terminal *t, Flt px, Flt py, Flt len, int lit)
{
    FnTrace("LayoutZone::Button()");
    if (px >= size_x || py >= size_y)
        return 1;

    int xx = (int) (px * (Flt) font_width);
    int sx = x + border - 3 + left_margin + xx;
    int sy = y + border - 4 + header + (int) (py * (Flt) font_height);
    int w1 = (int) (len * (Flt) font_width) + 8;
    int w2 = w - xx - (border * 2) - left_margin - right_margin;
    int sw = Min(w1, w2);
    int sh = font_height + 8;
    if (sw <= 0)
        return 1;

    if (lit)
        t->RenderFilledFrame(sx, sy, sw, sh, 2, IMAGE_LIT_SAND, FRAME_LIT);
    else
        t->RenderFilledFrame(sx, sy, sw, sh, 2, IMAGE_SAND);
    return 0;
}

int LayoutZone::Background(Terminal *t, Flt line, Flt height, int my_texture)
{
    FnTrace("LayoutZone::Background()");
    if (line >= size_y)
        return 1;

    int sx = x + border - 3 + left_margin;
    int sy = y + border - 1 + header + (int) ((line * (Flt) font_height) + .5);
    int sw = w - (border * 2) + 6 - left_margin - right_margin;
    int sh = (int) ((height * (Flt) font_height) + .5);

    if (sx < x)
        sx = x;
    if (sw > w)
        sw = w;
    t->RenderRectangle(sx, sy, sw, sh, my_texture);
    return 0;
}

int LayoutZone::Raised(Terminal *t, Flt line, Flt height)
{
    FnTrace("LayoutZone::Raised()");
    if (line >= size_y)
        return 1;

    int sx = x + border - 3 + left_margin;
    int sy = y + border - 1 + header + (int) ((line * (Flt) font_height) + .5);
    int sw = w - (border * 2) + 6 - left_margin - right_margin;
    int sh = (int) ((height * (Flt) font_height) + .5);

    if (sx < x)
        sx = x;
    if (sw > w)
        sw = w;
    t->RenderButton(sx, sy, sw, sh, ZF_RAISED, IMAGE_SAND);
    return 0;
}

int LayoutZone::Underline(Terminal *t, Flt px, Flt py, Flt len, int my_color)
{
    FnTrace("LayoutZone::Underline()");
    int xx = x + border + left_margin + (int) (px * (Flt) font_width);
    int yy = y + border + header + (int) ((py + .95) * (Flt) font_height);
    int ll = (int) (len * (Flt) font_width);

    if (my_color == COLOR_DEFAULT)
        my_color = t->TextureTextColor(texture[0]);

    t->RenderHLine(xx, yy, ll, my_color);
    return 0;
}

int LayoutZone::ColumnSpacing(Terminal *term, int num_columns)
{
    FnTrace("LayoutZone::ColumnSpacing()");

    if (num_columns > 0)
    {
        term->FontSize(font, font_width, font_height);
        num_spaces = w / font_width / num_columns;
    }

    return num_spaces;
}

int LayoutZone::Width(Terminal *term)
{
    FnTrace("LayoutZone::Width()");
    term->FontSize(font, font_width, font_height);
    return w/font_width;
}
