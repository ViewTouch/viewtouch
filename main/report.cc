/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  
  
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
 * report.cc - revision 80 (10/13/98)
 * Implementation of report classes
 */

#include "report.hh"
#include "terminal.hh"
#include "layout_zone.hh"
#include "printer.hh"
#include "employee.hh"
#include "settings.hh"
#include "image_data.hh"
#include "system.hh"
#include "manager.hh"
#include "utility.hh"
#include <cstring>
#include <errno.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** ReportEntry Class ****/
// Constructor
ReportEntry::ReportEntry(const char *t, int c, int a, int m)
{
    FnTrace("ReportEntry::ReportEntry()");
    if (t)
    {
        text = new char[strlen(t)+1];
        strcpy(text, t);
    }
    else
        text = NULL; // draw a line

    next      = NULL;
    fore      = NULL;
    color     = c;
    align     = a;
    new_lines = 0;
    pos       = 0.0;
    edge      = a;
    mode      = m;
    max_len   = 256;
}


/**** Report Class ****/
// Constructor
Report::Report()
{
    current_mode  = 0;
    word_wrap     = 0;
    page          = 0;
    max_pages     = 0;
    lines_shown   = 0;
    max_width     = 80;
    min_width     = 0;
    header        = 0.0;
    footer        = 0.0;
    selected_line = -1;
    add_where     = 0;
    is_complete   = 1;
    update_flag   = 0;
    report_title.Set("");
    have_title    = 0;
    destination   = RP_DEST_EITHER;
    page_width    = 0;
}

// Member Functions
int Report::Clear()
{
    Purge();
    current_mode  = 0;
    word_wrap     = 0;
    page          = 0;
    max_pages     = 0;
    lines_shown   = 0;
    max_width     = 80;
    min_width     = 0;
    header        = 0;
    footer        = 0;
    selected_line = -1;
    add_where     = 0;
    is_complete   = 1;
    update_flag   = 0;
    report_title.Set("");
    have_title    = 0;
    destination   = RP_DEST_EITHER;
    page_width    = 0;

    return 0;
}

int Report::SetTitle(const genericChar *title)
{
    int retval = have_title;  // return previous value of have_title
    report_title.Set(title);
    have_title = 1;
    return retval;
}

int Report::Load(const char *textfile, int color)
{
    FnTrace("Report::Load()");
    char buffer[STRLENGTH];

    if (textfile == NULL)
        return 1;

    FILE *fp = fopen(textfile, "r");
    if (fp == NULL)
    {
        snprintf(buffer, STRLENGTH, "Report::Load Error %d opening %s",
                 errno, textfile);
        ReportError(buffer);
        return 1;
    }

    int i = 0;
    genericChar str[256];
    for (;;)
    {
        genericChar c = (char) fgetc(fp);
        if (feof(fp))
            break;

        if (c != '\n')
        {
            str[i] = c;
            ++i;
        }

        if (c == '\n' || i >= 255)
        {
            if (i > 0)
            {
                str[i] = '\0';
                if (strstr(str, "<b>") == str)
                {
                    Mode(PRINT_BOLD);
                    TextL(&str[3], color);
                    Mode(0);
                }
                else if (strstr(str, "<h>") == str)
                {
                    Mode(PRINT_BOLD);
                    TextC(&str[3], color);
                    Mode(0);
                }
                else
                    TextL(str, color);
                i = 0;
            }
            NewLine();
        }
    }

    if (i > 0)
    {
        str[i] = '\0';
        if (strstr(str, "<b>") == str)
        {
            Mode(PRINT_BOLD);
            TextL(&str[3], color);
            Mode(0);
        }
        else
            TextL(str, color);
    }

    fclose(fp);
    return 0;
}

int Report::Add(ReportEntry *re)
{
    if (re == NULL)
        return 1;

    if (add_where == 1)
    {
        // Add to Header
        header_list.AddToTail(re);
    }
    else
    {
        // Add to Body
        body_list.AddToTail(re);
    }
    return 0;
}

int Report::Purge()
{
    body_list.Purge();
    header_list.Purge();
    return 0;
}

int Report::PurgeHeader()
{
    header_list.Purge();
    return 0;
}

int Report::Append(Report *r)
{
    FnTrace("Report::Append()");
    if (r == NULL)
        return 1;

    NewLine();
    while (r->BodyList())
    {
        ReportEntry *ptr = r->BodyList();
        r->RemoveBody(ptr);
        Add(ptr);
    }

    delete r;
    return 0;
}

