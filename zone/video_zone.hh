/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
  
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
 * video_zone.hh
 * Zones related in some way to video.  For example, VideoTargetZone is used
 *  to determine which food types get sent to the Kitchen Video reports.
 */

#define VIDEO_TARGET_NORMAL  0
#define VIDEO_TARGET_KITCHEN 1

#include "form_zone.hh"

class VideoTargetZone : public FormZone
{
    unsigned long phrases_changed;

public:
    // Constructor
    VideoTargetZone();

    // Member Functions
    int          Type() { return ZONE_VIDEO_TARGET; }
    int          AddFields();
    RenderResult Render(Terminal *t, int update_flag);

    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
};
