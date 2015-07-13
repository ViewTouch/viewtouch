/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  *  All Rights Reserved
 * Confidential and Proprietary Information
 *
 * layer.cc - revision 23 (9/8/98)
 * Implementation of Layer objects
 */

#include <cctype>
#include <iostream>
#include <string.h>

#include "generic_char.hh"
#include "layer.hh"
#include "term_view.hh"
#include "image_data.hh"
#include "remote_link.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** Layer Class ****/
// Constructor
Layer::Layer(Display *d, GC g, Window draw_win, int lw, int lh)
{
    FnTrace("Layer::Layer()");

    int no = DefaultScreen(d);
    dis  = d;
    gfx  = g;
    win  = draw_win;
    pix  = XCreatePixmap(dis, draw_win, lw, lh, DefaultDepth(d, no));
    next = NULL;
    fore = NULL;
    id   = 0;
    offset_x = 0;
    offset_y = 0;
    window_frame = 0;

    max.SetRegion(0, 0, lw, lh);
    SetRegion(0, 0, lw, lh);
    update = 0;
    page_x = 0;
    page_y = 0;
    page_w = lw;
    page_h = lw;
    page_split = 0;
    split_opt = 0;
    frame_width = 2;
    title_color  = COLOR_CLEAR;
    title_height = 32;
    title_mode   = 0;
    bg_texture   = IMAGE_DARK_SAND;
    use_clip = 0;
    cursor = CURSOR_POINTER;
}

// Destructor
Layer::~Layer()
{
    if (pix)
        XFreePixmap(dis, pix);
}

// Member Functions
int Layer::DrawArea(int dx, int dy, int dw, int dh)
{
    FnTrace("Layer::DrawArea()");

    XCopyArea(dis, pix, win, gfx, dx, dy, dw, dh, dx + x, dy + y);
    return 0;
}

int Layer::DrawAll()
{
    FnTrace("Layer::DrawAll()");

    if (split_opt)
    {
        return DrawArea(page_x, page_y, page_w, page_h - page_split);
    }
    else
    {
        return DrawArea(0, 0, w, h);
    }
}

int Layer::BlankPage(int mode, int texture, int tc, int size, int split,
                     int so, const genericChar* title, const genericChar* my_time)
{
    FnTrace("Layer::BlankPage()");

    title_mode  = mode;
    bg_texture  = texture;
    page_split  = split;
    split_opt   = so;
    title_color = tc;
    page_title.Set(title);
    TimeString.Set(my_time);

    switch (size)
    {
    case SIZE_640x480:
        page_w = 640;
        page_h = 480;
        frame_width = 2;
        break;
    case SIZE_800x600:
        page_w = 800;
        page_h = 600;
        frame_width = 2;
        break;
   case SIZE_1024x600:
        page_w = 1024;
        page_h = 600;
        frame_width = 2;
        break;
  case SIZE_1024x768:
        page_w = 1024;
        page_h = 768;
        frame_width = 3;
        break;
    case SIZE_1280x800:
        page_w = 1280;
        page_h = 800;
        frame_width = 3;
        break;
    case SIZE_1280x1024:
        page_w = 1280;
        page_h = 1024;
        frame_width = 3;
        break;
       case SIZE_1366x768:
        page_w = 1366;
        page_h = 768;
        frame_width = 3;
        break;
    case SIZE_1440x900:
        page_w = 1440;
        page_h = 900;
        frame_width = 3;
        break;
    case SIZE_1600x900:
        page_w = 1600;
        page_h = 900;
        frame_width = 4;
        break;
    case SIZE_1680x1050:
        page_w = 1680;
        page_h = 1050;
        frame_width = 4;
        break;
       case SIZE_1920x1080:
        page_w = 1920;
        page_h = 1080;
        frame_width = 4;
        break;
    case SIZE_1920x1200:
        page_w = 1920;
        page_h = 1200;
        frame_width = 4;
        break;
    case SIZE_2560x1440:
        page_w = 2560;
        page_h = 1440;
        frame_width = 4;
        break;
    case SIZE_2560x1600:
        page_w = 2560;
        page_h = 1600;
        frame_width = 4;
        break;
     }
    page_x = Max(w - page_w, 0) / 2 + offset_x;
    page_y = Max(h - page_h, 0) / 2 + offset_y;
    use_clip = 0;

    if (page_split > 0)
    {
        if (title_color == COLOR_CLEAR)
        {
            Rectangle(0, 0, page_w, page_h - page_split, texture);
        }
        else
        {
            Rectangle(0, title_height, page_w, page_h - title_height - page_split,
                      texture);
        }
        if (!so)
        {
            XSetForeground(dis, gfx, ColorTE);
            XFillRectangle(dis, pix, gfx, page_x, page_y + page_h - page_split,
                           page_w, 2);
            Rectangle(0, page_h - page_split + 2, page_w, page_split - 2,
                      IMAGE_DARK_SAND);
        }
    }
    else
    {
        if (title_color == COLOR_CLEAR)
        {
            Rectangle(0, 0, page_w, page_h, texture);
        }
        else
        {
            Rectangle(0, title_height, page_w, page_h - title_height, texture);
        }
    }

    if (page_w < w || page_h < h)
    {
        XSetForeground(dis, gfx, ColorBlack);
        if (page_y > 0)
        {
            XFillRectangle(dis, pix, gfx, 0, 0, w, page_y);
            XFillRectangle(dis, pix, gfx, 0, page_y + page_h, w,
                           h - (page_y + page_h));
        }
        if (page_x > 0)
        {
            XFillRectangle(dis, pix, gfx, 0, page_y, page_x, page_h);
            XFillRectangle(dis, pix, gfx, page_x + page_w, page_y,
                           w - (page_x + page_w), page_h);
        }
    }
    TitleBar();
    return 0;
}

int Layer::Background(int bx, int by, int bw, int bh)
{
    FnTrace("Layer::Background()");

    RegionInfo tr;
    if (page_split > 0)
    {
        if (title_color == COLOR_CLEAR)
        {
            tr.SetRegion(0, 0, page_w, page_h - page_split);
        }
        else
        {
            tr.SetRegion(0, title_height, page_w, page_h - title_height - page_split);
        }

        RegionInfo r(0, page_h - page_split, page_w, 2);
        r.Intersect(bx, by, bw, bh);
        if (r.w > 0 && r.h > 0)
        {
            XSetForeground(dis, gfx, ColorTE);
            XFillRectangle(dis, pix, gfx, page_x + r.x, page_y + r.y, r.w, r.h);
        }

        r.SetRegion(0, page_h - page_split + 2, page_w, page_split - 2);
        r.Intersect(bx, by, bw, bh);
        if (r.w > 0 && r.h > 0)
            Rectangle(r.x, r.y, r.w, r.h, IMAGE_DARK_SAND);
    }
    else
    {
        if (title_color == COLOR_CLEAR)
        {
            tr.SetRegion(0, 0, page_w, page_h);
        }
        else
        {
            tr.SetRegion(0, title_height, page_w, page_h - title_height);
        }
    }

    tr.Intersect(bx, by, bw, bh);
    if (tr.w > 0 && tr.h > 0)
        Rectangle(tr.x, tr.y, tr.w, tr.h, bg_texture);
    if (y < title_height)
        TitleBar();

    if (page_y > 0 || page_x > 0)
    {
        bx += page_x;
        by += page_y;
        XSetForeground(dis, gfx, ColorBlack);

        tr.SetRegion(0, 0, w, page_y);
        tr.Intersect(bx, by, bw, bh);
        if (tr.w > 0 && tr.h > 0)
            XFillRectangle(dis, pix, gfx, tr.x, tr.y, tr.w, tr.h);

        tr.SetRegion(0, page_y + page_h, w, h - (page_y + page_h));
        tr.Intersect(bx, by, bw, bh);
        if (tr.w > 0 && tr.h > 0)
            XFillRectangle(dis, pix, gfx, tr.x, tr.y, tr.w, tr.h);

        tr.SetRegion(0, page_y, page_x, page_h);
        tr.Intersect(bx, by, bw, bh);
        if (tr.w > 0 && tr.h > 0)
            XFillRectangle(dis, pix, gfx, tr.x, tr.y, tr.w, tr.h);

        tr.SetRegion(page_x + page_w, page_y, w - (page_x + page_w), page_h);
        tr.Intersect(bx, by, bw, bh);
        if (tr.w > 0 && tr.h > 0)
            XFillRectangle(dis, pix, gfx, tr.x, tr.y, tr.w, tr.h);
    }
    return 0;
}