int Report::CreateHeader(Terminal *term, Printer *p, Employee *e)
{
    FnTrace("Report::CreateHeader()");
    char buffer[STRLENGTH];

    if (e == NULL)
        return 1;

    Settings *s = term->GetSettings();
    PurgeHeader();
    Header();

    genericChar str[256];
    sprintf(str, "%s: %s", term->Translate("Author"), e->system_name.Value());

    if (p == NULL || p->Width() < 80)
    {
        TextL(s->store_name.Value());
        NewLine();
        TextL(str);
        NewLine();
        TextL(term->TimeDate(SystemTime, TD0));
    }
    else
    {
        TextL(s->store_name.Value());
        if (s->store_address2.length > 0)
        {
            snprintf(buffer, STRLENGTH, "%s, %s", s->store_address.Value(),
                     s->store_address2.Value());
        }
        else
            strcpy(buffer, s->store_address.Value());
        TextR(buffer);
        NewLine();
        TextL(str);
        TextR(term->TimeDate(SystemTime, TD0));
    }
    UnderlinePosL(0, 0);
    NewLine(2);
    Body();
    return 0;
}

int Report::Render(Terminal *term, LayoutZone *lz, Flt header_size,
                   Flt footer_size, int p, int print, Flt spacing)
{
    FnTrace("Report::Render()");
    int last_line = 0;
    ReportEntry *re = BodyList();
    while (re)
    {
        if (re->next)
            last_line += re->new_lines;
        else
            ++last_line;
        re = re->next;
    }

    header = 0;
    if (header_size > 0)
        header = header_size + 1;

    footer = 0;
    if (footer_size > 0)
        footer = footer_size + 1;

    lines_shown = (int) ((lz->size_y - (header + footer)) / spacing + .5);
    if ((last_line > lines_shown || print) && footer < 2)
        footer = 2;

    lines_shown = (int) ((lz->size_y - (header + footer)) / spacing + .5);
    if (debug_mode && lines_shown < 1)
    {  //FIX BAK-->Why does lines_shown sometimes hit 0?
        fprintf(stderr, "Report::Render lines_shown = %d, fixing\n", lines_shown);
        if (lz)
        {
            fprintf(stderr, "    Zone: %s\n", lz->name.Value());
            printf("    LZ Y:  %f\n", lz->size_y);
        }
        lines_shown = 1;
    }
    max_pages = 1 + ((last_line - 1) / lines_shown);

    if (p >= max_pages)
        p = max_pages;
    if (p < 0)
    {
        if (lines_shown > 0)
            p = selected_line / lines_shown;
        else
            p = 0;
    }
    page = p;

    int start_line = page * lines_shown;
    int end_line   = start_line + lines_shown - 1;
    if (selected_line >= start_line && selected_line <= end_line)
    {
        Flt rline = header + (Flt) (selected_line - start_line) * spacing;
        lz->Background(term, rline - ((spacing - 1)/2), spacing, IMAGE_LIT_SAND);
    }

    int line = 0;
    re = BodyList();
    while (re && line <= end_line)
    {
        if (line >= start_line)
        {
            Flt xx, rline = header + (Flt) (line - start_line) * spacing;
            switch (re->edge)
            {
            case ALIGN_CENTER:
                xx = (lz->size_x / 2); break;
            case ALIGN_RIGHT:
                xx = lz->size_x - re->pos; break;
            default:
                xx = re->pos; break;
            }

            if (re->text == NULL)
            {
                switch (re->align)
                {
                case ALIGN_CENTER:
                    xx -= (Flt) re->max_len / 2; break;
                case ALIGN_RIGHT:
                    xx -= (Flt) re->max_len; break;
                }
                Flt len = (Flt) re->max_len;
                if (re->max_len <= 0)
                    len = lz->size_x;
                if (re->mode & PRINT_UNDERLINE)
                    lz->Underline(term, xx, rline, len, re->color);
                else
                    lz->Line(term, rline, re->color);
            }
            else
            {
                switch (re->align)
                {
                case ALIGN_LEFT:
                    lz->TextPosL(term, xx, rline, re->text, re->color, re->mode);
                    break;
                case ALIGN_CENTER:
                    lz->TextPosC(term, xx, rline, re->text, re->color, re->mode);
                    break;
                case ALIGN_RIGHT:
                    lz->TextPosR(term, xx, rline, re->text, re->color, re->mode);
                    break;
                }
            }
        }
        line += re->new_lines;
        re = re->next;
    }

    int color = lz->color[0];
    if (header_size > 0)
        lz->Line(term, (Flt) header_size + .1, color);

    if (footer > 0)
    {
        lz->Line(term, lz->size_y -.1 - footer, color);
        Flt tl = lz->size_y - footer + 1;
        if (print)
        {
            if (max_pages > 1)
                lz->TextPosL(term, 1, tl, term->Translate("Touch To Print"), color);
            else
                lz->TextC(term, tl, term->Translate("Touch Report To Print"), color);
        }

        if (max_pages > 1)
        {
            genericChar str[32];
            strcpy(str, term->PageNo(page + 1, max_pages));
            if (print)
                lz->TextPosR(term, lz->size_x - 1, tl, str, color);
            else
                lz->TextC(term, tl, str, color);
        }
    }
    return 0;
}

