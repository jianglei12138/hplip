/*****************************************************************************\
  colormatch.cpp : Implimentation for the ColorMatcher class

  Copyright (c) 1996 - 2015, HP Co.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of HP nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
  NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
  TO, PATENT INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\*****************************************************************************/

#include "CommonDefinitions.h"
#include "Processor.h"
#include "ColorMatcher.h"

ColorMatcher::ColorMatcher
(
    ColorMap cm,
    unsigned int DyeCount,
    unsigned int iInputWidth
) : ColorPlaneCount(DyeCount),
    InputWidth(iInputWidth),
    cmap(cm)
{
    constructor_error = NO_ERROR;
    ASSERT((cmap.ulMap1 != NULL || cmap.ulMap3 != NULL));
    StartPlane = K;       // most common case

    if (ColorPlaneCount == 3)     // CMY pen
    {
        StartPlane = C;
    }

    EndPlane = Y;         // most common case
    if (ColorPlaneCount == 6)
    {
        EndPlane = Mlight;
    }
    if (ColorPlaneCount == 1)
    {
        EndPlane = K;
    }

    Contone = (BYTE *) new BYTE[InputWidth * ColorPlaneCount];
    if (Contone == NULL)
    {
        goto MemoryError;
    }

    Restart();  // this zeroes buffers and sets nextraster counter

    return;

MemoryError:
    constructor_error=ALLOCMEM_ERROR;

    FreeBuffers();
    return;
} //ColorMatcher

ColorMatcher::~ColorMatcher()
{
    FreeBuffers();
} //~ColorMatcher


void ColorMatcher::Restart()
// also reset cache when we have one
{
    memset(Contone, 0, InputWidth*ColorPlaneCount);

    started = false;
}

void ColorMatcher::Flush()
// needed to reset cache
{
    if (!started)
    {
        return;
    }
    Restart();
}

void ColorMatcher::FreeBuffers()
{
    delete [] Contone;
}

bool ColorMatcher::NextOutputRaster(RASTERDATA &next_raster)
{
    if (iRastersReady == 0)
        return false;

    iRastersReady--;
    iRastersDelivered++;
    if (raster.rasterdata[COLORTYPE_COLOR] != NULL)
    {
        next_raster.rastersize[COLORTYPE_COLOR] = raster.rastersize[COLORTYPE_COLOR];
        next_raster.rasterdata[COLORTYPE_COLOR] = Contone;
    }
    else
    {
        next_raster.rastersize[COLORTYPE_COLOR] = 0;
        next_raster.rasterdata[COLORTYPE_COLOR] = raster.rasterdata[COLORTYPE_COLOR];
    }
    next_raster.rastersize[COLORTYPE_BLACK] = raster.rastersize[COLORTYPE_BLACK];
    next_raster.rasterdata[COLORTYPE_BLACK] = raster.rasterdata[COLORTYPE_BLACK];
    return true;
}


unsigned int ColorMatcher::GetMaxOutputWidth()
{
    if (myplane == COLORTYPE_COLOR)
    {
        if (raster.rasterdata[myplane] == NULL)
            return 0;
        else
            return InputWidth*ColorPlaneCount;
    }
    else
    {
        return raster.rastersize[myplane];
    }
} //GetMaxOutPutWidth

void ColorMatcher::ColorMatch
(
    unsigned long width,
    const uint32_t *map,
    unsigned char *rgb,
    unsigned char *kplane,
    unsigned char *cplane,
    unsigned char *mplane,
    unsigned char *yplane
)
{
    static      uint32_t    prev_red = 255, prev_green = 255, prev_blue = 255;
    static      BYTE        bcyan, bmagenta, byellow, bblack;

    uint32_t    r;
    uint32_t    g;
    uint32_t    b;

    for (unsigned long i = 0; i < width; i++)
    {
        r = *rgb++;
        g = *rgb++;
        b = *rgb++;

        if(i == 0 || ( (prev_red != r) || (prev_green != g) || (prev_blue != b) ))
        {
            prev_red =   r;
            prev_green = g;
            prev_blue =  b;

            Interpolate(map, (BYTE)r, (BYTE)g,(BYTE)b, &bblack, &bcyan, &bmagenta, &byellow);
        }
        if (kplane)
            *(kplane + i) = bblack;
        if (cplane)
            *(cplane + i) = bcyan;
        if (mplane)
            *(mplane + i) = bmagenta;
        if (yplane)
            *(yplane + i) = byellow;

    }

} //ColorMatch