int Layer::TitleBar()
{
    FnTrace("Layer::TitleBar()");

    int tc = title_color;
    if (tc != COLOR_CLEAR)
    {
        XSetForeground(dis, gfx, ColorTextH[tc]);
        XFillRectangle(dis, pix, gfx, page_x, page_y, page_w, 2);
        XFillRectangle(dis, pix, gfx, page_x, page_y + 2, 2,
                       title_height - 4);
        XSetForeground(dis, gfx, ColorTextT[tc]);
        XFillRectangle(dis, pix, gfx, page_x + 2, page_y + 2, page_w - 4,
                       title_height - 4);
        XSetForeground(dis, gfx, ColorTextS[tc]);
        XFillRectangle(dis, pix, gfx, page_x, page_y + title_height - 2,
                       page_w, 2);
        XFillRectangle(dis, pix, gfx, page_x + page_w - 2, page_y + 2, 2,
                       title_height - 4);
    }

    int c1 = COLOR_WHITE, c2 = COLOR_YELLOW;
    if (tc == COLOR_WHITE || tc == COLOR_YELLOW)
    {
        c1 = COLOR_BLACK;
        c2 = COLOR_BLUE;
    }
    if (Message.length > 0)
    {
        Text(Message.Value(), Message.length,
             page_w / 2, 4, c1, FONT_TIMES_24, ALIGN_CENTER);
    }
    else
    {
        if (title_mode == MODE_MACRO)
        {
            Text("** RECORDING MACRO **", 21, page_w / 2, 6, c2,
                 FONT_TIMES_20B, ALIGN_CENTER);
        }
        else if (title_mode == MODE_EXPIRED)
        {
            Text("** SOFTWARE EXPIRED **", 22, page_w / 2, 6, c2,
                 FONT_TIMES_20B, ALIGN_CENTER);
        }
        else if (title_mode == MODE_TRAINING)
        {
            Text("** TRAINING MODE **", 19, page_w / 2, 6, c2,
                 FONT_TIMES_20B, ALIGN_CENTER);
        }
        else if (title_mode == MODE_TRANSLATE)
        {
            Text("** TRANSLATION MODE **", 22, page_w / 2, 6, c2,
                 FONT_TIMES_20B, ALIGN_CENTER);
        }
        else if (title_mode == MODE_EDIT)
        {
            Text("** EDIT MODE **", 15, page_w / 2, 6, c2,
                 FONT_TIMES_20B, ALIGN_CENTER);
        }
        else
        {
            Text(StoreName.Value(), StoreName.length, page_w / 2, 6, c2,
                 FONT_TIMES_20B, ALIGN_CENTER);
        }

        Text(page_title.Value(), page_title.length, 20, 6, c1,
             FONT_TIMES_20, ALIGN_LEFT);
        int offset = 20;
        if (IsTermLocal && page_w >= WinWidth)
            offset = 36;
        Text(TimeString.Value(), TimeString.length, page_w - offset, 6, c1,
             FONT_TIMES_20, ALIGN_RIGHT);
    }
    return 0;
}

int Layer::Text(const char* string, int len, int tx, int ty, int c, int font,
                int align, int max_pixel_width)
{
    FnTrace("Layer::Text()");

    int f = font & 31;
    XFontStruct *font_info = GetFontInfo(f);
    if (max_pixel_width > 0)
    {
        int i;
        for (i = len; i > 0; --i)
        {
            if (XTextWidth(font_info, string, i) <= max_pixel_width)
                break;
        }
        len = i;
    }

    if (len <= 0)
    {
        return 1;
    }

    int tw = XTextWidth(font_info, string, len);
    if (align == ALIGN_CENTER)
    {
        tx -= (tw + 1) / 2;
    }
    else if (align == ALIGN_RIGHT)
    {
        tx -= tw;
    }
    tx += page_x;
    ty += page_y + GetFontBaseline(f);

    int ul = 0;
    int yy = ty + 4;
    int xx = tx + tw;
    if (font & FONT_UNDERLINE)
        ul = 1; // print underline

    XSetFont(dis, gfx, font_info->fid);
    XSetForeground(dis, gfx, ColorTextH[c]);
    GenericDrawString(dis, pix, gfx, tx - 1, ty - 1, string, len);
    if (ul)
        XDrawLine(dis, pix, gfx, tx - 1, yy - 1, xx - 1, yy - 1);

    XSetForeground(dis, gfx, ColorTextS[c]);
    GenericDrawString(dis, pix, gfx, tx + 1, ty + 1, string, len);
    if (ul)
        XDrawLine(dis, pix, gfx, tx + 1, yy + 1, xx + 1, yy + 1);

    XSetForeground(dis, gfx, ColorTextT[c]);
    GenericDrawString(dis, pix, gfx, tx, ty, string, len);
    if (ul)
        XDrawLine(dis, pix, gfx, tx, yy, xx, yy);
    return 0;
}

int Layer::ZoneText(const char* str, int tx, int ty, int tw, int th,
                    int color, int font, int align)
{
    FnTrace("Layer::ZoneText()");

    const genericChar* sub_string[64];  // FIX - should allow any number of lines of text
    int   sub_length[64];
    int i;

    int f = font & 31;
    XFontStruct *font_info = GetFontInfo(f);
    int font_h = GetFontHeight(f);
    int max_lines = th / font_h;
    if (max_lines > 63)
        max_lines = 63;

    int line = 0;
    const genericChar* c = str;
    while (line < max_lines)
    {
        if (*c == '\0')
            break;
        int len = 0;
        while (c[len] != '\0' && c[len] != '\\')
            ++len;

        while (line < max_lines)
        {
            sub_string[line] = c;
            if (XTextWidth(font_info, c, len) <= tw)
            {
                sub_length[line] = len;
                c += len;
                while (isspace(*c))
                    ++c;
                if (*c == '\\')
                    ++c;
                while (isspace(*c))
                    ++c;
                ++line;
                break;
            }
            // Find breaks in name that fit
            int lw;
            for (lw = len; lw > 0; --lw)
            {
                if (isspace(c[lw]) && !isspace(c[lw-1]) &&
                    (XTextWidth(font_info, c, lw) <= tw))
                {
                    break;
                }
            }
            if (lw <= 0)
            {
                // Truncate name
                for (lw = len; lw > 1; --lw)
                {
                    if (XTextWidth(font_info, c, lw) <= tw)
                        break;
                }
            }

            sub_length[line] = lw;
            c   += lw;
            len -= lw;
            while (isspace(*c))
                ++c, --len;
            if (*c == '\\')
                ++c, --len;
            while (isspace(*c))
                ++c, --len;
            ++line;
        }
    }

    int sx = tx, sy = ty + ((th - (line * font_h)) / 2);
    if (align == ALIGN_CENTER)
    {
        sx += tw / 2;
    }
    else if (align == ALIGN_RIGHT)
    {
        sx += tw;
    }

    for (i = 0; i < line; ++i)
    {
        if (sub_length[i] > 0)
            Text(sub_string[i], sub_length[i], sx, sy, color, font, align);
        sy += font_h;
    }
    if (*c && line >= max_lines && title_mode == MODE_EDIT)
        Text("!", 1, tx, ty, COLOR_RED, FONT_TIMES_24, ALIGN_LEFT);
    return 0;
}

int Layer::Rectangle(int rx, int ry, int rw, int rh, int image)
{
    FnTrace("Layer::Rectangle()");

    if (image == IMAGE_CLEAR)
        return 0;

    RegionInfo r(rx, ry, rw, rh);
    if (use_clip)
        r.Intersect(clip);

    if (r.w > 0 && r.h > 0)
    {
        XSetTSOrigin(dis, gfx, page_x, page_y);
        XSetTile(dis, gfx, GetTexture(image));
        XSetFillStyle(dis, gfx, FillTiled);
        XFillRectangle(dis, pix, gfx, page_x + r.x, page_y + r.y, r.w, r.h);
        XSetFillStyle(dis, gfx, FillSolid);
    }
    return 0;
}

int Layer::SolidRectangle(int rx, int ry, int rw, int rh, int pixel)
{
    FnTrace("Layer::SolidRectangle()");

    RegionInfo r(rx, ry, rw, rh);
    if (use_clip)
        r.Intersect(clip);

    if (r.w > 0 && r.h > 0)
    {
        XSetForeground(dis, gfx, pixel);
        XFillRectangle(dis, pix, gfx, page_x + r.x, page_y + r.y, r.w, r.h);
    }
    return 0;
}