int Report::Print(Printer *printer)
{
    FnTrace("Report::Print()");
    if (printer == NULL)
        return 1;

    int  max_w = printer->MaxWidth();
    genericChar text[256];
    int  mode[256];
    int i;

    for (i = 0; i < max_w; ++i)
    {
        text[i] = ' ';
        mode[i] = 0;
    }

    if (have_title)
        printer->SetTitle(report_title.Value());
    printer->Start();
    for (int z = 0; z < 2; ++z)
    {
        ReportEntry *re = BodyList();
        if (z == 0)
            re = HeaderList();
        while (re)
        {
            int width = printer->Width(re->mode);
            if (width > max_width)
                width = max_width;

            PrintEntry(re, 0, width, max_w, text, mode);

            if (re->new_lines > 0 || re->next == NULL)
            {
                int max_end = max_w;
                while (max_end > 0 && text[max_end-1] == ' ' &&
                       (mode[max_end-1] & PRINT_UNDERLINE) == 0)
                {
                    --max_end;
                }
                for (i = 0; i < max_end; ++i)
                    printer->Put(text[i], mode[i]);

                int next_mode = 0;
                if (re->next)
                {
                    next_mode = re->next->mode &
                        (PRINT_RED | PRINT_LARGE | PRINT_NARROW);
                    printer->LineFeed(re->new_lines);
                }
                else
                    printer->LineFeed(re->new_lines);

                for (i = 0; i < max_w; ++i)
                {
                    text[i] = ' ';
                    mode[i] = next_mode;
                }
            }
            re = re->next;
        }

        if (z == 0 && HeaderList())
            printer->LineFeed(1);
    }

    printer->End();
    return 0;
}

