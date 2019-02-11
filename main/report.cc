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
#include <vector>
#include <numeric> // std::accumulate

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** ReportEntry Class ****/
// Constructor
ReportEntry::ReportEntry(const std::string &t, int c, int a, int m) :
    text(t),
    color(c),
    align(a),
    edge(a),
    mode(m)
{
    FnTrace("ReportEntry::ReportEntry()");
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
    report_title.clear();
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
    report_title.clear();
    have_title    = 0;
    destination   = RP_DEST_EITHER;
    page_width    = 0;

    return 0;
}

int Report::SetTitle(const std::string &title)
{
    int retval = have_title;  // return previous value of have_title
    report_title = title;
    have_title = 1;
    return retval;
}

int Report::Load(const std::string &textfile, int color)
{
    FnTrace("Report::Load()");
    char buffer[STRLENGTH];

    if (textfile.empty())
        return 1;

    FILE *fp = fopen(textfile.c_str(), "r");
    if (fp == NULL)
    {
        snprintf(buffer, STRLENGTH, "Report::Load Error %d opening %s",
                 errno, textfile.c_str());
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

int Report::Add(const ReportEntry &re)
{
    if (add_where == 1)
    {
        // Add to Header
        header_list.push_back(re);
    }
    else
    {
        // Add to Body
        body_list.push_back(re);
    }
    return 0;
}

int Report::Purge()
{
    body_list.clear();
    header_list.clear();
    return 0;
}

int Report::PurgeHeader()
{
    header_list.clear();
    return 0;
}

int Report::Append(const Report &r)
{
    FnTrace("Report::Append()");

    NewLine();
    body_list.insert(body_list.end(), r.body_list.begin(), r.body_list.end());
    return 0;
}

int Report::CreateHeader(Terminal *term, Printer *p, const Employee *e)
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
        if (s->store_address2.size() > 0)
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
    auto get_line_count = [](const int &sum, const ReportEntry &re) -> int
    {
        return sum + re.new_lines;
    };
    // last entry counts only as one line, don't know why, original code was like this
    int last_line = std::accumulate(body_list.cbegin(), std::prev(body_list.cend()), 1, get_line_count);

    header = (header_size > 0) ? static_cast<float>(header_size + 1) : 0;
    footer = (footer_size > 0) ? static_cast<float>(footer_size + 1) : 0;

    lines_shown = (int) ((lz->size_y - (header + footer)) / spacing + .5);
    if ((last_line > lines_shown || print) && footer < 2)
        footer = 2;

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
    for (ReportEntry &re : body_list)
    {
        if (line > end_line)
        {
            break;
        }
        if (line >= start_line)
        {
            Flt xx, rline = header + (Flt) (line - start_line) * spacing;
            switch (re.edge)
            {
            case ALIGN_CENTER:
                xx = (lz->size_x / 2); break;
            case ALIGN_RIGHT:
                xx = lz->size_x - re.pos; break;
            default:
                xx = re.pos; break;
            }

            if (re.text.empty())
            {
                switch (re.align)
                {
                case ALIGN_CENTER:
                    xx -= (Flt) re.max_len / 2; break;
                case ALIGN_RIGHT:
                    xx -= (Flt) re.max_len; break;
                }
                Flt len = (Flt) re.max_len;
                if (re.max_len <= 0)
                    len = lz->size_x;
                if (re.mode & PRINT_UNDERLINE)
                    lz->Underline(term, xx, rline, len, re.color);
                else
                    lz->Line(term, rline, re.color);
            }
            else
            {
                switch (re.align)
                {
                case ALIGN_LEFT:
                    lz->TextPosL(term, xx, rline, re.text.c_str(), re.color, re.mode);
                    break;
                case ALIGN_CENTER:
                    lz->TextPosC(term, xx, rline, re.text.c_str(), re.color, re.mode);
                    break;
                case ALIGN_RIGHT:
                    lz->TextPosR(term, xx, rline, re.text.c_str(), re.color, re.mode);
                    break;
                }
            }
        }
        line += re.new_lines;
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
                lz->TextPosL(term, 1, tl, term->Translate("Touch Here To See More"), color);
            else
                lz->TextC(term, tl, term->Translate("Touch To Print"), color);
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

    for (int i = 0; i < max_w; ++i)
    {
        text[i] = ' ';
        mode[i] = 0;
    }

    if (have_title)
        printer->SetTitle(report_title);
    printer->Start();
    auto print_entries = [&](std::vector<ReportEntry> &entry_list)
    {
        for (size_t re_idx = 0; re_idx<entry_list.size(); re_idx++)
        {
            const ReportEntry &re = entry_list[re_idx];
            // current element is the last element
            bool last_entry = re_idx+1 == entry_list.size();

            int width = printer->Width(re.mode);
            if (width > max_width)
                width = max_width;

            PrintEntry(re, 0, width, max_w, text, mode);

            if (re.new_lines > 0 || last_entry)
            {
                int max_end = max_w;
                while (max_end > 0 && text[max_end-1] == ' ' &&
                       (mode[max_end-1] & PRINT_UNDERLINE) == 0)
                {
                    --max_end;
                }
                for (int i = 0; i < max_end; ++i)
                    printer->Put(text[i], mode[i]);

                int next_mode = 0;
                if (!last_entry)
                {
                    const ReportEntry &re_next = entry_list[re_idx+1];
                    next_mode = re_next.mode &
                        (PRINT_RED | PRINT_LARGE | PRINT_NARROW);
                }
                printer->LineFeed(re.new_lines);

                for (int i = 0; i < max_w; ++i)
                {
                    text[i] = ' ';
                    mode[i] = next_mode;
                }
            }
        }
    };
    print_entries(header_list);
    if (!header_list.empty())
    {
        printer->LineFeed(1);
    }
    print_entries(body_list);

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
    std::vector<std::vector<char>> text(max_h, std::vector<char>(max_w+4, 0));
    std::vector<std::vector<int >> mode(max_h, std::vector<int >(max_w+4, 0));

    int header_lines = std::accumulate(
                header_list.cbegin(), header_list.cend(), 0,
                [](const int &sum, const ReportEntry &re) {return sum + re.new_lines;});

    int body_start = 2 + header_lines + 1;
    int max_lines  = max_h;
    int col_w      = max_w;
    if (col_w > max_width)
        col_w = max_width;

    // Check without footer
    int line = body_start;
    int column = 1;
    int my_page_num = 1;
    for (const ReportEntry &re : body_list)
    {
        line += re.new_lines;
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
    }

    if (my_page_num > 1)
    {
        // Check with footer
        max_lines = max_h - 2;
        line = body_start, column = 1, my_page_num = 1;
        for (const ReportEntry &re : body_list)
        {
            line += re.new_lines;
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
        }
    }

    int total_pages = my_page_num;
    my_page_num = 1;
    if (have_title)
        printer->SetTitle(report_title);
    printer->Start();
    size_t re_idx=0; // index to the currently processed body-ReportEntry
    while (re_idx<body_list.size())
    {
        // clear page
        for (int h = 0; h < max_h; ++h)
        {
            for (int w = 0; w < max_w; ++w)
            {
                text[h][w] = ' ';
                mode[h][w] = 0;
            }
            text[h][max_w] = '\0';
        }

        line   = 2;
        column = 0;

        // Header
        for (const ReportEntry &he : header_list)
        {
            PrintEntry(he, 0, max_w, max_w, text[line].data(), mode[line].data());
            if (he.new_lines > 0)
                line += he.new_lines;
        }
        line = body_start;

        // Body
        while (re_idx < body_list.size() && column < max_c)
        {
            while (re_idx < body_list.size() && line < max_lines)
            {
                const ReportEntry &re = body_list.at(re_idx);
                PrintEntry(re, column * 41, col_w, max_w, text[line].data(), mode[line].data());
                if (re.new_lines < 0)
                    line = max_lines;  // end of column
                else if (re.new_lines > 0)
                    line += re.new_lines;
                re_idx++; // go to next ReportEntry
            }
            line = body_start;
            ++column;
        }

        // Footer
        if (total_pages > 1)
        {
            line = max_h - 2;
            for (int i = 0; i < max_w; ++i)
                mode[line][i] = PRINT_UNDERLINE;
            ++line;
            strcpy(str, MasterLocale->Page(my_page_num, total_pages, lang, buffer));
            int len = strlen(str);
            strncpy(&text[line][(max_w-len)/2], str, len);
            ++my_page_num;
        }

        // Print Page
        for (int h = 0; h < max_h; ++h)
        {
            int max_end = max_w;

            while ((max_end > 0 && text[h][max_end-1] == ' ') &&
                   ((mode[h][max_end-1] & PRINT_UNDERLINE) == 0))
                --max_end;

            for (int i = 0; i < max_end; ++i)
                printer->Put(text[h][i], mode[h][i]);

            if (h < (max_h - 1))
            {
                printer->LineFeed(1);
            }
            else if (re_idx < body_list.size())
                printer->FormFeed();
        }
    }

    printer->End();
    return 0;
}

int Report::PrintEntry(const ReportEntry &report_entry, int start, int width, int my_max_width,
                       genericChar text[], int mode[])
{
    FnTrace("Report::PrintEntry()");

    int len;
    int i;
    int c;

    if (report_entry.text.empty())
        len = report_entry.max_len;
    else
    {
        len = report_entry.text.size();
        if (len > report_entry.max_len)
            len = report_entry.max_len;
    }
    if (len > (my_max_width - start))
        len = Max(my_max_width - start, 0);
    if (width > (my_max_width - start))
        width = Max(my_max_width - start, 0);

    int xx = (int) (report_entry.pos + .5);
    switch (report_entry.edge)
    {
    case ALIGN_CENTER:
        xx = (width / 2);
        break;
    case ALIGN_RIGHT:
        xx = width - xx;
        break;
    }

    switch (report_entry.align)
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

    if (!report_entry.text.empty())
    {
        xx += start;
        strncpy(&text[xx], report_entry.text.c_str(), len);
        if (report_entry.color == COLOR_RED || report_entry.color == COLOR_DK_RED)
            c = PRINT_RED;
        else if (report_entry.color == COLOR_BLUE || report_entry.color == COLOR_DK_BLUE)
            c = PRINT_BLUE;
        else
            c = 0;
        for (i = xx; i < (xx + len); ++i)
            mode[i] = report_entry.mode | c;
    }
    else
    {
        if (len <= 0)
            len = width - xx;
        xx += start;
        for (i = xx; i < (xx + len); ++i)
        {
            if (!(report_entry.mode & PRINT_UNDERLINE))
                text[i]  = '-';
            mode[i] = report_entry.mode;
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

int Report::Text(const std::string &text, int c, int align, float indent)
{
    FnTrace("Report::Text()");
    int retval = 0;
    ReportEntry re(text, c, align, current_mode);
    if (indent > 0.01)
    {
        re.pos  = indent;
        re.edge = ALIGN_LEFT;
    }
    else if (indent < -0.01)
    {
        re.pos  = -indent;
        re.edge = ALIGN_RIGHT;
    }
    else
        re.edge = align;
    retval = Add(re);
    return retval;
}

/****
 * Text2Col:  Prints the text across two columns, but requires that
 *   the page be at least 80 columns, for now at least.
 ***/
int Report::Text2Col(const std::string &text, int color, int align, float indent)
{
    FnTrace("Report::Text2Col()");
    int retval = 0;
    float col_width = (page_width / 2);
    float col2 = col_width + 1;
    float pos = 0;

    if (page_width >= 80)
    {
        if (align == ALIGN_CENTER)
        {
            pos = (col_width - text.size()) / 2;
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
    ReportEntry re("", c, ALIGN_LEFT, current_mode);
    return Add(re);
}

int Report::Underline(int len, int c, int a, float indent)
{
    FnTrace("Report::Underline()");
    ReportEntry re("", c, a, PRINT_UNDERLINE);
    if (indent > 0.01)
    {
        re.pos  = indent;
        re.edge = ALIGN_LEFT;
    }
    else if (indent < -0.01)
    {
        re.pos  = -indent;
        re.edge = ALIGN_RIGHT;
    }
    else
        re.edge = a;
    re.max_len = len;
    return Add(re);
}

int Report::NewLine(int nl)
{
    FnTrace("Report::NewLine()");
    if (add_where == 1)
    {
        if (header_list.empty() || header_list.back().new_lines < 0)
            return 1;
        header_list.back().new_lines += nl;
    }
    else
    {
        if (body_list.empty() || body_list.back().new_lines < 0)
            return 1;
        body_list.back().new_lines += nl;
    }

    return 0;
}

int Report::NewPage()
{
    FnTrace("Report::NewPage()");
    if (body_list.empty())
        return 1;

    body_list.back().new_lines = -1;
    return 0;
}
