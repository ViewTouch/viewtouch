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

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** Textures ****/
#include "xpm/darksand-6.xpm"
#include "xpm/darkwood-10.xpm"
#include "xpm/grayparchment-8.xpm"
#include "xpm/graymarble-12.xpm"
#include "xpm/greenmarble-12.xpm"
#include "xpm/litewood-8.xpm"
#include "xpm/litsand-6.xpm"
#include "xpm/sand-8.xpm"
#include "xpm/wood-10.xpm"
#include "xpm/canvas-8.xpm"
#include "xpm/pearl-8.xpm"
#include "xpm/parchment-6.xpm"
#include "xpm/tanparchment-8.xpm"
#include "xpm/smoke-4.xpm"
#include "xpm/leather-8.xpm"
#include "xpm/gradient-8.xpm"
#include "xpm/black.xpm"
#include "xpm/gradient-brown.xpm"
#include "xpm/blueparchment.xpm"
#include "xpm/greySand.xpm"
#include "xpm/whiteMesh.xpm"


/**** Image Values ****/
int ImageValue[] = {
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
    -1};

char **ImageData[] = {
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
    NULL};

/**** Functions ****/
int ImageColorsUsed()
{
    int colors = 0;
    int tmp;
    int c;
    for (int i = 0; ImageData[i] != NULL; ++i)
    {
        sscanf(ImageData[i][0], "%d %d %d", &tmp, &tmp, &c);
        colors += c;
    }
    return colors;
}

int ImageWidth(int image)
{
    int width = 0;
    sscanf(ImageData[image][0], "%d", &width);

    return width;
}

int ImageHeight(int image)
{
    int height = 0;
    int tmp;
    sscanf(ImageData[image][0], "%d %d", &tmp, &height);

    return height;
}