int Report::FormalPrint(Printer *printer, int columns)
{
    FnTrace("Report::FormalPrint()");
    char buffer[STRLONG] = "";
    int  lang = LANG_PHRASE;

    if (printer == NULL)
        return 1;

    int max_w = printer->MaxWidth();
    int max_h = printer->MaxLines();
    if (max_w < 80 || max_h < 0)
        return Print(printer); // Just do simpler print

    int max_c = max_w / 40;
    if (min_width > 39)
    {
        max_c = max_w / Max(min_width, max_width);
        if (max_c <= 0)
            max_c = 1;
    }

    genericChar str[256];
    genericChar text[max_h][max_w+4];
    int  mode[max_h][max_w+4];

    int header_lines = 0;
    ReportEntry *re = HeaderList();
    while (re)
    {
        header_lines += re->new_lines;
        re = re->next;
    }

    int body_start = 2 + header_lines + 1;
    int max_lines  = max_h;
    int col_w      = max_w;
    if (col_w > max_width)
        col_w = max_width;

    // Check without footer
    re = BodyList();
    int line = body_start;
    int column = 1;
    int my_page_num = 1;
    while (re)
    {
        line += re->new_lines;
        if (line >= max_lines)
        {
            line = body_start;
            ++column;
            if (column > max_c)
            {
                ++my_page_num;
                break;
            }
            else if (col_w > 39)
                col_w = 39;
        }
        re = re->next;
    }

    if (my_page_num > 1)
    {
        // Check with footer
        max_lines = max_h - 2;
        re = BodyList();
        line = body_start, column = 1, my_page_num = 1;
        while (re)
        {
            line += re->new_lines;
            if (line >= max_lines)
            {
                line = body_start;
                ++column;
                if (column > max_c)
                {
                    ++my_page_num;
                    column = 1;
                }
            }
            re = re->next;
        }
    }

    int total_pages = my_page_num;
    my_page_num = 1;
    if (have_title)
        printer->SetTitle(report_title.Value());
    printer->Start();
    re = BodyList();
    while (re)
    {
        // clear page
        int h;
        int w;
        for (h = 0; h < max_h; ++h)
        {
            for (w = 0; w < max_w; ++w)
            {
                text[h][w] = ' ';
                mode[h][w] = 0;
            }
            text[h][max_w] = '\0';
        }

        line   = 2;
        column = 0;

        // Header
        ReportEntry *he = HeaderList();
        while (he)
        {
            PrintEntry(he, 0, max_w, max_w, text[line], mode[line]);
            if (he->new_lines > 0)
                line += he->new_lines;
            he = he->next;
        }
        line = body_start;

        // Body
        while (re && column < max_c)
        {
            while (re && line < max_lines)
            {
                PrintEntry(re, column * 41, col_w, max_w, text[line], mode[line]);
                if (re->new_lines < 0)
                    line = max_lines;  // end of column
                else if (re->new_lines > 0)
                    line += re->new_lines;
                re = re->next;
            }
            line = body_start;
            ++column;
        }

        // Footer
        if (total_pages > 1)
        {
            int i;

            line = max_h - 2;
            for (i = 0; i < max_w; ++i)
                mode[line][i] = PRINT_UNDERLINE;
            ++line;
            strcpy(str, MasterLocale->Page(my_page_num, total_pages, lang, buffer));
            int len = strlen(str);
            strncpy(&text[line][(max_w-len)/2], str, len);
            ++my_page_num;
        }

        // Print Page
        for (h = 0; h < max_h; ++h)
        {
            int max_end = max_w;
            int i;

            while ((max_end > 0 && text[h][max_end-1] == ' ') &&
                   ((mode[h][max_end-1] & PRINT_UNDERLINE) == 0))
                --max_end;

            for (i = 0; i < max_end; ++i)
                printer->Put(text[h][i], mode[h][i]);

            if (h < (max_h - 1))
            {
                printer->LineFeed(1);
            }
            else if (re)
                printer->FormFeed();
        }
    }

    printer->End();
    return 0;
}

int Report::PrintEntry(ReportEntry *report_entry, int start, int width, int my_max_width,
                       genericChar text[], int mode[])
{
    FnTrace("Report::PrintEntry()");
    if (report_entry == NULL)
        return 1;

    int len;
    int i;
    int c;

    if (report_entry->text == NULL)
        len = report_entry->max_len;
    else
    {
        len = strlen(report_entry->text);
        if (len > report_entry->max_len)
            len = report_entry->max_len;
    }
    if (len > (my_max_width - start))
        len = Max(my_max_width - start, 0);
    if (width > (my_max_width - start))
        width = Max(my_max_width - start, 0);

    int xx = (int) (report_entry->pos + .5);
    switch (report_entry->edge)
    {
    case ALIGN_CENTER:
        xx = (width / 2);
        break;
    case ALIGN_RIGHT:
        xx = width - xx;
        break;
    }

    switch (report_entry->align)
    {
    case ALIGN_CENTER:
        xx -= len / 2;
        break;
    case ALIGN_RIGHT:
        xx -= len;
        break;
    }

    if (xx < 0)
    {
        if (debug_mode)
            printf("Warning: Report::PrintEntry xx < 0:  %d\n", xx);
        xx = 0;
    }

    if (report_entry->text)
    {
        xx += start;
        strncpy(&text[xx], report_entry->text, len);
        if (report_entry->color == COLOR_RED || report_entry->color == COLOR_DK_RED)
            c = PRINT_RED;
        else if (report_entry->color == COLOR_BLUE || report_entry->color == COLOR_DK_BLUE)
            c = PRINT_BLUE;
        else
            c = 0;
        for (i = xx; i < (xx + len); ++i)
            mode[i] = report_entry->mode | c;
    }
    else
    {
        if (len <= 0)
            len = width - xx;
        xx += start;
        for (i = xx; i < (xx + len); ++i)
        {
            if (!(report_entry->mode & PRINT_UNDERLINE))
                text[i]  = '-';
            mode[i] = report_entry->mode;
        }
    }

    return 0;
}

int Report::TouchLine(Flt spacing, Flt ln)
{
    FnTrace("Report::TouchLine()");
    Flt line = (ln - header + ((spacing-1)/2)) / spacing;
    if (line < 0)
        return -1; // header touch
    if (line >= (Flt) lines_shown)
        return -2; // footer touch
    return (int) line + (lines_shown * page);
}

