/*****************************************************************************\
  scaler_open.cpp : Implimentation for the Scaler_Open class

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
#include "scaler_open.h"


APDK_BEGIN_NAMESPACE

Scaler_Open::Scaler_Open(SystemServices* pSys,int inputwidth,
                         int numerator,int denominator,
                         BOOL bVIP, unsigned int iNumInks)
    : Scaler(pSys,inputwidth,numerator,denominator,bVIP, iNumInks),
        NumInks(iNumInks), vip(bVIP)
{
    rowremainder = remainder;
}

Scaler_Open::~Scaler_Open()
{ }

BOOL Scaler_Open::Process(RASTERDATA* raster_in)
{
    iRastersDelivered=0;

    if (raster_in == NULL || (raster_in->rasterdata[COLORTYPE_COLOR] == NULL && raster_in->rasterdata[COLORTYPE_BLACK] == NULL))
    {
        rowremainder=remainder;
        return FALSE;
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
		return TRUE;
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
 return TRUE;
}


BYTE* Scaler_Open::NextOutputRaster(COLORTYPE color)
{
    if (iRastersReady==0)
        return NULL;

	if (myplane == COLORTYPE_BLACK)
	{
		if (raster.rasterdata[myplane] == NULL)
			iRastersReady = 1;
	}
	if (color == COLORTYPE_COLOR)
	{
		iRastersReady--; iRastersDelivered++;
	}
	if (raster.rastersize[color] > 0)
	{
		return pOutputBuffer[color];
	}
	else
	{
		return NULL;
	}
}

APDK_END_NAMESPACE

