/*****************************************************************************\
  Mode3.cpp : Implimentation for the Mode3 class

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
#include "Compressor.h"
#include "Pipeline.h"
#include "Mode3.h"

Mode3::Mode3 (unsigned int RasterSize) : Compressor (RasterSize, true)
{
    // Worst case is when two rows are completely different
    // In that case, one command byte is added for every 8 bytes
    // In the worst case, compression expands data by 50%
    compressBuf = new BYTE[RasterSize + RasterSize/2];
    if (compressBuf == NULL)
        constructor_error = ALLOCMEM_ERROR;
    
    memset (SeedRow, 0x0, inputsize);
}

Mode3::~Mode3 ()
{
}

void Mode3::Flush ()
{
    if (!seeded)
        return;
    compressedsize = 0;
    iRastersReady  = 0;
    seeded         = false;
    memset (SeedRow, 0x0, inputsize);
}

bool Mode3::Process (RASTERDATA *input)
{
    if (input==NULL || 
        (myplane == COLORTYPE_COLOR && input->rasterdata[COLORTYPE_COLOR] == NULL) ||
        (myplane == COLORTYPE_BLACK && input->rasterdata[COLORTYPE_BLACK] == NULL))    // flushing pipeline
    {
        Flush();
        return false;
    }
    else
    {
        seeded = true;
    }

    unsigned    int     uOrgSize = input->rastersize[myplane];
    unsigned    int     size = input->rastersize[myplane];
    unsigned    int     uOffset;

    BYTE        *pszSptr  = SeedRow;
    BYTE        *pszInPtr = input->rasterdata[myplane];
    BYTE        *pszCurPtr;
    BYTE        ucByteCount;
    BYTE        *pszOutPtr = compressBuf;

    while (size > 0)
    {
        uOffset = 0;

        if (seeded)
        {
            while ((*pszSptr == *pszInPtr) && (uOffset < size))
            {
                pszSptr++;
                pszInPtr++;
                uOffset++;
            }
        }

        if (uOffset >= size)
        {
          break;
        }

        size -= uOffset;

        pszCurPtr = pszInPtr;
        ucByteCount = 1;
        pszSptr++;
        pszInPtr++;
        while ((*pszSptr != *pszInPtr) && ucByteCount < size && ucByteCount < 8)
        {
            pszSptr++;
            pszInPtr++;
            ucByteCount++;
        }
        ucByteCount--;
        if (uOffset < 31)
        {
            *pszOutPtr++ = ((ucByteCount << 5) | uOffset);
        }
        else
        {
            uOffset -= 31;
            *pszOutPtr++ = ((ucByteCount << 5) | 31);

            while (uOffset >= 255)
            {
                *pszOutPtr++ = 255;
                uOffset -= 255;
            }
            *pszOutPtr++ = uOffset;
        }
        ucByteCount++;
        size -= (ucByteCount);
        memcpy (pszOutPtr, pszCurPtr, ucByteCount);
        pszOutPtr += ucByteCount;
    }

    compressedsize = pszOutPtr - compressBuf;
    memcpy (SeedRow, input->rasterdata[myplane], uOrgSize);
    seeded = true;
    iRastersReady = 1;
    return true;
}

bool Mode3::NextOutputRaster (RASTERDATA& next_raster)
{
	if (iRastersReady == 0)
	{
        return false;
    }

    if (myplane == COLORTYPE_COLOR && compressedsize != 0)
    {
        next_raster.rastersize[COLORTYPE_COLOR] = compressedsize;
        next_raster.rasterdata[COLORTYPE_COLOR] = compressBuf;
    }
    else
    {
        next_raster.rastersize[COLORTYPE_COLOR] = 0;
        next_raster.rasterdata[COLORTYPE_COLOR] = raster.rasterdata[COLORTYPE_COLOR];
    }

    next_raster.rastersize[COLORTYPE_BLACK] = raster.rastersize[COLORTYPE_BLACK];
    next_raster.rasterdata[COLORTYPE_BLACK] = raster.rasterdata[COLORTYPE_BLACK];

    iRastersReady = 0;
    return true;
}