int Layer::Circle(int cx, int cy, int cw, int ch, int image)
{
    FnTrace("Layer::Circle()");

    if (image == IMAGE_CLEAR)
        return 0;

    XSetTSOrigin(dis, gfx, page_x, page_y);
    XSetTile(dis, gfx, GetTexture(image));
    XSetFillStyle(dis, gfx, FillTiled);
    XFillArc(dis, pix, gfx, page_x + cx, page_y + cy, cw, ch, 0, 360 * 64);
    XSetFillStyle(dis, gfx, FillSolid);
    return 0;
}

int Layer::Diamond(int dx, int dy, int dw, int dh, int image)
{
    FnTrace("Layer::Diamond()");

    if (image == IMAGE_CLEAR)
        return 0;

    dx += page_x;
    dy += page_y;
    short mid_x = dx + (dw/2), far_x = dx + (dw-1);
    short mid_y = dy + (dh/2), far_y = dy + (dh-1);
    XPoint pts[] = {
        {mid_x,   (short)dy},      {far_x,   (short)(mid_y-1)},
        {far_x,   mid_y},   {mid_x,   far_y},
        {(short)(mid_x-1), far_y},   {(short)dx,      mid_y},
        {(short)dx,      (short)(mid_y-1)}, {(short)(mid_x-1), (short)dy}};

    XSetTSOrigin(dis, gfx, page_x, page_y);
    XSetTile(dis, gfx, GetTexture(image));
    XSetFillStyle(dis, gfx, FillTiled);
    XFillPolygon(dis, pix, gfx, pts, 8, Convex, CoordModeOrigin);
    XSetFillStyle(dis, gfx, FillSolid);
    return 0;
}

int Layer::Shape(int sx, int sy, int sw, int sh, int image, int shape)
{
    FnTrace("Layer::Shape()");

    if (sw <= 0 || sh <= 0)
        return 0;
    else if (shape == SHAPE_CIRCLE)
        return Circle(sx-1, sy-1, sw+2, sh+2, image);
    else if (shape == SHAPE_DIAMOND)
        return Diamond(sx, sy, sw, sh, image);
    else
        return Rectangle(sx, sy, sw, sh, image);
}

int Layer::Edge(int ex, int ey, int ew, int eh, int thick, int image)
{
    FnTrace("Layer::Edge()");

    if (image == IMAGE_CLEAR)
        return 0;

    if (ew <= 0 || eh <= 0)
        return 1;

    RegionInfo r;
    int h2 = eh - (thick * 2);
    XSetTSOrigin(dis, gfx, page_x, page_y);
    XSetTile(dis, gfx, GetTexture(image));
    XSetFillStyle(dis, gfx, FillTiled);

    r.SetRegion(ex, ey, ew, thick);
    if (use_clip)
        r.Intersect(clip);
    if (r.w > 0 && r.h > 0)
        XFillRectangle(dis, pix, gfx, page_x + r.x, page_y + r.y, r.w, r.h);

    r.SetRegion(ex, ey + eh - thick, ew, thick);
    if (use_clip)
        r.Intersect(clip);
    if (r.w > 0 && r.h > 0)
        XFillRectangle(dis, pix, gfx, page_x + r.x, page_y + r.y, r.w, r.h);

    r.SetRegion(ex, ey + thick, thick, h2);
    if (use_clip)
        r.Intersect(clip);
    if (r.w > 0 && r.h > 0)
        XFillRectangle(dis, pix, gfx, r.x + page_x, r.y + page_y, r.w, r.h);

    r.SetRegion(ex + ew - thick, ey + thick, thick, h2);
    if (use_clip)
        r.Intersect(clip);
    if (r.w > 0 && r.h > 0)
        XFillRectangle(dis, pix, gfx, page_x + r.x, page_y + r.y, r.w, r.h);

    XSetFillStyle(dis, gfx, FillSolid);
    return 0;
}

int Layer::Edge(int ex, int ey, int ew, int eh, int thick, int image, int shape)
{
    FnTrace("Layer::Edge(w/shape)");

    if (shape == SHAPE_DIAMOND)
        return Diamond(ex, ey, ew, eh, image);
    else if (shape == SHAPE_CIRCLE)
        return Circle(ex, ey, ew, eh, image);
    else
        return Edge(ex, ey, ew, eh, thick, image);
}

int Layer::Frame(int fx, int fy, int fw, int fh, int thick, int flags)
{
    FnTrace("Layer::Frame()");

    int i;

    fx += page_x;
    fy += page_y;
    int t, b, l, r;
    if (flags & FRAME_LIT)
    {
        t = ColorLTE; b = ColorLBE;
        l = ColorLLE; r = ColorLRE;
    }
    else if (flags & FRAME_DARK)
    {
        t = ColorDTE; b = ColorDBE;
        l = ColorDLE; r = ColorDRE;
    }
    else
    {
        t = ColorTE; b = ColorBE;
        l = ColorLE; r = ColorRE;
    }

    if (flags & FRAME_INSET)
    {
        int tmp;
        tmp = t; t = b; b = tmp;
        tmp = l; l = r; r = tmp;
    }
    if (flags & FRAME_2COLOR)
    {
        // go from 4 border colors to 2
        t = l; b = r;
    }

    int shape = flags & 7;
    if (shape == SHAPE_CIRCLE)
    {
        int offset = thick / 2;
        int cx = fx + offset;
        int cy = fy + offset;
        int cw = fw - (offset * 2) - 1;
        int ch = fh - (offset * 2) - 1;

        XSetLineAttributes(dis, gfx, 3, LineSolid, CapProjecting, JoinMiter);
        XSetForeground(dis, gfx, r);
        XDrawArc(dis, pix, gfx, cx, cy, cw, ch, 320 * 64, 80 * 64);
        XDrawArc(dis, pix, gfx, cx, cy, cw, ch, 220 * 64, 20 * 64);
        XSetForeground(dis, gfx, t);
        XDrawArc(dis, pix, gfx, cx, cy, cw, ch,  60 * 64, 80 * 64);
        XSetForeground(dis, gfx, l);
        XDrawArc(dis, pix, gfx, cx, cy, cw, ch, 140 * 64, 80 * 64);
        XDrawArc(dis, pix, gfx, cx, cy, cw, ch,  40 * 64, 20 * 64);
        XSetForeground(dis, gfx, b);
        XDrawArc(dis, pix, gfx, cx, cy, cw, ch, 240 * 64, 80 * 64);
        XSetLineAttributes(dis, gfx, 1, LineSolid, CapProjecting, JoinMiter);
    }
    else if (shape == SHAPE_DIAMOND)
    {
        XPoint pts[4];
        int mid_x = fx + (fw / 2), far_x = fx + fw - 1;
        int mid_y = fy + (fh / 2), far_y = fy + fh - 1;

        pts[0].x = fx;
        pts[0].y = mid_y;
        pts[1].x = fx + thick;
        pts[1].y = mid_y;
        pts[2].x = mid_x - 1;
        pts[2].y = far_y - thick;
        pts[3].x = mid_x - 1;
        pts[3].y = far_y;
        XSetForeground(dis, gfx, l);
        XFillPolygon(dis, pix, gfx, pts, 4, Convex, CoordModeOrigin);

        pts[0].x = fx;
        pts[0].y = mid_y;
        pts[1].x = mid_x - 1;
        pts[1].y = fy;
        pts[2].x = mid_x - 1;
        pts[2].y = fy + thick;
        pts[3].x = fx + thick;
        pts[3].y = mid_y;
        XSetForeground(dis, gfx, t);
        XFillPolygon(dis, pix, gfx, pts, 4, Convex, CoordModeOrigin);

        pts[0].x = mid_x;
        pts[0].y = fy;
        pts[1].x = far_x;
        pts[1].y = mid_y;
        pts[2].x = far_x - thick;
        pts[2].y = mid_y;
        pts[3].x = mid_x;
        pts[3].y = fy + thick;
        XSetForeground(dis, gfx, r);
        XFillPolygon(dis, pix, gfx, pts, 4, Convex, CoordModeOrigin);

        pts[0].x = mid_x;
        pts[0].y = far_y;
        pts[1].x = mid_x;
        pts[1].y = far_y - thick;
        pts[2].x = far_x - thick;
        pts[2].y = mid_y;
        pts[3].x = far_x;
        pts[3].y = mid_y;
        XSetForeground(dis, gfx, b);
        XFillPolygon(dis, pix, gfx, pts, 4, Convex, CoordModeOrigin);
    }
    else
    {
        // Rectangle Frame
        RegionInfo rg;
        rg.SetRegion(fx, fy, thick, fh);
        if (rg.w > 0 && rg.h > 0)
        {
            XSetForeground(dis, gfx, l);
            XFillRectangle(dis, pix, gfx, rg.x, rg.y, rg.w, rg.h);
        }

        rg.SetRegion(fx + fw - thick, fy, thick, fh);
        if (rg.w > 0 && rg.h > 0)
        {
            XSetForeground(dis, gfx, r);
            XFillRectangle(dis, pix, gfx, rg.x, rg.y, rg.w, rg.h);
        }

        XSetForeground(dis, gfx, t);
        for (i = 0; i < thick; ++i)
        {
            int yy = fy + i;
            int x1 = fx + i;
            int x2 = fx + fw - i - 2;
            if (x2 >= x1)
                XDrawLine(dis, pix, gfx, x1, yy, x2, yy);
        }

        XSetForeground(dis, gfx, b);
        for (i = 0; i < thick; ++i)
        {
            int yy = fy + fh - i - 1;
            int x1 = fx + i;
            int x2 = fx + fw - i - 2;
            if (x2 >= x1)
                XDrawLine(dis, pix, gfx, x1, yy, x2, yy);
        }
    }
    return 0;
}

