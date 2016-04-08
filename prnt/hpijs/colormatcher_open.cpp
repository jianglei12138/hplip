/*****************************************************************************\
  colormatcher_open.cpp : Implimentation for the ColorMatcher_Open class

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

#include "header.h"
#include "hptypes.h"
#include "colormatch.h"

#include "colormatcher_open.h"
#define INTERPOLATE_5_BITS(a, b, d)     a + ( ( ( (long)b - (long)a ) * d) >> 5)
#define INTERPOLATE_4_BITS(a, b, d)     a + ( ( ( (long)b - (long)a ) * d) >> 4)

// Spatial Interpolation
#define INTERPOLATE_CUBE(r,g,b, cube, DOCALC) \
    DOCALC( (DOCALC( (DOCALC( cube[0], cube[4], (r))), \
                    (DOCALC( cube[2], cube[6], (r))), (g))), \
            (DOCALC( (DOCALC( cube[1], cube[5], (r))), \
                    (DOCALC( cube[3], cube[7], (r))), (g))), \
            (b))

APDK_BEGIN_NAMESPACE

ColorMatcher_Open::ColorMatcher_Open
(
    SystemServices* pSys,
    ColorMap cm,
    unsigned int DyeCount,
    unsigned int iInputWidth
) : ColorMatcher(pSys,cm, DyeCount,iInputWidth)
{  }

ColorMatcher_Open::~ColorMatcher_Open()
{ }


//#define DOCALC(a, b, d)     a + ( ( ( (long)b - (long)a ) * d) >> 5)

/*
BYTE DOCALC(BYTE a, BYTE b, BYTE d)
{
    return a + ( ( ( (long)b - (long)a ) * d) >> 5);
}

BYTE NewCalc(BYTE color[8], BYTE diff_red, BYTE diff_green, BYTE diff_blue)
{
    int dr32 = diff_red - 32;
    int dg32 = diff_green - 32;
    int db32 = diff_blue - 32;

    int x;
    x = -(color[0] * db32 * dg32 * dr32 + color[1] * diff_blue * (32 - diff_red) * dg32 +
        color[2] * diff_green * (32 - diff_red) * db32 + color[3] * diff_blue * diff_green * dr32 -
        diff_red * (color[4] * db32 * dg32 + color[5] * diff_blue * (32 - diff_green) -
        diff_green * (color[6] * db32 - color[7] * diff_blue))) >> 15;
    return x;
}
*/

void ColorMatcher_Open::Interpolate
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

#ifdef  _WIN32_WCE
    long    cyan[8], magenta[8],yellow[8],black[8];
#else
    BYTE    cyan[8], magenta[8],yellow[8],black[8];
#endif
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

#ifdef APDK_DJ3320

void ColorMatcher_Open::Interpolate
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
#ifdef  _WIN32_WCE
    long    cyan[8], magenta[8],yellow[8],black[8];
#else
    BYTE    cyan[8], magenta[8],yellow[8],black[8];
#endif

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

#endif

APDK_END_NAMESPACE
