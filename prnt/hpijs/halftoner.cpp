/*****************************************************************************\
  halftoner.cpp : Implimentation for the Halftoner class

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


//===========================================================================
//
//  Filename     :  halftoner.cpp
//
//  Module       :  Open Source Imaging
//
//  Description  :  This file contains the constructor and destructor for
//                  the Halftoner class, which performs color-matching and
//                  halftoning.
//
// Detailed Description:
//
// The only member functions needed are Process(inputRaster)
// and Restart (used to skip white space and for new page).
//
// Configurability required in Slimhost driver is reflected in the
// parameters to the constructor:
// 1. SystemServices encapsulates memory management for platform-independence
// 2. PrintMode contains info on resolution and other properties
// 3. iInputWidth tells how many pixels input per plane
// 4. iNumRows is 1 except for mixed-resolution cases
// 5. HiResFactor is for boosting base resolution, e.g. 2 if 600 dpi grid
//      (base res assumed to be 300)
//
// These structures, together with the variable StartPlane (designating
// K or C in the fixed ordering KCMY), are accessed by the Translator
// component of the driver, in order to properly package the data in
// the printer command language.
//============================================================================

#include "header.h"
#include "hptypes.h"
#include "halftoner.h"

APDK_BEGIN_NAMESPACE

Halftoner::Halftoner
(
    SystemServices* pSys,
    PrintMode* pPM,
    unsigned int iInputWidth,
    unsigned int iNumRows[],
    unsigned int HiResFactor,
    BOOL matrixbased
) : ColorPlaneCount(pPM->dyeCount),
    InputWidth(iInputWidth),
    ResBoost(HiResFactor),
    pSS(pSys),
    nNextRaster(0),
    fBlackFEDResPtr(pPM->BlackFEDTable),
    fColorFEDResPtr(pPM->ColorFEDTable),
    iColor(0),
    iRow(0),
    iPlane(0),
    tempBuffer(NULL),
    tempBuffer2(NULL),
    hold_random(0),
    usematrix(matrixbased)
{
    unsigned int i;
    int j,k,PlaneSize;
    constructor_error = NO_ERROR;

    StartPlane = K;       // most common case

    if (ColorPlaneCount == 3)     // CMY pen
      {
        StartPlane=C;
        NumRows[K] = ColorDepth[K] = OutputWidth[K] = 0;
      }

    EndPlane=Y;         // most common case
    if (ColorPlaneCount == 6)
    {
        EndPlane = Mlight;
    }
    if (ColorPlaneCount == 1)
    {
        EndPlane = K;
    }

    AdjustedInputWidth = InputWidth;
    if (AdjustedInputWidth % 8)
    {
        AdjustedInputWidth += 8 - (AdjustedInputWidth % 8);
    }

    // init arrays
    for (i = StartPlane; i < (ColorPlaneCount + StartPlane); i++)
    {
        ColorDepth[i]= pPM->ColorDepth[i];
        NumRows[i]=iNumRows[i];

        OutputWidth[i] = AdjustedInputWidth * NumRows[i] * ResBoost;

    }
    for (;i < (unsigned)MAXCOLORPLANES; i++)
    {
        NumRows[i] = ColorDepth[i] = OutputWidth[i] = 0;
    }

    oddbits = AdjustedInputWidth - InputWidth;
    ///////////////////////////////////////////////////////////////////////////
    for (i=0; i <= Mlight; i++)
    {
        ErrBuff[i]=NULL;
    }

    for (i=StartPlane; i <= EndPlane; i++)
    {
        ErrBuff[i] = (short*)pSS->AllocMem((OutputWidth[i] + 2) * sizeof(short));
        if (ErrBuff[i] == NULL)
        {
            goto MemoryError;
        }
    }

    if (OutputWidth[K] > AdjustedInputWidth)
    // need to expand input data (easier than expanding bit-pixels after) on K row
    {
        tempBuffer = (BYTE*) pSS->AllocMem(OutputWidth[K]);
        if (tempBuffer == NULL)
        {
            goto MemoryError;
        }
        if (EndPlane > Y)
        {
            tempBuffer2 = (BYTE*) pSS->AllocMem(OutputWidth[K]);
            if (tempBuffer2 == NULL)
            {
                goto MemoryError;
            }
        }

    }


    Restart();  // this zeroes buffers and sets nextraster counter

    // allocate output buffers
    for (i = 0; i < (unsigned)MAXCOLORPLANES; i++)
    {
        for (j = 0; j < MAXCOLORROWS; j++)
        {
            for (k = 0; k < MAXCOLORDEPTH; k++)
            {
                ColorPlane[i][j][k] = NULL;
            }
        }
    }

    for (i=StartPlane; i < (ColorPlaneCount+StartPlane); i++)
    {
        for (j=0; j < NumRows[i]; j++)
        {
            for (k=0; k < ColorDepth[i]; k++)
            {
                PlaneSize= OutputWidth[i]/8 + // doublecheck ... should already be divisble by 8
                            ((OutputWidth[i] % 8)!=0);
                ColorPlane[i][j][k] = (BYTE*) pSS->AllocMem(PlaneSize);
                if (ColorPlane[i][j] == NULL)
                {
                    goto MemoryError;
                }
                memset(ColorPlane[i][j][k], 0, PlaneSize);
            }
        }
    }

    PlaneSize = (OutputWidth[0] + 7) / 8;
    if (PlaneSize > 0)
    {
        originalKData = (BYTE*) pSS->AllocMem(PlaneSize);
        if (originalKData == NULL)
        {
            goto MemoryError;
        }
        memset(originalKData, 0, PlaneSize);
    }
    return;

MemoryError:
    constructor_error=ALLOCMEM_ERROR;

    FreeBuffers();

    for (i=0; i < ColorPlaneCount; i++)
    {
        for (j=0; j < NumRows[i]; j++)
        {
            for (k=0; k < ColorDepth[i]; k++)
            {
                if (ColorPlane[i][j][k])
                {
                    pSS->FreeMemory(ColorPlane[i][j][k]);
                }
            }
        }
    }
	if (originalKData)
	{
		pSS->FreeMemory(originalKData);
	}

} //Halftoner


Halftoner::~Halftoner()
{
    DBG1("destroying Halftoner \n");

    FreeBuffers();

    for (int i=0; i < MAXCOLORPLANES; i++)
    {
        for (int j=0; j < NumRows[i]; j++)
        {
            for (int k=0; k < ColorDepth[i]; k++)
            {
                if (ColorPlane[i][j][k])
                {
                    pSS->FreeMemory(ColorPlane[i][j][k]);
                }
            }
        }
    }
	if (originalKData)
	{
		pSS->FreeMemory(originalKData);
	}
} //~Halftoner


void Halftoner::Restart()
{
    nNextRaster = 0;

    for (unsigned int i = StartPlane; i <= EndPlane; i++)
      {
        memset(ErrBuff[i], 0, (OutputWidth[i]+2) * sizeof(short));
      }

    started = FALSE;
} //Restart


void Halftoner::Flush()
{
    if (!started)
    {
        return;
    }
    Restart();
} //Flush


void Halftoner::FreeBuffers()
{
    for (unsigned int i = StartPlane; i <= EndPlane; i++)
      {
        pSS->FreeMemory(ErrBuff[i]);
      }
    if (tempBuffer)
    {
        pSS->FreeMemory(tempBuffer);
    }
    if (tempBuffer2)
    {
        pSS->FreeMemory(tempBuffer2);
    }
} //FreeBuffers


// dumb horizontal doubling (tripling, etc.) for resolution-boost prior to halftoning
void Halftoner::PixelMultiply(unsigned char* buffer, unsigned int width, unsigned int factor)
{
    if (factor == 1)
    {
        return;
    }

    for (int j = (int)(width-1); j >= 0; j--)
    {
        unsigned int iOffset = j * factor;
        for (unsigned int k = 0; k < factor; k++)
        {
            buffer[iOffset + k] = buffer[j];
        }
    }

} //PixelMultiply


BYTE* Halftoner::NextOutputRaster(COLORTYPE  rastercolor)
{
	if (rastercolor == COLORTYPE_COLOR)
	{
		if (iRastersReady == 0)
		{
			return NULL;
		}

		if (iColor == (ColorPlaneCount+StartPlane))
		{
			return NULL;
		}

		if (iPlane == ColorDepth[iColor])
		{
			iPlane = 0;
			iRow++;
			return NextOutputRaster(rastercolor);
		}

		if (iRow == NumRows[iColor])
		{
			iRow = 0;
			iColor++;
			return NextOutputRaster(rastercolor);
		}

		iRastersDelivered++;
		iRastersReady--;
		return ColorPlane[iColor][iRow][iPlane++];
	}
	else
	{
		return NULL;
	}
} //NextOutputRaster


BOOL Halftoner::LastPlane()
{
      return ((iColor == (ColorPlaneCount+StartPlane-1)) &&
              (iRow == (unsigned int)(NumRows[iColor] - 1)) &&
              (iPlane == ColorDepth[iColor])        // was pre-incremented
              );
} //LastPlane


BOOL Halftoner::FirstPlane()
{
      return ((iColor == StartPlane) &&
              (iRow == 0) &&
              (iPlane == 1)        // was pre-incremented
              );
} //FirstPlane


unsigned int Halftoner::GetOutputWidth(COLORTYPE  rastercolor)
// return size of data in the plane being delivered (depends on iRastersDelivered)
// (will be used in connection with compression seedrow)
{
	if (rastercolor == COLORTYPE_COLOR)
	{
		unsigned int colorplane, tmp;
		// figure out which colorplane we're on
		unsigned int rasterd = iRastersDelivered;
		// we come after increment of iRastersDelivered
		if (rasterd>0)
		{
			rasterd--;
		}

		tmp = (unsigned int)(NumRows[0]*ColorDepth[0]);
		if (rasterd < tmp)
		{
			colorplane = 0;
		}
		// have to count up to possible 6th plane;
		// but we'll save code by assuming sizes of C,M,Y (Cl,Ml) are all same
		else
		{
			colorplane = 1;
		}

		int temp = (OutputWidth[colorplane] + 7) / 8;
		return temp;
	}
	else
	{
		return 0;
	}
} //GetOutputWidth


unsigned int Halftoner::GetMaxOutputWidth(COLORTYPE  rastercolor)
// This is needed by Configure, since the output-width for Halftoner is variable
// depending on the colorplane
{
	if (rastercolor == COLORTYPE_COLOR)
	{
		unsigned int max=0;
		for (unsigned int i=StartPlane; i <= EndPlane; i++)
		{
			if (OutputWidth[i] > max)
			{
				max = OutputWidth[i];
			}
		}
		return (max / 8) + ((max % 8)!=0);
	}
	else
	{
		return 0;
	}
} //GetMaxOutputWidth


unsigned int Halftoner::PlaneCount()
{
 unsigned int count=0;

     for (int i = 0; i < MAXCOLORPLANES; i++)
     {
         count += NumRows[i] * ColorDepth[i];
     }

  return count;
} //PlaneCount


void Halftoner::CleanOddBits(unsigned int iColor, unsigned int iRow)
{
    int index = (OutputWidth[iColor]/8)-1;

    for (int i=0; i < ColorDepth[iColor]; i++)
    {
        BYTE lastbyte0 = ColorPlane[iColor][iRow][i][index];
        lastbyte0 = lastbyte0 >> oddbits;
        lastbyte0 = lastbyte0 << oddbits;
        ColorPlane[iColor][iRow][i][index] = lastbyte0;
    }
} //CleanOddBits

APDK_END_NAMESPACE