int Layer::FilledFrame(int fx, int fy, int fw, int fh, int ww,
                       int texture, int flags)
{
    FnTrace("Layer::FilledFrame()");

    int ww2 = ww * 2;
    Shape(fx + ww, fy + ww, fw - ww2, fh - ww2, texture, flags & 7);
    Frame(fx, fy, fw, fh, ww, flags);
    return 0;
}

int Layer::StatusBar(int sx, int sy, int sw, int sh, int bar_color,
                     const genericChar* text, int font, int text_color)
{
    FnTrace("Layer::StatusBar()");

    Frame(sx, sy, sw, sh, 2, FRAME_2COLOR);
    XSetForeground(dis, gfx, ColorTextT[bar_color]);
    XFillRectangle(dis, pix, gfx, page_x + sx + 2,
                   page_y + sy + 2, sw - 4, sh - 4);
    int len = strlen(text);
    if (len > 0)
        Text(text, len, x + sx + (sw / 2), y + sy + ((sh - GetFontHeight(font) + 1) / 2),
             text_color, font, ALIGN_CENTER);
    return 0;
}

int Layer::HLine(int lx, int ly, int len, int ww, int color)
{
    FnTrace("Layer::HLine()");

    lx += page_x;
    ly += page_y;

    int w1 = ww / 2, w2 = ww - w1;
    XSetForeground(dis, gfx, ColorTextH[color]);
    XDrawLine(dis, pix, gfx, lx, ly - w1 - 1, lx + len - 1, ly - w1 - 1);

    XSetForeground(dis, gfx, ColorTextT[color]);
    XFillRectangle(dis, pix, gfx, lx, ly - w1, len, ww);

    XSetForeground(dis, gfx, ColorTextS[color]);
    XDrawLine(dis, pix, gfx, lx, ly + w2, lx + len - 1, ly + w2);
    return 0;
}

int Layer::VLine(int lx, int ly, int len, int ww, int color)
{
    FnTrace("Layer::VLine()");

    lx += page_x;
    ly += page_y;

    int w1 = ww / 2, w2 = ww - w1;
    XSetForeground(dis, gfx, ColorTextH[color]);
    XDrawLine(dis, pix, gfx, lx - w1 - 1, ly, lx - w1 - 1, ly + len - 1);

    XSetForeground(dis, gfx, ColorTextT[color]);
    XFillRectangle(dis, pix, gfx, lx - w1, ly, ww, len);

    XSetForeground(dis, gfx, ColorTextS[color]);
    XDrawLine(dis, pix, gfx, lx + w2, ly, lx + w2, ly + len - 1);
    return 0;
}

int Layer::EditCursor(int ex, int ey, int ew, int eh)
{
    FnTrace("Layer::EditCursor()");

    ex += page_x;
    ey += page_y;

    XSetForeground(dis, gfx, ColorBlack);
    XDrawRectangle(dis, pix, gfx, ex, ey, ew - 1, eh - 1);
    XDrawLine(dis, pix, gfx, ex, ey, ex + ew - 1, ey + eh - 1);
    XDrawLine(dis, pix, gfx, ex, ey + eh - 1, ex + ew - 1, ey);
    return 0;
}

int Layer::Shadow(int sx, int sy, int sw, int sh, int size, int shape)
{
    FnTrace("Layer::Shadow()");

    XSetForeground(dis, gfx, ColorBlack);
    XSetFillStyle(dis, gfx, FillStippled);
    RegionInfo r;
    switch (shape)
    {
    case SHAPE_DIAMOND:
    {
        short dx = page_x + sx + size, dy = page_y + sy + size;
        short mid_x = dx + (sw / 2), far_x = dx + (sw - 1);
        short mid_y = dy + (sh / 2), far_y = dy + (sh - 1);
        XPoint pts[] = {
            {mid_x,   dy},      {far_x,   (short)(mid_y-1)},
            {far_x,   mid_y},   {mid_x,   far_y},
            {(short)(mid_x-1), far_y},   {dx,      mid_y},
            {dx,      (short)(mid_y-1)}, {(short)(mid_x-1), dy}};
        XFillPolygon(dis, pix, gfx, pts, 8, Convex, CoordModeOrigin);
        XSetFillStyle(dis, gfx, FillSolid);
    }
    break;
    case SHAPE_CIRCLE:
        XFillArc(dis, pix, gfx, page_x + sx + size,
                 page_y + sy + size, sw, sh, 0, 360 * 64);
        XSetFillStyle(dis, gfx, FillSolid);
        break;
    default:
        r.SetRegion(sx + sw, sy + size, size, sh);
        if (use_clip)
            r.Intersect(clip);
        if (r.w > 0 && r.h > 0)
            XFillRectangle(dis, pix, gfx, r.x + page_x, r.y + page_y, r.w, r.h);

        r.SetRegion(sx + size, sy + sh, sw - size, size);
        if (use_clip)
            r.Intersect(clip);
        if (r.w > 0 && r.h > 0)
            XFillRectangle(dis, pix, gfx, r.x + page_x, r.y + page_y, r.w, r.h);
        XSetFillStyle(dis, gfx, FillSolid);

        break;
    }
    return 0;
}

int Layer::Ghost(int gx, int gy, int gw, int gh)
{
    FnTrace("Layer::Ghost()");

    RegionInfo r(gx, gy, gw, gh);
    if (use_clip)
        r.Intersect(clip);
    if (r.w > 0 && r.h > 0)
    {
        XSetForeground(dis, gfx, ColorBlack);
        XSetFillStyle(dis, gfx, FillStippled);
        XFillRectangle(dis, pix, gfx, r.x + page_x, r.y + page_y, r.w, r.h);
        XSetFillStyle(dis, gfx, FillSolid);
    }
    return 0;
}

