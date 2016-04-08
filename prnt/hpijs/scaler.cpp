/*****************************************************************************\
  scaler.cpp : Implimentation for the Scaler class

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

APDK_BEGIN_NAMESPACE

#define MAX_OUTPUT_RASTERS 32


Scaler::Scaler(SystemServices* pSys,unsigned int inputwidth,
               unsigned int numerator,unsigned int denominator, BOOL vip, unsigned int BytesPerPixel)
        : pSS(pSys), iInputWidth(inputwidth)
{

    ASSERT(denominator > 0);

    constructor_error=NO_ERROR;
    for (int i = COLORTYPE_COLOR; i < MAX_COLORTYPE; i++)
    {
        pOutputBuffer[i] = NULL;
    }

    ScaleFactor= (float)numerator / (float)denominator;
    if (ScaleFactor > (float)MAX_OUTPUT_RASTERS)      //
    {
        constructor_error = INDEX_OUT_OF_RANGE;
        return;
    }
    int factor = (int)ScaleFactor;
    float rem = ScaleFactor - (float)factor;
    rem *= 1000;
    remainder = (int)rem;

    iOutputWidth = (int) (((float) iInputWidth / (float) denominator) *
                          (float) numerator);
    iOutputWidth++;         // safety measure to protect against roundoff error

    if (numerator == denominator)
        scaling=FALSE;
    else scaling=TRUE;


    // ScaleBound=max number of output rows per input row;
    // i.e., if scale=4.28, then sometimes 5 rows will come out

    int ScaleBound = int(ScaleFactor);
    if  (ScaleFactor > (float) ScaleBound)
        ScaleBound++;

    // allocate a buffer for one output row
    int RSBuffSize= (int)(((float)(BytesPerPixel*iOutputWidth)) * ScaleBound );
    pOutputBuffer[COLORTYPE_COLOR]=(BYTE*)pSS->AllocMem(RSBuffSize);
    if (pOutputBuffer[COLORTYPE_COLOR] == NULL)
    {
        constructor_error=ALLOCMEM_ERROR;
        return;
    }

//  Initialize RGB buffer to white
    if (vip)
    {
        memset(pOutputBuffer[COLORTYPE_COLOR], 0xFF, RSBuffSize);
    }

	int BlackBuffSize= (int)(((float)(iOutputWidth)) * ScaleBound );
    pOutputBuffer[COLORTYPE_BLACK]=(BYTE*)pSS->AllocMem(BlackBuffSize);
    if (pOutputBuffer[COLORTYPE_BLACK] == NULL)
    {
        constructor_error=ALLOCMEM_ERROR;
        return;
    }

    if (ScaleFactor < 2.0)
        ReplicateOnly = TRUE;
    else ReplicateOnly=FALSE;

    if (ScaleFactor > (float)factor)
        fractional=TRUE;
    else fractional=FALSE;
}

Scaler::~Scaler()
{
	for (int i = COLORTYPE_COLOR; i < MAX_COLORTYPE; i++)
	{
		if (pOutputBuffer[i]) 
		{
			pSS->FreeMemory(pOutputBuffer[i]);
			pOutputBuffer[i] = NULL;
		}
	}
}

unsigned int Scaler::GetMaxOutputWidth(COLORTYPE  rastercolor)
{
	if (rastercolor == COLORTYPE_COLOR)
	{
		return (iOutputWidth-1)*NUMBER_PLANES;  // we padded it in case of roundoff error
	}
	else
	{	
		return (iOutputWidth-1);  // we padded it in case of roundoff error
	}
}

unsigned int Scaler::GetOutputWidth(COLORTYPE color)
{
	if (color == COLORTYPE_COLOR)
	{
		if (raster.rasterdata[color])
			if (myplane == COLORTYPE_BLACK)
				return raster.rastersize[color];
			else
				return (iOutputWidth-1)*NUMBER_PLANES;  // we padded it in case of roundoff error
		else
			return 0;
	}
	else
	{	
		if (raster.rasterdata[color])
			if (myplane == COLORTYPE_COLOR)
				return raster.rastersize[color];
			else
				return (iOutputWidth-1);  // we padded it in case of roundoff error
		else 
			return 0;
	}
}

APDK_END_NAMESPACE

