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
 * image_data.hh - revision 25 (1/7/98)
 * Data for textures in XPM format
 */

#ifndef _IMAGE_DATA_HH
#define IMAGE_DATA_HH


#include <array>
#include <cstddef> // size_t

/**** Image Data ****/

enum textures {
    IMAGE_SAND,
    IMAGE_LIT_SAND,
    IMAGE_DARK_SAND,
    IMAGE_LITE_WOOD,
    IMAGE_WOOD,
    IMAGE_DARK_WOOD,
    IMAGE_GRAY_PARCHMENT,
    IMAGE_GRAY_MARBLE,
    IMAGE_GREEN_MARBLE,
    IMAGE_PARCHMENT,
    IMAGE_PEARL,
    IMAGE_CANVAS,
    IMAGE_TAN_PARCHMENT,
    IMAGE_SMOKE,
    IMAGE_LEATHER,
    IMAGE_BLUE_PARCHMENT,
    IMAGE_GRADIENT,
    IMAGE_GRADIENTBROWN,
    IMAGE_BLACK,
    IMAGE_GREYSAND,
    IMAGE_WHITEMESH,
    IMAGE_CARBON_FIBER,
    IMAGE_WHITE_TEXTURE,
    IMAGE_DARK_ORANGE_TEXTURE,
    IMAGE_YELLOW_TEXTURE,
    IMAGE_GREEN_TEXTURE,
    IMAGE_ORANGE_TEXTURE,
    IMAGE_BLUE_TEXTURE,
    IMAGE_POOL_TABLE,
    IMAGE_TEST,
    IMAGE_DIAMOND_LEATHER,
    IMAGE_BREAD,
    IMAGE_LAVA,
    IMAGE_DARK_MARBLE,
    IMAGE_COUNT // 34 // number of images
};

constexpr int IMAGE_CLEAR     = 253;
constexpr int IMAGE_UNCHANGED = 254;
constexpr int IMAGE_DEFAULT   = 255;

extern const std::array<const char*, IMAGE_COUNT> TextureFiles;
extern const std::array<const char**, IMAGE_COUNT> ImageData; // Deprecated - for backward compatibility

/**** Functions ****/
int ImageColorsUsed();  // Returns total colors used in all xpm files
int ImageWidth(const size_t image);
int ImageHeight(const size_t image);

#endif