int Layer::Zone(int zx, int zy, int zw, int zh,
                int zone_frame, int texture, int shape)
{
    FnTrace("Layer::Zone()");

    int frame;
    switch (zone_frame)
    {
    case ZF_RAISED1:
    case ZF_INSET1:
    case ZF_DOUBLE1:
        frame = 0; break;
    case ZF_RAISED2:
    case ZF_INSET2:
    case ZF_DOUBLE2:
        frame = FRAME_LIT; break;
    case ZF_RAISED3:
    case ZF_INSET3:
    case ZF_DOUBLE3:
        frame = FRAME_DARK; break;
    default:
        switch (texture)
        {
        case IMAGE_LIT_SAND:  frame = FRAME_LIT; break;
        case IMAGE_DARK_WOOD: frame = FRAME_DARK; break;
        default:              frame = 0; break;
        }
        break;
    }

    int b = frame_width, b2 = b * 2;
    switch (zone_frame)
    {
    default:
    case ZF_DEFAULT:
    case ZF_NONE:
        Shape(zx, zy, zw, zh, texture, shape);
        break;
    case ZF_RAISED:
    case ZF_RAISED1:
    case ZF_RAISED2:
    case ZF_RAISED3:
        Shape(zx + b, zy + b, zw - b2, zh - b2, texture, shape);
        Frame(zx, zy, zw, zh, b, shape | frame);
        break;
    case ZF_INSET:
    case ZF_INSET1:
    case ZF_INSET2:
    case ZF_INSET3:
        Shape(zx + b, zy + b, zw - b2, zh - b2, texture, shape);
        Frame(zx, zy, zw, zh, b, shape | FRAME_INSET | frame);
        break;
    case ZF_DOUBLE:
    case ZF_DOUBLE1:
    case ZF_DOUBLE2:
    case ZF_DOUBLE3:
        Shape(zx + b, zy + b, zw - b2, zh - b2, texture, shape);
        Frame(zx, zy, zw, zh, b, shape | frame);
        Frame(zx + b2, zy + b2, zw - b*4, zh - b*4, b, shape | frame);
        break;
    case ZF_BORDER:
        Shape(zx + b, zy + b, zw - b2, zh - b2, texture, shape);
        Frame(zx, zy, zw, zh, b, shape);
        Frame(zx + b2, zy + b2, zw - b*4, zh - b*4, b, shape | FRAME_INSET);
        break;
    case ZF_CLEAR_BORDER:
        Shape(zx + b*3, zy + b*3, zw - b*6, zh - b*6, texture, shape);
        Frame(zx, zy, zw, zh, b, shape);
        Frame(zx + b2, zy + b2, zw - b*4, zh - b*4, b, shape | FRAME_INSET);
        break;
    case ZF_SAND_BORDER:
        Edge(zx + b, zy + b, zw - b2, zh - b2, b, IMAGE_SAND, shape);
        Shape(zx + b*3, zy + b*3, zw - b*6, zh - b*6, texture, shape);
        Frame(zx, zy, zw, zh, b, shape);
        Frame(zx + b2, zy + b2, zw - b*4, zh - b*4, b, shape | FRAME_INSET);
        break;
    case ZF_LIT_SAND_BORDER:
        Edge(zx + b, zy + b, zw - b2, zh - b2, b, IMAGE_LIT_SAND, shape);
        Shape(zx + b*3, zy + b*3, zw - b*6, zh - b*6, texture, shape);
        Frame(zx, zy, zw, zh, b, shape | FRAME_LIT);
        Frame(zx + b2, zy + b2, zw - b*4, zh - b*4, b, shape | FRAME_INSET | FRAME_LIT);
        break;
    case ZF_INSET_BORDER:
        Edge(zx + b, zy + b, zw - b2, zh - b2, b, IMAGE_DARK_SAND, shape);
        Shape(zx + b*3, zy + b*3, zw - b*6, zh - b*6, texture, shape);
        Frame(zx, zy, zw, zh, b, shape | FRAME_INSET);
        Frame(zx + b2, zy + b2, zw - b*4, zh - b*4, b, shape);
        break;
    case ZF_PARCHMENT_BORDER:
        Edge(zx + b, zy + b, zw - b2, zh - b2, b, IMAGE_PARCHMENT, shape);
        Shape(zx + b*3, zy + b*3, zw - b*6, zh - b*6, texture, shape);
        Frame(zx, zy, zw, zh, b, shape);
        Frame(zx + b2, zy + b2, zw - b*4, zh - b*4, b, shape | FRAME_INSET);
        break;
    case ZF_DOUBLE_BORDER:
        Edge(zx + b, zy + b, zw - b2, zh - b2, b*3, IMAGE_SAND, shape);
        Shape(zx + b*5, zy + b*5, zw - b*10, zh - b*10, texture, shape);
        Frame(zx, zy, zw, zh, b, shape);
        Frame(zx + b2, zy + b2, zw - b*4, zh - b*4, b, shape);
        Frame(zx + b*4, zy + b*4, zw - b*8, zh - b*8, b, shape | FRAME_INSET);
        break;
    case ZF_LIT_DOUBLE_BORDER:
        Edge(zx + b, zy + b, zw - b2, zh - b2, b*3, IMAGE_LIT_SAND, shape);
        Shape(zx + b*5, zy + b*5, zw - b*10, zh - b*10, texture, shape);
        Frame(zx, zy, zw, zh, b, shape | FRAME_LIT);
        Frame(zx + b2, zy + b2, zw - b*4, zh - b*4, b, shape | FRAME_LIT);
        Frame(zx + b*4, zy + b*4, zw - b*8, zh - b*8, b, shape | FRAME_INSET | FRAME_LIT);
        break;
    case ZF_HIDDEN:
        return 0;
    }
    return 0;
}

int Layer::FramedWindow(int wx, int wy, int ww, int wh, int color)
{
    FnTrace("Layer::FramedWindow()");

    wx += page_x;
    wy += page_y;
    int far_x = wx + ww - 1;
    int far_y = wy + wh - 1;

    XSetForeground(dis, gfx, ColorTextH[color]);
    XDrawLine(dis, pix, gfx, wx, wy, far_x, wy);
    XDrawLine(dis, pix, gfx, wx, wy + 1, far_x - 1, wy + 1);
    XDrawLine(dis, pix, gfx, wx, wy + 2, wx, far_y - 1);
    XDrawLine(dis, pix, gfx, wx + 1, wy + 2, wx + 1, far_y - 2);
    XDrawLine(dis, pix, gfx, wx + 5, far_y - 5, far_x - 6, far_y - 5);
    XDrawLine(dis, pix, gfx, wx + 6, far_y - 6, far_x - 7, far_y - 6);
    XDrawLine(dis, pix, gfx, far_x - 5, wy + 29, far_x - 5, far_y - 5);
    XDrawLine(dis, pix, gfx, far_x - 6, wy + 30, far_x - 6, far_y - 6);
    XFillRectangle(dis, pix, gfx, wx + 5, wy + 5, ww - 10, 20);

    XSetForeground(dis, gfx, ColorTextS[color]);
    XDrawLine(dis, pix, gfx, far_x, wy + 1, far_x, far_y);
    XDrawLine(dis, pix, gfx, far_x - 1, wy + 2, far_x - 1, far_y - 1);
    XDrawLine(dis, pix, gfx, wx, far_y, far_x - 1, far_y);
    XDrawLine(dis, pix, gfx, wx + 1, far_y - 1, far_x - 1, far_y - 1);
    XDrawLine(dis, pix, gfx, wx + 6, wy + 28, far_x - 5, wy + 28);
    XDrawLine(dis, pix, gfx, wx + 7, wy + 29, far_x - 6, wy + 29);
    XDrawLine(dis, pix, gfx, wx + 5, wy + 28, wx + 5, far_y - 6);
    XDrawLine(dis, pix, gfx, wx + 6, wy + 29, wx + 6, far_y - 7);

    XSetForeground(dis, gfx, ColorTextT[color]);
    XFillRectangle(dis, pix, gfx, wx + 2, wy + 2, ww - 4, 3);
    XFillRectangle(dis, pix, gfx, wx + 2, wy + 25, ww - 4, 3);
    XFillRectangle(dis, pix, gfx, wx + 2, wy + wh - 5, ww - 4, 3);
    XFillRectangle(dis, pix, gfx, wx + 2, wy + 3, 3, wh - 6);
    XFillRectangle(dis, pix, gfx, far_x - 4, wy + 3, 3, wh - 6);
    return 0;
}

int Layer::HGrip(int gx, int gy, int gw, int gh)
{
    FnTrace("Layer::HGrip()");

    int toggle = 0;
    int i;

    for (i = 0; i < gh; ++i)
    {
        if (toggle)
            XSetForeground(dis, gfx, ColorBE);
        else
            XSetForeground(dis, gfx, ColorTE);
        XDrawLine(dis, pix, gfx, gx, gy + i, gx + gw - 1, gy + i);
        toggle ^= 1;
    }
    return 0;
}

int Layer::VGrip(int gx, int gy, int gw, int gh)
{
    FnTrace("Layer::VGrip()");

    int toggle = 0;
    int i;

    for (i = 0; i < gw; ++i)
    {
        if (toggle)
            XSetForeground(dis, gfx, ColorLE);
        else
            XSetForeground(dis, gfx, ColorRE);
        XDrawLine(dis, pix, gfx, gx + i, gy, gx + i, gy + gh - 1);
        toggle ^= 1;
    }
    return 0;
}

int Layer::SetClip(int cx, int cy, int cw, int ch)
{
    FnTrace("Layer::SetClip()");

    XRectangle clip_rec;
    clip_rec.x = cx + page_x;
    clip_rec.y = cy + page_y;
    clip_rec.width = cw;
    clip_rec.height = ch;
    XSetClipRectangles(dis, gfx, 0, 0, &clip_rec, 1, Unsorted);

    use_clip = 1;
    clip.SetRegion(cx, cy, cw, ch);
    return 0;
}

