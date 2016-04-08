/*****************************************************************************\
  Scaler.cpp : Implimentation for the Scaler class

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
#include "Scaler.h"

#define MAX_OUTPUT_RASTERS 32

Scaler::Scaler(unsigned int inputwidth, unsigned int numerator,
               unsigned int denominator, bool bVIP, unsigned int BytesPerPixel,
               unsigned int iNumInks)
{

    iInputWidth = inputwidth;
    rowremainder = remainder;
    NumInks = iNumInks;
    vip = bVIP;

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

    iOutputWidth = (int) (((float)iInputWidth / (float)denominator) *
                          (float)numerator);
    iOutputWidth++;         // safety measure to protect against roundoff error

    if (numerator == denominator)
        scaling = false;
    else scaling = true;

    // ScaleBound=max number of output rows per input row;
    // i.e., if scale=4.28, then sometimes 5 rows will come out

    int ScaleBound = int(ScaleFactor);
    if  (ScaleFactor > (float) ScaleBound)
        ScaleBound++;

    // allocate a buffer for one output row
    int RSBuffSize = (int) (((float)(BytesPerPixel*iOutputWidth)) * ScaleBound );
    pOutputBuffer[COLORTYPE_COLOR] = (BYTE *) new BYTE[RSBuffSize];
    if (pOutputBuffer[COLORTYPE_COLOR] == NULL)
    {
        constructor_error = ALLOCMEM_ERROR;
        return;
    }
    int BlackBuffSize = (int) (((float) (iOutputWidth)) * ScaleBound );
    pOutputBuffer[COLORTYPE_BLACK] = (BYTE*) new BYTE[BlackBuffSize];
    if (pOutputBuffer[COLORTYPE_BLACK] == NULL)
    {
        constructor_error = ALLOCMEM_ERROR;
        return;
    }

    if (ScaleFactor < 2.0)
        ReplicateOnly = true;
    else
        ReplicateOnly = false;

    if (ScaleFactor > (float) factor)
        fractional = true;
    else fractional = false;
}

Scaler::~Scaler ()
{
    for (int i = COLORTYPE_COLOR; i < MAX_COLORTYPE; i++)
    {
        if (pOutputBuffer[i]) 
        {
            delete [] pOutputBuffer[i];
            pOutputBuffer[i] = NULL;
        }
    }
}

unsigned int Scaler::GetMaxOutputWidth()
{
	if (myplane == COLORTYPE_COLOR)
	{
		return (iOutputWidth-1)*NUMBER_PLANES;  // we padded it in case of roundoff error
	}
	else
	{	
		return (iOutputWidth-1);  // we padded it in case of roundoff error
	}
}

bool Scaler::Process(RASTERDATA* raster_in)
{
    iRastersDelivered=0;

    if (raster_in == NULL || (raster_in->rasterdata[COLORTYPE_COLOR] == NULL && raster_in->rasterdata[COLORTYPE_BLACK] == NULL))
    {
        rowremainder=remainder;
        return false;
    }

    if (!scaling)
	{
		// just copy both to output buffer
		for (int i = COLORTYPE_COLOR; i < MAX_COLORTYPE; i++)
		{
			if (raster_in->rasterdata[i])
			{
				memcpy(pOutputBuffer[i], raster_in->rasterdata[i], raster_in->rastersize[i]);
			}
		}
		iRastersReady = 1;
		return true;
    }

	if (myplane == COLORTYPE_COLOR)
	{
		if (raster_in->rasterdata[COLORTYPE_BLACK])
		{
			memcpy(pOutputBuffer[COLORTYPE_BLACK], raster_in->rasterdata[COLORTYPE_BLACK], raster_in->rastersize[COLORTYPE_BLACK]);
		}
	}

	if (myplane == COLORTYPE_BLACK)
	{
		if (raster_in->rasterdata[COLORTYPE_COLOR])
		{
			memcpy(pOutputBuffer[COLORTYPE_COLOR], raster_in->rasterdata[COLORTYPE_COLOR], raster_in->rastersize[COLORTYPE_COLOR]);
		}
	}

    // multiply row
    unsigned int ifactor = (unsigned int) (int) ScaleFactor;
    unsigned int targptr=0;
    unsigned int sourceptr=0;
    unsigned int rem = remainder;

	if (myplane == COLORTYPE_COLOR || myplane == COLORTYPE_BOTH)
	{
		if (raster_in->rasterdata[COLORTYPE_COLOR])
		{
			if (vip)                   // RGB values interleaved
			{
				unsigned int width = iInputWidth*3;
				for (unsigned int i=0; i < width; i += 3)
				{
					unsigned int factor = ifactor;
					if (rem >= 1000)
					{
						factor++;
						rem -= 1000;
					}
					for (unsigned int j=0; j < factor; j++)
					{
						pOutputBuffer[COLORTYPE_COLOR][targptr++] = raster_in->rasterdata[COLORTYPE_COLOR][i];
						pOutputBuffer[COLORTYPE_COLOR][targptr++] = raster_in->rasterdata[COLORTYPE_COLOR][i+1];
						pOutputBuffer[COLORTYPE_COLOR][targptr++] = raster_in->rasterdata[COLORTYPE_COLOR][i+2];
					}
					rem += remainder;
				}
			}
			else            // KCMY values NOT interleaved
							// iInputWidth = bytes per plane
			for (unsigned int i=0; i < NumInks; i++)  // loop over planes
			{
				unsigned int planecount=0;          // count output bytes for this plane
				unsigned int src=0;                 // count input bytes for this plane
				while ((planecount < iOutputWidth-1) && (src < iInputWidth))
				{
					unsigned int factor = ifactor;
					if (rem >= 1000)
					{
						factor++;
						rem -= 1000;
					}
					for (unsigned int j=0; (j < factor) && (planecount < iOutputWidth-1); j++)
					{
						pOutputBuffer[COLORTYPE_COLOR][targptr++] = raster_in->rasterdata[COLORTYPE_COLOR][sourceptr];
						planecount++;
					}
					rem += remainder;
					sourceptr++; src++;
				}
				while (planecount < iOutputWidth-1)     // fill out odd bytes so all planes are equal
				{
					pOutputBuffer[COLORTYPE_COLOR][targptr++] = raster_in->rasterdata[COLORTYPE_COLOR][sourceptr-1];
					planecount++;
				}

			}
		}
	}

	ifactor = (unsigned int) (int) ScaleFactor;
    targptr=0;
    sourceptr=0;
    rem = remainder;

	if (myplane == COLORTYPE_BLACK || myplane == COLORTYPE_BOTH)
	{	
		if (raster_in->rasterdata[COLORTYPE_BLACK])
		{	
			// K values NOT interleaved
			// iInputWidth = bytes per plane
			unsigned int planecount=0;          // count output bytes for this plane
			unsigned int src=0;                 // count input bytes for this plane
			while ((planecount < iOutputWidth-1) && (src < iInputWidth))
			{
				unsigned int factor = ifactor;
				if (rem >= 1000)
				{
					factor++;
					rem -= 1000;
				}
				for (unsigned int j=0; (j < factor) && (planecount < iOutputWidth-1); j++)
				{
					pOutputBuffer[COLORTYPE_BLACK][targptr++] = raster_in->rasterdata[COLORTYPE_BLACK][sourceptr];
					planecount++;
				}
				rem += remainder;
				sourceptr++; src++;
			}
			while (planecount < iOutputWidth-1)     // fill out odd bytes so all planes are equal
			{
				pOutputBuffer[COLORTYPE_BLACK][targptr++] = raster_in->rasterdata[COLORTYPE_BLACK][sourceptr-1];
				planecount++;
			}
		}
	}

    unsigned int factor = ifactor;
    if (rowremainder >= 1000)
    {
        factor++;
        rowremainder -= 1000;
    }
	if (myplane == COLORTYPE_BLACK && raster_in->rasterdata[COLORTYPE_COLOR])
		iRastersReady = 1;
	else
		iRastersReady=factor;
    iRastersDelivered=0;
    rowremainder += remainder;
 return true;
}

bool Scaler::NextOutputRaster(RASTERDATA &next_raster)
{
    if (iRastersReady == 0)
        return false;

    if (myplane == COLORTYPE_BLACK)
    {
        if (raster.rasterdata[COLORTYPE_BLACK] == NULL)
            iRastersReady = 1;
    }
    if (myplane == COLORTYPE_COLOR)
    {
        iRastersReady--;
	iRastersDelivered++;
    }
    bool    bval = false;
    if (raster.rastersize[COLORTYPE_BLACK] > 0)
    {
        next_raster.rastersize[COLORTYPE_BLACK] = raster.rastersize[COLORTYPE_BLACK];
        next_raster.rasterdata[COLORTYPE_BLACK] = pOutputBuffer[COLORTYPE_BLACK];
        bval = true;
    }
    else
    {
        next_raster.rastersize[COLORTYPE_BLACK] = 0;
        next_raster.rasterdata[COLORTYPE_BLACK] = NULL;
    }
    if (raster.rastersize[COLORTYPE_COLOR] > 0)
    {
        next_raster.rastersize[COLORTYPE_COLOR] = raster.rastersize[COLORTYPE_COLOR];
        next_raster.rasterdata[COLORTYPE_COLOR] = pOutputBuffer[COLORTYPE_COLOR];
        bval = true;
    }
    else
    {
        next_raster.rastersize[COLORTYPE_COLOR] = 0;
        next_raster.rasterdata[COLORTYPE_COLOR] = NULL;
    }
    return bval;
}

