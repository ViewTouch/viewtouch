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
 * unified_target_zone.cc
 * Unified targeting zone that combines video and printer targeting functionality
 * This eliminates the need for separate video and printer targeting interfaces
 */

#include "unified_target_zone.hh"
#include "terminal.hh"
#include "labels.hh"
#include "locale.hh"
#include "settings.hh"
#include "system.hh"
#include <cstring>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

// Constructor
UnifiedTargetZone::UnifiedTargetZone()
{
    FnTrace("UnifiedTargetZone::UnifiedTargetZone()");

    phrases_changed = 0;
    current_mode = 0;  // Start with Video Target mode
    AddFields();
}

// Member Functions
int UnifiedTargetZone::AddFields()
{
    FnTrace("UnifiedTargetZone::AddFields()");

    // Add mode toggle button
    AddButtonField("Toggle Video/Printer Mode", "toggle_mode");
    AddNewLine();
    
    // Add instruction text
    AddLabel("Video Targets must match Printer Targets");
    AddNewLine();
    AddNewLine();

    int i = 0;

    // Ensure we don't go beyond the array bounds and handle NULL entries
    while (i < MAX_FAMILIES && FamilyName[i] != NULL)
    {
        AddListField(MasterLocale->Translate(FamilyName[i]),
                     PrinterIDName, PrinterIDValue);
        i++;
    }

    return 0;
}

RenderResult UnifiedTargetZone::Render(Terminal *term, int update_flag)
{
    FnTrace("UnifiedTargetZone::Render()");

    if (phrases_changed < term->system_data->phrases_changed)
    {
        Purge();
        AddFields();
        phrases_changed = term->system_data->phrases_changed;
    }

    FormZone::Render(term, update_flag);
    
    // Display current mode in title
    genericChar title[256];
    sprintf(title, "Unified Family Targeting - %s Mode", GetModeName());
    TextC(term, 0, title, color[0]);
    
    return RENDER_OKAY;
}

SignalResult UnifiedTargetZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("UnifiedTargetZone::Signal()");
    
    if (strcmp(message, "toggle_mode") == 0)
    {
        ToggleMode();
        Draw(term, 1);  // Redraw to update display
        return SIGNAL_OKAY;
    }
    
    return SIGNAL_IGNORED;
}

// Touch method is not needed - ButtonField handles it automatically

int UnifiedTargetZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("UnifiedTargetZone::LoadRecord()");
    Settings *settings = term->GetSettings();
    FormField *form = FieldList();
    int idx = 0;
    
    // Skip the toggle button and labels (first 4 fields: button, newline, label, newline)
    for (int i = 0; i < 4 && form; i++)
        form = form->next;
    
    // Ensure we don't go beyond the array bounds and handle NULL entries
    while (idx < MAX_FAMILIES && FamilyName[idx] && form)
    {
        form->active = (settings->family_group[FamilyValue[idx]] != SALESGROUP_NONE);
        
        // Load based on current mode
        if (current_mode == 0)  // Video Target mode
            form->Set(settings->video_target[idx]);
        else  // Printer Target mode
            form->Set(settings->family_printer[idx]);
            
        form = form->next;
        ++idx;
    }
    return 0;
}

int UnifiedTargetZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("UnifiedTargetZone::SaveRecord()");
    Settings *settings = term->GetSettings();
    FormField *form = FieldList();
    int idx = 0;
    
    // Skip the toggle button and labels (first 4 fields: button, newline, label, newline)
    for (int i = 0; i < 4 && form; i++)
        form = form->next;
    
    // Ensure we don't go beyond the array bounds and handle NULL entries
    while (idx < MAX_FAMILIES && FamilyName[idx] && form)
    {
        int value;
        form->Get(value);
        
        // Save to both arrays to ensure they match
        settings->video_target[idx] = value;
        settings->family_printer[idx] = value;
        
        form = form->next;
        ++idx;
    }
    
    if (write_file)
        settings->Save();
    return 0;
}

const char* UnifiedTargetZone::GetModeName()
{
    return (current_mode == 0) ? "Video Target" : "Printer Target";
} 