int Layer::ClearClip()
{
    FnTrace("Layer::ClearClip()");

    XSetClipMask(dis, gfx, None);
    use_clip = 0;
    return 0;
}

int Layer::MouseEnter(LayerList *ll)
{
    FnTrace("Layer::MouseEnter()");

    ShowCursor(cursor);
    if (window_frame)
    {
        FramedWindow(0, 0, w, h, ll->active_frame_color);
        ZoneText(window_title.Value(), 5, 6, w - 10, 20, COLOR_BLACK, FONT_TIMES_18);
        update = 1;
        ll->UpdateAll(0);
    }
    return 0;
}

int Layer::MouseExit(LayerList *ll)
{
    FnTrace("Layer::MouseExit()");

    if (window_frame)
    {
        FramedWindow(0, 0, w, h, ll->inactive_frame_color);
        ZoneText(window_title.Value(), 5, 6, w - 10, 20, COLOR_BLACK, FONT_TIMES_18);
        update = 1;
        ll->UpdateAll(0);
    }
    return 0;
}

int Layer::MouseAction(LayerList *ll, int mx, int my, int code)
{
    FnTrace("Layer::MouseAction()");

    if (buttons.MouseAction(ll, this, mx, my, code))
    {
        return 0;
    }

    WInt8(SERVER_MOUSE);
    WInt16(id);
    WInt8(code);
    WInt16(mx - page_x);
    WInt16(my - page_y);
    return SendNow();
}

int Layer::Touch(LayerList *ll, int tx, int ty)
{
    FnTrace("Layer::Touch()");

    WInt8(SERVER_TOUCH);
    WInt16(id);
    WInt16(tx - page_x);
    WInt16(ty - page_y);
    return SendNow();
}

int Layer::Keyboard(LayerList *ll, genericChar key, int code, int state)
{
    FnTrace("Layer::Keyboard()");

    WInt8(SERVER_KEY);
    WInt16(id);
    WInt16(key);
    WInt32(code);
    WInt32(state);
    return SendNow();
}


/**** LayerList Class ****/
// Constructor
LayerList::LayerList()
{
    FnTrace("LayerList::LayerList()");

    dis = NULL;
    gfx = 0;
    win = 0;
    select_on = 0;
    select_x1 = 0;
    select_y1 = 0;
    select_x2 = 0;
    select_y2 = 0;
    drag = NULL;
    drag_x = 0;
    drag_y = 0;
    mouse_x = 0;
    mouse_y = 0;
    screen_blanked = 0;
    active_frame_color = COLOR_DK_RED;
    inactive_frame_color = COLOR_DK_BLUE;
    last_object = NULL;
    last_layer  = NULL;
}

// Member Functions
int LayerList::XWindowInit(Display *d, GC g, Window w)
{
    FnTrace("LayerList::XWindowInit()");

    dis = d;
    gfx = g;
    win = w;
    XSetWindowBackground(dis, win, ColorBlack);
    XClearWindow(dis, win);
    return 0;
}

int LayerList::Add(Layer *l, int update)
{
    FnTrace("LayerList::Add()");

    if (l == NULL)
        return 1;

    list.AddToTail(l);
    if (update)
        l->MouseExit(this);
    return 0;
}

int LayerList::AddInactive(Layer *l)
{
    FnTrace("LayerList::AddInactive()");

    return inactive.AddToTail(l);
}

int LayerList::Remove(Layer *l, int update)
{
    FnTrace("LayerList::Remove()");

    if (l == NULL)
        return 1;

    // check to see if layer was in active list
    int was_active = 0;
    if (update)
    {
        Layer *tmp = list.Head();
        while (tmp)
        {
            if (tmp == l)
            {
                was_active = 1;
                break;
            }    
            tmp = tmp->next;
        }
    }

    if (list.RemoveSafe(l))
        inactive.RemoveSafe(l);

    // update other layers
    if (was_active)
    {
        UpdateArea(l->x, l->y, l->w, l->h);
        if (last_layer == l)
        {
            last_object = NULL;
            last_layer  = FindByPoint(mouse_x, mouse_y);
            if (last_layer)
                last_layer->MouseEnter(this);
        }
    }
    return 0;
}

int LayerList::Purge()
{
    FnTrace("LayerList::Purge()");

    list.Purge();
    inactive.Purge();
    return 0;
}

Layer *LayerList::FindByPoint(int x, int y)
{
    FnTrace("LayerList::FindByPoint()");

    for (Layer *l = list.Tail(); l != NULL; l = l->fore)
    {
        if (l->IsPointIn(x, y))
            return l;
    }

    return NULL;
}

Layer *LayerList::FindByID(int id)
{
    FnTrace("LayerList::FindByID()");

    Layer *l;

    for (l = list.Head(); l != NULL; l = l->next)
        if (l->id == id)
            return l;

    for (l = inactive.Head(); l != NULL; l = l->next)
        if (l->id == id)
            return l;
    return NULL;
}

int LayerList::SetScreenBlanker(int set)
{
    FnTrace("LayerList::SetScreenBlanker()");

    if (set == screen_blanked)
        return 1;
    drag = NULL;
    screen_blanked = set;
    if (set)
        ShowCursor(CURSOR_BLANK);
    else
        ShowCursor(CURSOR_POINTER);
    if (!set)
        UpdateAll();

    // FIX - should handle details of screen blanking (in term_view.cc currently)
    return 0;
}

int LayerList::SetScreenImage(int set)
{
    screen_image = set;

    return 0;
}

int LayerList::UpdateAll(int select_all)
{
    FnTrace("LayerList::UpdateAll()");

    if (screen_blanked)
    {
        XClearWindow(dis, win);
        if (screen_image)
            DrawScreenSaver();
        return 0;
    }
    
    Layer *l = list.Head();
    if (l == NULL)
        return 0;
    
    if (select_all)
        while (l)
        {
            l->update = 1;
            l = l->next;
        }

    l = list.Tail();
    l->DrawArea(0, 0, l->w, l->h);

    Layer *next_layer = l->fore;
    if (next_layer)
    {
        int p0 = l->x;
        int p1 = l->y;
        int p2 = p0 + l->w;
        int p3 = p1 + l->h;
        if (p1 > 0)
            OptimalUpdateArea(0, 0, WinWidth, p1, next_layer);
        if (p0 > 0)
            OptimalUpdateArea(0, p1, p0, l->h, next_layer);
        if (p2 < WinWidth)
            OptimalUpdateArea(p2, p1, WinWidth - p2, l->h, next_layer);
        if (p3 < WinHeight)
            OptimalUpdateArea(0, p3, WinWidth, WinHeight - p3, next_layer);
    }

    for (l = list.Head(); l != NULL; l = l->next)
        l->update = 0;
    return 0;
}

int LayerList::UpdateArea(int ax, int ay, int aw, int ah)
{
    FnTrace("LayerList::UpdateArea()");

    Layer *l;

    if (screen_blanked)
    {
        XClearWindow(dis, win);
        if (screen_image)
            BlankScreen();
        return 0;
    }
    
    for (l = list.Head(); l != NULL; l = l->next)
    {
        if (l->Overlap(ax, ay, aw, ah))
            l->update = 1;
    }

    OptimalUpdateArea(ax, ay, aw, ah);

    for (l = list.Head(); l != NULL; l = l->next)
        l->update = 0;
    return 0;
}

int LayerList::OptimalUpdateArea(int ax, int ay, int aw, int ah, Layer *end)
{
    FnTrace("LayerList::OptimalUpdateArea()");

    if (screen_blanked)
        return 0;

    Layer *l = list.Tail();
    if (end)
        l = end;
    while (l)
    {
        if (l->Overlap(ax, ay, aw, ah))
            break;
        l = l->fore;
    }
    if (l == NULL)
        return 0;

    RegionInfo r;
    if (l->update)
    {
        r.SetRegion(ax, ay, aw, ah);
        r.Intersect(l);
        l->DrawArea(r.x - l->x, r.y - l->y, r.w, r.h);
    }

    Layer *next_layer = l->fore;
    if (next_layer == NULL)
        return 0;

    int p0 = l->x;
    int p1 = l->y;
    int p2 = p0 + l->w;
    int p3 = p1 + l->h;
    if (p1 > 0)
    {
        r.SetRegion(0, 0, WinWidth, p1);
        r.Intersect(ax, ay, aw, ah);
        if (r.w > 0 && r.h > 0)
            OptimalUpdateArea(r.x, r.y, r.w, r.h, next_layer);
    }
    if (p0 > 0)
    {
        r.SetRegion(0, p1, p0, l->h);
        r.Intersect(ax, ay, aw, ah);
        if (r.w > 0 && r.h > 0)
            OptimalUpdateArea(r.x, r.y, r.w, r.h, next_layer);
    }
    if (p2 < WinWidth)
    {
        r.SetRegion(p2, p1, WinWidth - p2, l->h);
        r.Intersect(ax, ay, aw, ah);
        if (r.w > 0 && r.h > 0)
            OptimalUpdateArea(r.x, r.y, r.w, r.h, next_layer);
    }
    if (p3 < WinHeight)
    {
        r.SetRegion(0, p3, WinWidth, WinHeight - p3);
        r.Intersect(ax, ay, aw, ah);
        if (r.w > 0 && r.h > 0)
            OptimalUpdateArea(r.x, r.y, r.w, r.h, next_layer);
    }
    return 0;
}