int Report::Divider(char divc, int dwidth)
{
    FnTrace("Report::Divider()");
    int retval = 0;
    char buffer[STRLONG];
    int width = (page_width > 0) ? page_width : max_width;
    char divider = (divc == '\0') ? div_char : divc;

    if (dwidth > 0)
        width = dwidth;
    else
        width = (page_width > 0) ? page_width : max_width;
    memset(buffer, divider, width);
    buffer[width + 1] = '\0';
    TextL(buffer);
    NewLine();

    return retval;
}

int Report::Divider2Col(char divc, int dwidth)
{
    FnTrace("Report::Divider2Col()");
    int  retval = 0;
    char buffer[STRLONG];
    int  width = 0;
    char divider = (divc == '\0') ? div_char : divc;

    if (page_width >= 80)
    {
        if (dwidth > 0)
            width = dwidth;
        else
            width = page_width / 2;
        memset(buffer, divider, width);
        buffer[width] = '\0';
        TextL2Col(buffer);
        NewLine();
    }
    else
        Divider(dwidth);

    return retval;
}

int Report::Mode(int new_mode)
{
    current_mode = new_mode;
    return 0;
}

int Report::Text(const char *text, int c, int align, float indent)
{
    FnTrace("Report::Text()");
    int retval = 0;
    ReportEntry *re = new ReportEntry(text, c, align, current_mode);
    if (indent > 0.01)
    {
        re->pos  = indent;
        re->edge = ALIGN_LEFT;
    }
    else if (indent < -0.01)
    {
        re->pos  = -indent;
        re->edge = ALIGN_RIGHT;
    }
    else
        re->edge = align;
    retval = Add(re);
    return retval;
}

/****
 * Text2Col:  Prints the text across two columns, but requires that
 *   the page be at least 80 columns, for now at least.
 ***/
int Report::Text2Col(const char *text, int color, int align, float indent)
{
    FnTrace("Report::Text2Col()");
    int retval = 0;
    float col_width = (page_width / 2);
    float col2 = col_width + 1;
    int len = strlen(text);
    float pos = 0;

    if (page_width >= 80)
    {
        if (align == ALIGN_CENTER)
        {
            pos = (col_width - len) / 2;
            Text(text, color, ALIGN_LEFT, pos);
            pos += col_width;
            Text(text, color, ALIGN_LEFT, pos);
        }
        else if (align == ALIGN_LEFT)
        {
            Text(text, color, ALIGN_LEFT, indent);
            Text(text, color, ALIGN_LEFT, col2 + indent);
        }
        else
        {
            Text(text, color, ALIGN_RIGHT, indent);
            Text(text, color, ALIGN_RIGHT, col_width + indent);
        }
    }
    else
        retval = Text(text, color, align, indent);

    return retval;
}

int Report::Number(int n, int c, int a, float indent)
{
    FnTrace("Report::Number()");
    genericChar str[32];
    sprintf(str, "%d", n);
    return Text(str, c, a, indent);
}

int Report::Line(int c)
{
    FnTrace("Report::Line()");
    ReportEntry *re = new ReportEntry(NULL, c, ALIGN_LEFT, current_mode);
    return Add(re);
}

int Report::Underline(int len, int c, int a, float indent)
{
    FnTrace("Report::Underline()");
    ReportEntry *re = new ReportEntry(NULL, c, a, PRINT_UNDERLINE);
    if (indent > 0.01)
    {
        re->pos  = indent;
        re->edge = ALIGN_LEFT;
    }
    else if (indent < -0.01)
    {
        re->pos  = -indent;
        re->edge = ALIGN_RIGHT;
    }
    else
        re->edge = a;
    re->max_len = len;
    return Add(re);
}

int Report::NewLine(int nl)
{
    FnTrace("Report::NewLine()");
    if (add_where == 1)
    {
        if (HeaderListEnd() == NULL || HeaderListEnd()->new_lines < 0)
            return 1;
        HeaderListEnd()->new_lines += nl;
    }
    else
    {
        if (BodyListEnd() == NULL || BodyListEnd()->new_lines < 0)
            return 1;
        BodyListEnd()->new_lines += nl;
    }

    return 0;
}

int Report::NewPage()
{
    FnTrace("Report::NewPage()");
    if (BodyListEnd() == NULL)
        return 1;

    BodyListEnd()->new_lines = -1;
    return 0;
}
