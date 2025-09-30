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
#include "assets/images/xpm/darksand-6.xpm"
#include "assets/images/xpm/darkwood-10.xpm"
#include "assets/images/xpm/grayparchment-8.xpm"
#include "assets/images/xpm/graymarble-12.xpm"
#include "assets/images/xpm/greenmarble-12.xpm"
#include "assets/images/xpm/litewood-8.xpm"
#include "assets/images/xpm/litsand-6.xpm"
#include "assets/images/xpm/sand-8.xpm"
#include "assets/images/xpm/wood-10.xpm"
#include "assets/images/xpm/canvas-8.xpm"
#include "assets/images/xpm/pearl-8.xpm"
#include "assets/images/xpm/parchment-6.xpm"
#include "assets/images/xpm/tanparchment-8.xpm"
#include "assets/images/xpm/smoke-4.xpm"
#include "assets/images/xpm/leather-8.xpm"
#include "assets/images/xpm/gradient-8.xpm"
#include "assets/images/xpm/black.xpm"
#include "assets/images/xpm/gradient-brown.xpm"
#include "assets/images/xpm/blueparchment.xpm"
#include "assets/images/xpm/greySand.xpm"
#include "assets/images/xpm/whiteMesh.xpm"
#include "assets/images/xpm/carbonfiber-128-6.xpm"
#include "assets/images/xpm/whitetexture-128-32.xpm"
#include "assets/images/xpm/darkorangetexture-128-32.xpm"
#include "assets/images/xpm/yellowtexture-128-32.xpm"
#include "assets/images/xpm/greentexture-128-32.xpm"
#include "assets/images/xpm/orangetexture-128-32.xpm"
#include "assets/images/xpm/bluetexture-128-32.xpm"
#include "assets/images/xpm/pooltable-256.xpm"
#include "assets/images/xpm/test-256.xpm"
#include "assets/images/xpm/diamondleather-256.xpm"
#include "assets/images/xpm/bread-256.xpm"
#include "assets/images/xpm/lava-256.xpm"
#include "assets/images/xpm/darkmarble-256.xpm"


/**** Image Values ****/

constexpr std::array<const char**, IMAGE_COUNT> ImageData = {
    SandData,
    LitSandData,
    DarkSandData,
    LiteWoodData,
    WoodData,
    DarkWoodData,
    GrayParchmentData,
    GrayMarbleData,
    GreenMarbleData,
    ParchmentData,
    PearlData,
    CanvasData,
    TanParchmentData,
    smokeData,
    LeatherData,
    BlueParchementData,
    gradient_8_xpm,
    gradient_brown_xpm,
    black_xpm,
    greySand,
    whiteMesh,
    CarbonFiberData,
    WhiteTextureData,
    DarkOrangeTextureData,
    YellowTextureData,
    GreenTextureData,
    OrangeTextureData,
    BlueTextureData,
    PoolTableData,
    TestData,
    DiamondLeatherData,
    BreadData,
    LavaData,
    DarkMarbleData
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
