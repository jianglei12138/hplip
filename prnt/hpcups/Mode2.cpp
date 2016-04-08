/*****************************************************************************\
  Mode2.cpp : implementaiton of Mode2 class

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
#include "Mode2.h"

Mode2::Mode2 (unsigned int RasterSize)
    : Compressor (RasterSize, false)
{
    compressBuf = (BYTE *) new BYTE[RasterSize];
        if (compressBuf == NULL)
            constructor_error = ALLOCMEM_ERROR;
}

Mode2::~Mode2()
{ }

bool Mode2::Process (RASTERDATA* input)
{
    BYTE* pDst = compressBuf;
    int ndstcount = 0;

    if (input==NULL || 
        (myplane == COLORTYPE_COLOR && input->rasterdata[COLORTYPE_COLOR] == NULL) ||
        (myplane == COLORTYPE_BLACK && input->rasterdata[COLORTYPE_BLACK] == NULL))    // flushing pipeline
    {
        compressedsize=0;

        iRastersReady=0;
        return false;
    }

    unsigned int size = input->rastersize[myplane];

    for (unsigned int ni = 0; ni < size;)
    {
        if ( ni + 1 < size && input->rasterdata[myplane][ ni ] == input->rasterdata[myplane][ ni + 1 ] )
        {
            unsigned int nrepeatcount;
            for ( ni += 2, nrepeatcount = 1; ni < size && nrepeatcount < 127; ++ni, ++nrepeatcount )
            {
                if ( input->rasterdata[myplane][ ni ] != input->rasterdata[myplane][ ni - 1 ] )
                {
                    break;
                }
            }
            int tmprepeat = 0 - nrepeatcount;
            BYTE trunc = (BYTE) tmprepeat;
            pDst[ ndstcount++ ] = trunc;
            pDst[ ndstcount++ ] = input->rasterdata[myplane][ ni - 1 ];
        }
        else
        {
            int nliteralcount;
            int nfirst = ni;
            for ( ++ni, nliteralcount = 0; ni < size && nliteralcount < 127; ++ni, ++nliteralcount )
            {
                if ( input->rasterdata[myplane][ ni ] == input->rasterdata[myplane][ ni - 1 ] )
                {
                    --ni;
                    --nliteralcount;
                    break;
                }
            }
            pDst[ ndstcount++ ] = (BYTE) nliteralcount;
            for ( int nj = 0; nj <= nliteralcount; ++nj )
            {
                pDst[ ndstcount++ ] = input->rasterdata[myplane][ nfirst++ ];
            }
        }
    }

    size = ndstcount;
    compressedsize = size;
    iRastersReady = 1;
    return true;
}

bool Mode2::NextOutputRaster(RASTERDATA& next_raster)
{
    if (iRastersReady==0){
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

    if (myplane == COLORTYPE_BLACK && compressedsize != 0)
    {
        next_raster.rastersize[COLORTYPE_BLACK] = compressedsize;
        next_raster.rasterdata[COLORTYPE_BLACK] = compressBuf;
    }
    else
    {
        next_raster.rastersize[COLORTYPE_BLACK] = 0;
        next_raster.rasterdata[COLORTYPE_BLACK] = raster.rasterdata[COLORTYPE_BLACK];
    }

    iRastersReady=0;
    return true;
}