void ColorMatcher::ColorMatch
(
    unsigned long width,
    const unsigned char *map,
    unsigned char *rgb,
    unsigned char *kplane,
    unsigned char *cplane,
    unsigned char *mplane,
    unsigned char *yplane
)
{
    static      BYTE        prev_red = 255, prev_green = 255, prev_blue = 255;
    static      BYTE        bcyan, bmagenta, byellow, bblack;

    BYTE    r;
    BYTE    g;
    BYTE    b;

    for (unsigned long i = 0; i < width; i++)
    {
        r = *rgb++;
        g = *rgb++;
        b = *rgb++;
        if(i == 0 || ( (prev_red != r) || (prev_green != g) || (prev_blue != b) ))
        {
            prev_red =   r;
            prev_green = g;
            prev_blue =  b;

            Interpolate(map, (BYTE)r, (BYTE)g,(BYTE)b, &bblack, &bcyan, &bmagenta, &byellow);
        }
        if (kplane)
            *(kplane + i) = bblack;
        if (cplane)
            *(cplane + i) = bcyan;
        if (mplane)
            *(mplane + i) = bmagenta;
        if (yplane)
            *(yplane + i) = byellow;

    }

} //ColorMatch

uint32_t Packed(unsigned int k,unsigned int c,unsigned int m,unsigned int y)
{
    uint32_t p = y;
    p = p << 8;
    p += m;
    p = p << 8;
    p += c;
    p = p << 8;
    p += k;
    return p;
} //Packed


DRIVER_ERROR ColorMatcher::MakeGrayMap(const uint32_t *colormap, uint32_t* graymap)
{
    unsigned long   ul_MapPtr;
    for (unsigned int r = 0; r < 9; r++)
    {
        unsigned long ul_RedMapPtr = r * 9 * 9;
        for (unsigned int g = 0; g < 9; g++)
        {
            unsigned long ul_GreenMapPtr = g * 9;
            for (unsigned int b = 0; b < 9; b++)
            {
                unsigned long mapptr = b + (g * 9) + (r * 9 * 9);       // get address in map
                ul_MapPtr = b + ul_GreenMapPtr + ul_RedMapPtr;
                ASSERT(mapptr == ul_MapPtr);
                // put r,g,b in monitor range
                unsigned int oldR = r * 255 >> 3;
                unsigned int oldG = g * 255 >> 3;
                unsigned int oldB = b * 255 >> 3;

                // calculate gray equivalence
                unsigned int gray = ((30 * oldR + 59 * oldG + 11 * oldB + 50) / 100);

                uint32_t *start;
                start = (uint32_t *)
                        ( ((gray & 0xE0) <<1) + ((gray & 0xE0)>>1) + (gray>>5) +
                          ((gray & 0xE0) >>2) + (gray>>5) + (gray>>5) + colormap);

                 BYTE k,c,m,y;
                 Interpolate(start, gray, gray, gray, &k, &c, &m, &y);

                // second interpolate if Clight/Mlight

                *(graymap + mapptr) = Packed(k, c, m, y);
            }
        }
    }
    return NO_ERROR;
} //MakeGrayMap


bool ColorMatcher::Process(RASTERDATA* pbyInputKRGBRaster)
{
    if (pbyInputKRGBRaster == NULL || (pbyInputKRGBRaster->rasterdata[COLORTYPE_BLACK] == NULL && pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR] == NULL))
    {
        Restart();
        return false;   // no output
    }
    started=true;

    if (pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR])
    {
        BYTE* buff1 = NULL;
        BYTE* buff2 = NULL;
        BYTE* buff3 = NULL;
        BYTE* buff4 = NULL;

        if (StartPlane == K)
        {
            buff1 = Contone;
            if (EndPlane>K)
            {
                buff2 = buff1 + InputWidth;
                buff3 = buff2 + InputWidth;
                buff4 = buff3 + InputWidth;
            }
        }
        else
        {
            buff2 = Contone;
            buff3 = buff2 + InputWidth;
            buff4 = buff3 + InputWidth;
        }

        if (cmap.ulMap3)
        {
            ColorMatch( InputWidth, // ASSUMES ALL INPUTWIDTHS EQUAL
                (const unsigned char *) cmap.ulMap3,
                pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR],
                buff1,
                buff2,
                buff3,
                buff4
            );
        }
        if (cmap.ulMap1)
        {
            // colormatching -- can only handle 4 planes at a time
            ColorMatch( InputWidth, // ASSUMES ALL INPUTWIDTHS EQUAL
                (const uint32_t *) cmap.ulMap1,
                pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR],
                buff1,
                buff2,
                buff3,
                buff4
            );
        }

        if (EndPlane > Y && cmap.ulMap2)
        {
            BYTE* buff5 = buff4 + InputWidth;
            BYTE* buff6 = buff5 + InputWidth;

            ColorMatch( InputWidth,
                (const uint32_t *) cmap.ulMap2,    // 2nd map is for lighter inks
                pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR],
                NULL,           // don't need black again
                buff5,buff6,
                NULL            // don't need yellow again
            );
        }
    }

    iRastersReady = 1;
    iRastersDelivered = 0;
    return true;   // one raster in, one raster out
}