int LayerList::RubberBandOff()
{
    FnTrace("LayerList::RubberBandOff()");

    if (select_on == 0)
        return 1;

    // Erase rubber band
    int rx = Min(select_x1, select_x2);
    int ry = Min(select_y1, select_y2);
    int rw = Abs(select_x1 - select_x2);
    int rh = Abs(select_y1 - select_y2);
    UpdateArea(rx, ry, rw + 1, 1);
    UpdateArea(rx, ry, 1, rh + 1);
    UpdateArea(rx + rw, ry, 1, rh + 1);
    UpdateArea(rx, ry + rh, rw + 1, 1);
    XFlush(dis);
    select_on = 0;
    return 0;
}

int LayerList::RubberBandUpdate(int ux, int uy)
{
    FnTrace("LayerList::RubberBandUpdate()");

    int rx, ry, rw, rh;
    if (select_on == 0)
    {
        select_on = 1;
        select_x1 = ux;
        select_y1 = uy;
    }
    else
    {
        // Erase rubber band
        rx = Min(select_x1, select_x2);
        ry = Min(select_y1, select_y2);
        rw = Abs(select_x1 - select_x2);
        rh = Abs(select_y1 - select_y2);
        UpdateArea(rx, ry, rw + 1, 1);
        UpdateArea(rx, ry, 1, rh + 1);
        UpdateArea(rx + rw, ry, 1, rh + 1);
        UpdateArea(rx, ry + rh, rw + 1, 1);
    }
    select_x2 = ux;
    select_y2 = uy;
    rx = Min(select_x1, select_x2);
    ry = Min(select_y1, select_y2);
    rw = Abs(select_x1 - select_x2);
    rh = Abs(select_y1 - select_y2);
    XSetForeground(dis, gfx, ColorBlack);
    XDrawRectangle(dis, win, gfx, rx, ry, rw, rh);
    XFlush(dis);
    return 0;
}

int LayerList::MouseAction(int x, int y, int code)
{
    FnTrace("LayerList::MouseAction()");

    mouse_x = x;
    mouse_y = y;
    if (screen_blanked)
        return 1;

    if (!(code & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) ||
        (code & MOUSE_RELEASE))
    {
        drag = NULL;
    }
    if (drag)
    {
        return DragLayer(x, y);
    }

    //NOTE BAK->last_layer would be more appropriately named previous_layer
    if (last_layer && (code & MOUSE_DRAG))
    {
        // Mouse focus stays with last layer (or object) when draging
        if (last_object)
        {
            return last_object->MouseAction(this, last_layer,
                                            x - last_layer->x, y - last_layer->y, code);
        }
        else
        {
            return last_layer->MouseAction(this,
                                           x - last_layer->x, y - last_layer->y, code);
        }
    }

    Layer *l = FindByPoint(x, y);
    if (l == NULL)
    {
        drag        = NULL;
        last_layer  = NULL;
        last_object = NULL;
        return 0;
    }

    if (last_object &&
        (!last_object->IsPointIn(x - last_layer->x, y - last_layer->y) ||
         last_layer != l))
    {
        // Object mouse focus has changed
        last_object->MouseExit(this, last_layer);
        last_object = NULL;
    }

    if (last_layer != l)
    {
        // Layer mouse focus has changed
        if (last_layer)
        {
            last_layer->MouseExit(this);
        }

        l->MouseEnter(this);
        last_layer = l;
    }

    if ((code & MOUSE_PRESS) && l->window_frame & WINFRAME_MOVE)
    {
        RegionInfo r(l->x, l->y, l->w, 30);
        if (r.IsPointIn(x, y))
        {
            drag = l;
            drag_x = x;
            drag_y = y;
            return 0;
        }
    }
    return l->MouseAction(this, x - l->x, y - l->y, code);
}

int LayerList::DragLayer(int x, int y)
{
    FnTrace("LayerList::DragLayer()");

    if (drag == NULL)
        return 1;

    if (x < 0)
        x = 0;
    if (x >= WinWidth)
        x = WinWidth - 1;
    if (y < 0)
        y = 0;
    if (y >= WinHeight)
        y = WinHeight - 1;

    int dx = x - drag_x;
    int dy = y - drag_y;
    if (dx == 0 && dy == 0)
        return 0;

    RegionInfo r(drag);
    drag->x += dx;
    drag->y += dy;
    UpdateArea(drag->x, drag->y, drag->w, drag->h);

    if (dx > drag->w || dy > drag->h)
    {
        // easy case (old and new areas don't over lap)
        UpdateArea(r.x, r.y, r.w, r.h);
    }
    else
    {
        // not so easy case
        if (dx > 0)
        {
            UpdateArea(r.x, r.y, dx, r.h);
            if (dy > 0)
                UpdateArea(r.x + dx, r.y, drag->w, dy);
            else if (dy < 0)
                UpdateArea(r.x + dx, r.y + r.h + dy, drag->w, -dy);
        }
        else if (dx < 0)
        {
            UpdateArea(r.x + r.w + dx, r.y, -dx, r.h);
            if (dy > 0)
                UpdateArea(r.x, r.y, drag->w, dy);
            else if (dy < 0)
                UpdateArea(r.x, r.y + r.h + dy, drag->w, -dy);
        }
        else
        {
            if (dy > 0)
                UpdateArea(r.x, r.y, r.w, dy);
            else if (dy < 0)
                UpdateArea(r.x, r.y + r.h + dy, r.w, -dy);
        }
    }
    XFlush(dis);
    drag_x = x;
    drag_y = y;
    return 0;
}

int LayerList::Touch(int x, int y)
{
    FnTrace("LayerList::Touch()");

    Layer *l = FindByPoint(x, y);
    if (l)
        return l->Touch(this, x - l->x, y - l->y);
    else
        return 0;
}

int LayerList::Keyboard(char key, int code, int state)
{
    FnTrace("LayerList::Keyboard()");

    if (last_layer)
        return last_layer->Keyboard(this, key, code, state);
    else if (list.Head())
        return list.Head()->Keyboard(this, key, code, state);
    ReportError("keyboard input lost");

    return 0;
}

int LayerList::HideCursor()
{
    FnTrace("LayerList::HideCursor()");

#ifndef DEBUG
    XWarpPointer(dis, None, None, 0, 0, 0, 0, WinWidth, WinHeight);
#endif

    return 0;
}

int LayerList::SetCursor(Layer *l, int type)
{
    FnTrace("LayerList::SetCursor()");

    if (type == l->cursor)
        return 0;
    l->cursor = type;
    if (last_layer == l)
        ShowCursor(type);
    return 0;
}

//****************************************************************
// BAK --> It appears the LayerObject classes build the editor
//  interface.  The toolbar window is an LO_SomethingOrOther.
//****************************************************************

/**** LayerObject Class ****/
// Constructor
LayerObject::LayerObject()
{
    FnTrace("LayerObject::LayerObject()");

    next = NULL;
    fore = NULL;
    hilight = 0;
    select = 0;
    id = 0;
}

// Member Functions
int LayerObject::UpdateAll(LayerList *ll, Layer *l)
{
    FnTrace("LayerObject::UpdateAll()");

    l->update = 1;
    ll->OptimalUpdateArea(l->x + x, l->y + y, w, h);
    l->update = 0;
    return 0;
}

int LayerObject::MouseEnter(LayerList *ll, Layer *l)
{
    FnTrace("LayerObject::MouseEnter()");

    hilight = 1;
    Render(l);
    UpdateAll(ll, l);
    return 0;
}

int LayerObject::MouseExit(LayerList *ll, Layer *l)
{
    FnTrace("LayerObject::MouseExit()");

    hilight = 0;
    select = 0;
    Render(l);
    UpdateAll(ll, l);
    return 0;
}

