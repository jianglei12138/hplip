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

#include "header.h"
#include "hptypes.h"
#include "colormatch.h"

APDK_BEGIN_NAMESPACE

ColorMatcher::ColorMatcher
(
    SystemServices* pSys,
    ColorMap cm,
    unsigned int DyeCount,
    unsigned int iInputWidth
) : ColorPlaneCount(DyeCount),
    InputWidth(iInputWidth),
    pSS(pSys),
    cmap(cm)
{
    constructor_error = NO_ERROR;
    ASSERT(cmap.ulMap1 != NULL || cmap.ulMap3 != NULL);

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

	Contone = (BYTE*)pSS->AllocMem(InputWidth*ColorPlaneCount);
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
    DBG1("destroying ColorMatcher \n");

    FreeBuffers();
} //~ColorMatcher


void ColorMatcher::Restart()
// also reset cache when we have one
{
	memset(Contone, 0, InputWidth*ColorPlaneCount);

    started = FALSE;
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
	pSS->FreeMemory(Contone);
}


BYTE* ColorMatcher::NextOutputRaster(COLORTYPE  rastercolor)
{
    if (iRastersReady == 0)
    {
        return NULL;
    }

	if (rastercolor == COLORTYPE_COLOR)
	{
		iRastersReady--; iRastersDelivered++;
	}

	if (rastercolor == COLORTYPE_COLOR)
	{
		if (raster.rasterdata[rastercolor] == NULL)
			return NULL;
		else
			return Contone;
	}
	else
	{
		return raster.rasterdata[rastercolor];
	}
} //NextOutputRaster


unsigned int ColorMatcher::GetOutputWidth(COLORTYPE  rastercolor)
{
	if (rastercolor == COLORTYPE_COLOR)
	{
		if (raster.rasterdata[rastercolor] == NULL)
			return 0;
		else
			return InputWidth*ColorPlaneCount;
	}
	else
	{
		return raster.rastersize[rastercolor];
	}
} //GetOutputWidth


unsigned int ColorMatcher::GetMaxOutputWidth(COLORTYPE  rastercolor)
{
	if (rastercolor == COLORTYPE_COLOR)
	{
		if (raster.rasterdata[rastercolor] == NULL)
			return 0;
		else
			return InputWidth*ColorPlaneCount;
	}
	else
	{
		return raster.rastersize[rastercolor];
	}
} //GetMaxOutPutWidth


//////////////////////////////////////////////////////////////////////////////

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

#ifdef APDK_DJ3320
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
#endif

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


BOOL ColorMatcher::Process(RASTERDATA* pbyInputKRGBRaster)
{
    if (pbyInputKRGBRaster == NULL || (pbyInputKRGBRaster->rasterdata[COLORTYPE_BLACK] == NULL && pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR] == NULL))
    {
        Restart();
        return FALSE;   // no output
    }
    started=TRUE;

	if (pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR])
	{
		BYTE* buff1 = NULL;
		BYTE* buff2 = NULL;
		BYTE* buff3 = NULL;
		BYTE* buff4 = NULL;

		if (StartPlane==K)
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

#ifdef APDK_DJ3320
        if (cmap.ulMap3)
        {
		    ColorMatch( InputWidth, // ASSUMES ALL INPUTWIDTHS EQUAL
			    cmap.ulMap3,
			    pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR],
			    buff1,
			    buff2,
			    buff3,
			    buff4
		    );
        }
#endif
        if (cmap.ulMap1)
        {
		    // colormatching -- can only handle 4 planes at a time
		    ColorMatch( InputWidth, // ASSUMES ALL INPUTWIDTHS EQUAL
			    cmap.ulMap1,
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
				cmap.ulMap2,    // 2nd map is for lighter inks
				pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR],
				NULL,           // don't need black again
				buff5,buff6,
				NULL            // don't need yellow again
			);
		}
	}

    iRastersReady = 1;
    iRastersDelivered = 0;
    return TRUE;   // one raster in, one raster out
}

APDK_END_NAMESPACE