#define INTERPOLATE_5_BITS(a, b, d)     a + ( ( ( (long)b - (long)a ) * d) >> 5)
#define INTERPOLATE_4_BITS(a, b, d)     a + ( ( ( (long)b - (long)a ) * d) >> 4)

// Spatial Interpolation
#define INTERPOLATE_CUBE(r,g,b, cube, DOCALC) \
    DOCALC( (DOCALC( (DOCALC( cube[0], cube[4], (r))), \
                    (DOCALC( cube[2], cube[6], (r))), (g))), \
            (DOCALC( (DOCALC( cube[1], cube[5], (r))), \
                    (DOCALC( cube[3], cube[7], (r))), (g))), \
            (b))

void ColorMatcher::Interpolate
(
    const uint32_t *map,
    BYTE r,
    BYTE g,
    BYTE b,
    BYTE *blackout,
    BYTE *cyanout,
    BYTE *magentaout,
    BYTE *yellowout
)
{
    static int cube_location[]  = {0, 1,  9, 10,  81,  82,  90,  91 };
    const uint32_t *start;

    BYTE    cyan[8], magenta[8],yellow[8],black[8];
    start = (const uint32_t *)
        (((r & 0xE0) << 1) + ((r & 0xE0) >> 1) + (r >> 5) +
        ((g & 0xE0) >> 2) + (g >> 5) + (b >> 5) + map);

    uint32_t    cValue;
    for (int j = 0; j < 8; j++)
    {
        cValue = *(start + cube_location[j]);
        cyan[j]    = GetCyanValue (cValue);
        magenta[j] = GetMagentaValue (cValue);
        yellow[j]  = GetYellowValue (cValue);
        black[j]   = GetBlackValue (cValue);
    }

    ////////////////this is the 8 bit 9cube operation /////////////
    BYTE diff_red   = r & 0x1f;
    BYTE diff_green = g & 0x1f;
    BYTE diff_blue  = b & 0x1f;

    *cyanout    = INTERPOLATE_CUBE(diff_red,diff_green,diff_blue, cyan, INTERPOLATE_5_BITS );
    *magentaout = INTERPOLATE_CUBE(diff_red,diff_green,diff_blue, magenta, INTERPOLATE_5_BITS );
    *yellowout  = INTERPOLATE_CUBE(diff_red,diff_green,diff_blue, yellow, INTERPOLATE_5_BITS );
    *blackout   = INTERPOLATE_CUBE(diff_red,diff_green,diff_blue, black, INTERPOLATE_5_BITS );
}

void ColorMatcher::Interpolate
(
    const unsigned char *map,
    BYTE r,
    BYTE g,
    BYTE b,
    BYTE *blackout,
    BYTE *cyanout,
    BYTE *magentaout,
    BYTE *yellowout
)
{
    BYTE    cyan[8], magenta[8],yellow[8],black[8];

//  static int cube_location[]  = {0, 1, 17, 18, 289, 290, 306, 307};
    static int cube_location[]  = {0, 4, 68, 72, 1156, 1160, 1224, 1228};
    const   BYTE    *start;

    BYTE *node_ptr;

    start = (const unsigned char *)
        ((((r & 0xF0) << 4) + ((r & 0xF0) << 1) + (r >> 4) +
        ((g & 0xF0)) + (g >> 4) + (b >> 4)) * 4 + map);

    // use (start) to determine the surrounding cube values
    for (int j = 0; j < 8; ++j )
    {
        node_ptr = (BYTE *) (start + cube_location[j]);
        black[j]   = *node_ptr++;
        cyan[j]    = *node_ptr++;
        magenta[j] = *node_ptr++;
        yellow[j]  = *node_ptr;
    }


    // interpolate using the 4 LSBs
    BYTE diff_red   = r & 0x0f;
    BYTE diff_green = g & 0x0f;
    BYTE diff_blue  = b & 0x0f;

    *cyanout    = INTERPOLATE_CUBE(diff_red,diff_green,diff_blue, cyan, INTERPOLATE_4_BITS );
    *magentaout = INTERPOLATE_CUBE(diff_red,diff_green,diff_blue, magenta, INTERPOLATE_4_BITS );
    *yellowout  = INTERPOLATE_CUBE(diff_red,diff_green,diff_blue, yellow, INTERPOLATE_4_BITS );
    *blackout   = INTERPOLATE_CUBE(diff_red,diff_green,diff_blue, black, INTERPOLATE_4_BITS );
}