/**** LayerObjectList Class ****/
// Constructor
LayerObjectList::LayerObjectList()
{
    FnTrace("LayerObjectList::LayerObjectList()");
}

// Member Functions
int LayerObjectList::Add(LayerObject *lo)
{
    FnTrace("LayerObjectList::Add()");

    return list.AddToTail(lo);
}

int LayerObjectList::Remove(LayerObject *lo)
{
    FnTrace("LayerObjectList::Remove()");

    return list.Remove(lo);
}

int LayerObjectList::Purge()
{
    FnTrace("LayerObjectList::Purge()");

    list.Purge();
    return 0;
}

LayerObject *LayerObjectList::FindByID(int id)
{
    FnTrace("LayerObjectList::FindByID()");

    for (LayerObject *l = list.Tail(); l != NULL; l = l->fore)
        if (l->id == id)
            return l;
    return NULL;
}

LayerObject *LayerObjectList::FindByPoint(int x, int y)
{
    FnTrace("LayerObjectList::FindByPoint()");

    for (LayerObject *l = list.Tail(); l != NULL; l = l->fore)
    {
        if (l->IsPointIn(x, y))
        {
            return l;
        }
    }
    return NULL;
}

int LayerObjectList::Render(Layer *l)
{
    FnTrace("LayerObjectList::Render()");

    for (LayerObject *lo = list.Head(); lo != NULL; lo = lo->next)
        lo->Render(l);
    return 0;
}

int LayerObjectList::Layout(Layer *l)
{
    FnTrace("LayerObjectList::Layout()");

    for (LayerObject *lo = list.Head(); lo != NULL; lo = lo->next)
        lo->Layout(l);
    return 0;
}

int LayerObjectList::MouseAction(LayerList *ll, Layer *l,
                                 int x, int y, int code)
{
    FnTrace("LayerObjectList::MouseAction()");

    LayerObject *lo = FindByPoint(x, y);
    if (lo)
    {
        //NOTE BAK->The code only reaches this point for the toolbar in edit mode.
        //And it doesn't appear to reach this point if we're in the title bar.
        if (lo != ll->last_object)
        {
            lo->MouseEnter(ll, l);
            ll->last_object = lo;
        }
        return lo->MouseAction(ll, l, x, y, code);
    }
    return 0;
}


/**** LO_PushButton Class ****/
// Constructor
LO_PushButton::LO_PushButton(const char* str, int normal_color, int active_color)
{
    FnTrace("LO_PushButton::LO_PushButton()");

    text.Set(str);
    color[0] = normal_color;
    color[1] = active_color;
    font = FONT_TIMES_14;
}

// Member Functions
int LO_PushButton::Render(Layer *l)
{
    FnTrace("LO_PushButton::Render()");

    if (select)
        l->FilledFrame(x, y, w, h, 2, IMAGE_DARK_SAND, FRAME_INSET);
    else
        l->FilledFrame(x, y, w, h, 2, IMAGE_SAND);

    int c = color[hilight];
    int fw = l->frame_width;
    l->ZoneText(text.Value(), x+fw, y+fw, w-(fw*2), h-(fw*2), c, font);
    return 0;
}

int LO_PushButton::MouseAction(LayerList *ll, Layer *l, int mx, int my, int code)
{
    FnTrace("LO_PushButton::MouseAction()");

    if (code & MOUSE_PRESS)
    {
        select = 1;
    }
    else if (code & MOUSE_RELEASE && select)
    {
        Command(l);
        select = 0;
    }
    else
        return 0;

    Render(l);
    UpdateAll(ll, l);
    return 1;
}

int LO_PushButton::Command(Layer *l)
{
    FnTrace("LO_PushButton::Command()");

    WInt8(SERVER_BUTTONPRESS);
    WInt16(l->id);
    WInt16(id);
    return SendNow();
}

/**** LO_ScrollBar Class ****/
// Constructor
LO_ScrollBar::LO_ScrollBar()
{
    FnTrace("LO_ScrollBar::LO_ScrollBar()");

    bar_x = 0;
    bar_y = 0;
    press_x = 0;
    press_y = 0;
}

// Member Functions
int LO_ScrollBar::Render(Layer *l)
{
    FnTrace("LO_ScrollBar::Render()");

    l->FilledFrame(x, y, w, h, 1, IMAGE_DARK_SAND, FRAME_INSET);
    if (bar.IsSet())
    {
        if (select)
            l->FilledFrame(bar.x, bar.y, bar.w, bar.h, 2, IMAGE_LIT_SAND);
        else
            l->FilledFrame(bar.x, bar.y, bar.w, bar.h, 2, IMAGE_SAND);
        int size = Min(bar.w, bar.h) - 8;
        int center_x = bar.x + (bar.w / 2) - (size / 2);
        int center_y = bar.y + (bar.h / 2) - (size / 2);
        l->Frame(center_x, center_y, size, size, 1);

        if (bar.w < (w - 2))
        {
            l->VGrip(bar.x + 4, bar.y + 3, 6, bar.h - 6);
            l->VGrip(bar.x + bar.w - 10, bar.y + 3, 6, bar.h - 6);
        }
        if (bar.h < (h - 2))
        {
            l->HGrip(bar.x + 3, bar.y + 4, bar.w - 6, 6);
            l->HGrip(bar.x + 3, bar.y + bar.h - 10, bar.w - 6, 6);
        }
    }
    return 0;
}

int LO_ScrollBar::MouseAction(LayerList *ll, Layer *l, int mx, int my, int code)
{
    FnTrace("LO_ScrollBar::MouseAction()");

    if (code & MOUSE_PRESS)
    {
        press_x = mx;
        press_y = my;
        if (bar.IsPointIn(mx, my))
        {
            bar_x = bar.x;
            bar_y = bar.y;
            select = 1;
        }
        else
        {
            if (mx < bar.x)
                bar.x -= bar.w;
            else if (mx > (bar.x + bar.w))
                bar.x += bar.w;
            if (my < bar.y)
                bar.y -= bar.h;
            else if (my > (bar.y + bar.h))
                bar.y += bar.h;
        }
        goto bar_move;
    }

    if (code & MOUSE_DRAG && select)
    {
        bar.x = bar_x + (mx - press_x);
        bar.y = bar_y + (my - press_y);
        goto bar_move;
    }

    if (code & MOUSE_RELEASE && select)
    {
        select = 0;
        goto bar_move;
    }
    return 0;

bar_move:
    if (bar.x < (x + 1))
        bar.x = x + 1;
    if (bar.y < (y + 1))
        bar.y = y + 1;
    if ((bar.x + bar.w) > (x + w - 1))
        bar.x = x + w - 1 - bar.w;
    if ((bar.y + bar.h) > (y + h - 1))
        bar.y = y + h - 1 - bar.h;
    Render(l);
    UpdateAll(ll, l);
    return 1;
}

/**** LO_ItemList Class ****/
// Construtor
LO_ItemList::LO_ItemList()
{
    FnTrace("LO_ItemList::LO_ItemList()");
}

// Member Functions
int LO_ItemList::Render(Layer *l)
{
    FnTrace("LO_ItemList::Render()");

    return 0;
}

int LO_ItemList::MouseAction(LayerList *ll, Layer *l, int mouse_x, int mouse_y, int code)
{
    FnTrace("LO_ItemList::MouseAction()");

    return 0;
}

/**** LO_ItemMenu Class ****/
// Constructor
LO_ItemMenu::LO_ItemMenu()
{
    FnTrace("LO_ItemMenu::LO_ItemMenu()");
}

// Member Functions
int LO_ItemMenu::Render(Layer *l)
{
    FnTrace("LO_ItemMenu::Render()");

    return 0;
}

int LO_ItemMenu::MouseAction(LayerList *ll, Layer *l, int mouse_x, int mouse_y, int code)
{
    FnTrace("LO_ItemMenu::MouseAction()");

    return 0;
}

/**** LO_TextEntry Class ****/
// Constructor
LO_TextEntry::LO_TextEntry()
{
    FnTrace("LO_TextEntry::TextEntry()");
}

// Member Functions
int LO_TextEntry::Render(Layer *l)
{
    FnTrace("LO_TextEntry::Render()");

    return 0;
}

int LO_TextEntry::MouseAction(LayerList *ll, Layer *l, int mouse_x, int mouse_y, int code)
{
    FnTrace("LO_TextEntry::MouseAction()");

    return 0;
}

