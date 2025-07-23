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
 * unified_target_zone.hh
 * Unified targeting zone that combines video and printer targeting functionality
 * This eliminates the need for separate video and printer targeting interfaces
 */

#ifndef _UNIFIED_TARGET_ZONE_HH
#define _UNIFIED_TARGET_ZONE_HH

#include "form_zone.hh"

class UnifiedTargetZone : public FormZone
{
    unsigned long phrases_changed;
    int current_mode;  // 0 = Video Target, 1 = Printer Target

public:
    // Constructor
    UnifiedTargetZone();

    // Member Functions
    int          Type() { return ZONE_UNIFIED_TARGET; }
    int          AddFields();
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);

    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
    
    // Helper functions
    int ToggleMode() { current_mode = 1 - current_mode; return current_mode; }
    int GetMode() { return current_mode; }
    const char* GetModeName();
};

#endif 