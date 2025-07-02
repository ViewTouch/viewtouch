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
 * image_data.cc - revision 25 (9/7/98)
 * implementation of image_data module
 */

#include "basic.hh"
#include "image_data.hh"

#include <cassert>
#include <sstream>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** Textures ****/
#include "xpm/black.xpm"
#include "xpm/blueparchment.xpm"
#include "xpm/brass-8.xpm"
#include "xpm/canvas-8.xpm"
#include "xpm/concrete-8.xpm"
#include "xpm/copper-8.xpm"
#include "xpm/darksand-6.xpm"
#include "xpm/darkwood-10.xpm"
#include "xpm/demo-converted.xpm"
#include "xpm/embossed-8.xpm"
#include "xpm/glass-8.xpm"
#include "xpm/gradient-8.xpm"
#include "xpm/gradient-brown.xpm"
#include "xpm/grass-6.xpm"
#include "xpm/gray-6.xpm"
#include "xpm/grayblue-3.xpm"
#include "xpm/graymarble-12.xpm"
#include "xpm/grayparchment-8.xpm"
#include "xpm/greenmarble-12.xpm"
#include "xpm/greySand.xpm"
#include "xpm/leather-8.xpm"
#include "xpm/lighttanparchment-8.xpm"
#include "xpm/litewood-8.xpm"
#include "xpm/litsand-6.xpm"
#include "xpm/metal-8.xpm"
#include "xpm/parchment-6.xpm"
#include "xpm/pearl-8.xpm"
#include "xpm/plastic-8.xpm"
#include "xpm/redgravel-6.xpm"
#include "xpm/sand-8.xpm"
#include "xpm/silk-8.xpm"
#include "xpm/smoke-4.xpm"
#include "xpm/smoke-8.xpm"
#include "xpm/steel-8.xpm"
#include "xpm/stone-8.xpm"
#include "xpm/tanparchment-8.xpm"
#include "xpm/velvet-8.xpm"
#include "xpm/water-10.xpm"
#include "xpm/whiteMesh.xpm"
#include "xpm/wood-10.xpm"
#include "xpm/woodfloor-12.xpm"
#include "xpm/yellowstucco-8.xpm"



/**** Image Values ****/

constexpr std::array<const char**, IMAGE_COUNT> ImageData = {
    SandData,           // IMAGE_SAND
    LitSandData,        // IMAGE_LIT_SAND
    DarkSandData,       // IMAGE_DARK_SAND
    LiteWoodData,       // IMAGE_LITE_WOOD
    WoodData,           // IMAGE_WOOD
    DarkWoodData,       // IMAGE_DARK_WOOD
    GrayParchmentData,  // IMAGE_GRAY_PARCHMENT
    GrayMarbleData,     // IMAGE_GRAY_MARBLE
    GreenMarbleData,    // IMAGE_GREEN_MARBLE
    ParchmentData,      // IMAGE_PARCHMENT
    PearlData,          // IMAGE_PEARL
    CanvasData,         // IMAGE_CANVAS
    LightTanParchmentData, // IMAGE_TAN_PARCHMENT
    smoke8_xpm,         // IMAGE_SMOKE
    LeatherData,        // IMAGE_LEATHER
    BlueParchementData, // IMAGE_BLUE_PARCHMENT
    gradient_8_xpm,     // IMAGE_GRADIENT
    gradient_brown_xpm, // IMAGE_GRADIENTBROWN
    black_xpm,          // IMAGE_BLACK
    greySand,           // IMAGE_GREYSAND
    whiteMesh,          // IMAGE_WHITEMESH
    metal_xpm,          // IMAGE_METAL
    stone_xpm,          // IMAGE_STONE
    glass_xpm,          // IMAGE_GLASS
    velvet_xpm,         // IMAGE_VELVET
    concrete_xpm,       // IMAGE_CONCRETE
    silk_xpm,           // IMAGE_SILK
    brass_xpm,          // IMAGE_BRASS
    copper_xpm,         // IMAGE_COPPER
    steel_xpm,          // IMAGE_STEEL
    plastic_xpm,        // IMAGE_PLASTIC
    demo_xpm,           // IMAGE_DEMO

};

/**** Functions ****/
int ImageColorsUsed()
{
    int colors = 0;
    int tmp;
    int c;
    for (size_t image = 0; image < IMAGE_COUNT; ++image)
    {
        std::istringstream ss(ImageData[image][0]);
        ss >> tmp >> tmp >> c;
        if (ss.fail())
            assert(false);
        colors += c;
    }
    return colors;
}

int ImageWidth(const size_t image)
{
    assert(image < IMAGE_COUNT);
    int width = 0;
    std::istringstream ss(ImageData[image][0]);
    ss >> width;
    if (ss.fail())
        assert(false);

    return width;
}

int ImageHeight(const size_t image)
{
    assert(image < IMAGE_COUNT);
    int height = 0;
    int tmp;
    std::istringstream ss(ImageData[image][0]);
    ss >> tmp >> height;
    if (ss.fail())
        assert(false);

    return height;
}
