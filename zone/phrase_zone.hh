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
 * phrase_zone.hh - revision 3 (5/6/97)
 * Phase translation/replacement interface
 */

#ifndef _PHRASE_ZONE_HH
#define PHRASE_ZONE_HH

#include "form_zone.hh"


/**** Types ****/
class PhraseZone : public FormZone
{
public:
    // Constructor
    PhraseZone();

    // Member Functions
    int          Type() override { return ZONE_PHRASE; }
    RenderResult Render(Terminal *t, int update_flag) override;

    int LoadRecord(Terminal *t, int record) override;
    int SaveRecord(Terminal *t, int record, int write_file) override;
    int RecordCount(Terminal *t) override;
};

#endif
