/*****************************************************************************\
  halftoner_open.cpp : Open Source Imaging Halftoning

  Copyright (c) 1994 - 2015, HP Co.
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
#include "halftoner.h"
#include "halftoner_open.h"

APDK_BEGIN_NAMESPACE

extern unsigned char BayerMatrix[];

Halftoner_Open::Halftoner_Open
(
    SystemServices* pSys,
    PrintMode* pPM,
    unsigned int iInputWidth,
    unsigned int iNumRows[],
    unsigned int HiResFactor,
    BOOL matrixbased
)
    :  Halftoner(pSys,pPM,iInputWidth,iNumRows,HiResFactor,matrixbased)
{  }


Halftoner_Open::~Halftoner_Open()
{ }


BOOL Halftoner_Open::Process
(
    RASTERDATA* pbyInputKRGBRaster
)
{
    unsigned int i;
    int j, k;

    if ((pbyInputKRGBRaster == NULL) ||
       (pbyInputKRGBRaster && pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR]==NULL && pbyInputKRGBRaster->rasterdata[COLORTYPE_BLACK]==NULL))
    {
        Restart();
        return FALSE;   // no output
    }
    started=TRUE;

    for (i=StartPlane; i < (ColorPlaneCount+StartPlane); i++)
    {
        for (j=0; j < NumRows[i]; j++)
        {
            for (k=0; k < ColorDepth[i]; k++)
            {
                int PlaneSize= (OutputWidth[i] + 7) / 8;
                memset(ColorPlane[i][j][k], 0, PlaneSize);
            }
        }
    }
	if (pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR])
	{
		// increment current raster
		++nNextRaster;
		if ( -1 == nNextRaster )
			nNextRaster = 0;

		fRasterOdd        = ( 1 & nNextRaster ) ? 0 : 1;

		BYTE* input;
		unsigned int numpix;

		for (i=StartPlane; i <= EndPlane; i++)
		{
			if (OutputWidth[i] > AdjustedInputWidth)
			{
				memset(tempBuffer, 0, OutputWidth[i]);  // clear it out because outwidth might be
														// > factor*inwidth due to roundoff
				memcpy(tempBuffer,pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR],InputWidth);
				int factor = NumRows[i] * ResBoost;
				PixelMultiply(tempBuffer, InputWidth, factor);
				input=tempBuffer;
				numpix = OutputWidth[i];
			}
			else
			{
				input=pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR];
				numpix = AdjustedInputWidth;
			}

			fDitherParms[i].fNumPix = numpix;
			fDitherParms[i].fInput = input;
			fDitherParms[i].fErr = ErrBuff[i];
			fDitherParms[i].fErr++; // This is for serpentine
			fDitherParms[i].fSymmetricFlag = HPTRUE;   // Symmetric only
			if (i == K)
				fDitherParms[i].fFEDResPtr = fBlackFEDResPtr;
			else
				fDitherParms[i].fFEDResPtr = fColorFEDResPtr;
			fDitherParms[i].fRasterEvenOrOdd = fRasterOdd;
			fDitherParms[i].fHifipe = ColorDepth[i]>1;

			// for matrix //////////////
			if (usematrix)
			{
				fDitherParms[i].fSqueezeOffset=0;
				fDitherParms[i].fMatrixRowSize = BayerMatrix[1];
				BYTE colorindex = i + 2;
				if (i>=Y)
					colorindex = 4;
				BYTE ditheroffset = BayerMatrix[colorindex];
				BYTE matrixrowsize = BayerMatrix[1];
				fDitherParms[i].fDitherCellOffset = ditheroffset;
				HPBytePtr matrixptr = (HPBytePtr)(
					(((ditheroffset + nNextRaster) % matrixrowsize) * matrixrowsize)
						+ 5 + BayerMatrix);

				fDitherParms[i].fMatrixV1 = matrixptr;
			}

			////////////////////////////////////

			for (j=0; j < NumRows[i]; j++)
			{
				fDitherParms[i].fOutput1 = ColorPlane[i][j][0];
				fDitherParms[i].fOutput2 = ColorPlane[i][j][1];

				if (usematrix)
				{
					memset(fDitherParms[i].fOutput1, 0, OutputWidth[i]/8);
					if (fDitherParms[i].fOutput2)
						memset(fDitherParms[i].fOutput2, 0, OutputWidth[i]/8);
					HTMATRIXHI_KCMY((THTDitherParmsPtr) fDitherParms, i);
				}
				else HTEDiffOpen   ((THTDitherParmsPtr) fDitherParms, i);


				// cleanup bits at end of row due to input-width not being divisible by 8
				CleanOddBits(i,j);
			}

			pbyInputKRGBRaster->rasterdata[COLORTYPE_COLOR] += InputWidth;

		}
	}

	if (pbyInputKRGBRaster->rasterdata[COLORTYPE_BLACK])
	{
		int factor = 1;
		if (OutputWidth[K] > AdjustedInputWidth)
		{
			memset(tempBuffer, 0, OutputWidth[K]);
			memcpy(tempBuffer,pbyInputKRGBRaster->rasterdata[COLORTYPE_BLACK],pbyInputKRGBRaster->rastersize[COLORTYPE_BLACK]);

			factor = NumRows[K] * ResBoost;
			PixelMultiply(tempBuffer, pbyInputKRGBRaster->rastersize[COLORTYPE_BLACK], factor);
		}

		//  Convert 8bit per pixel data into 1 bit per pixel data
		memset(originalKData, 0, (pbyInputKRGBRaster->rastersize[COLORTYPE_BLACK] * factor +7)/8);
		int curBit = 0x80, curByte = 0;
		for (int i=0; i<pbyInputKRGBRaster->rastersize[COLORTYPE_BLACK] * factor; i++)
		{
			if (OutputWidth[K] > AdjustedInputWidth)
			{
				if (tempBuffer[i])
				{
					originalKData[curByte] |= curBit;
				}
			}
			else
			{
				if (pbyInputKRGBRaster->rasterdata[COLORTYPE_BLACK][i])
				{
					originalKData[curByte] |= curBit;
				}
			}
			if (curBit == 0x01)
			{
				curByte++;
				curBit = 0x80;
			}
			else
			{
				curBit = curBit >> 1;
			}
		}
		for (j=0; j < NumRows[K]; j++)
		{
			for (k = 0; k < (pbyInputKRGBRaster->rastersize[COLORTYPE_BLACK] * factor +7)/8; k++)
				if (ColorPlane[K][j][0])
					ColorPlane[K][j][0][k] |= originalKData[k];
				if (ColorPlane[K][j][1])
					ColorPlane[K][j][1][k] |= originalKData[k];
		}
	}

	iColor = StartPlane;
	iRow = iPlane = 0;
	iRastersReady = PlaneCount();
	iRastersDelivered = 0;
	return TRUE;   // one raster in, one raster out
} //Process


void Halftoner_Open::HTEDiffOpen
(
    THTDitherParmsPtr ditherParmsPtr,
    HPUInt16 count
)
{


    ditherParms = ditherParmsPtr+count;
    errPtr = ditherParms->fErr;
    numLoop = ditherParms->fNumPix;
    inputPtr = ditherParms->fInput;
    fedResTbl = ditherParms->fFEDResPtr;
    symmetricFlag = ditherParms->fSymmetricFlag;
    doNext8Pixels = HPTRUE;
    hifipe = ditherParms->fHifipe;
    outputPtr1 = ditherParms->fOutput1;

    outputPtr1 = ditherParms->fOutput1;

    if (hifipe)
    {
        outputPtr1 = ditherParms->fOutput1;
        outputPtr2 = ditherParms->fOutput2;
        outputPtr3 = ditherParms->fOutput3;
    }

    diffusionErrorPtr = errPtr;

    rasterByte1 = 0;
    rasterByte2 = 0;
    rasterByte3 = 0;

    if( ditherParms->fRasterEvenOrOdd == 1 )
    {
        tmpShortStore = *diffusionErrorPtr;

        *diffusionErrorPtr = 0;

        for (pixelCount = numLoop + 8; (pixelCount -= 8) > 0; )
        {
            if (pixelCount > 16) // if next 16 pixels are white, skip 8
            {
                doNext8Pixels = Forward16PixelsNonWhite(inputPtr);
            }
            else
            {
                doNext8Pixels = HPTRUE;
            }

            if (doNext8Pixels)
            {
                thValue = HPRand();
                FORWARD_FED( thValue, 0x80 );
                thValue = HPRand();
                FORWARD_FED( thValue, 0x40 );
                thValue = HPRand();
                FORWARD_FED( thValue, 0x20 );
                thValue = HPRand();
                FORWARD_FED( thValue, 0x10 );
                thValue = HPRand();
                FORWARD_FED( thValue, 0x08 );
                thValue = HPRand();
                FORWARD_FED( thValue, 0x04 );
                thValue = HPRand();
                FORWARD_FED( thValue, 0x02 );
                thValue = HPRand();
                FORWARD_FED( thValue, 0x01 );

                *outputPtr1++ = rasterByte1;
                rasterByte1 = 0;

                if (hifipe)
                {
                    *outputPtr2++ = rasterByte2;
                    rasterByte2 = 0;
                }
            }
            else  // Do white space skipping
            {
                inputPtr += 8;
                *outputPtr1++ = 0;
                if (hifipe)
                {
                    *outputPtr2++ = 0;
                }
                *diffusionErrorPtr++ = 0;
                *diffusionErrorPtr++ = 0;
                *diffusionErrorPtr++ = 0;
                *diffusionErrorPtr++ = 0;
                *diffusionErrorPtr++ = 0;
                *diffusionErrorPtr++ = 0;
                *diffusionErrorPtr++ = 0;
                *diffusionErrorPtr++ = 0;
                rasterByte1 = 0;
                rasterByte2 = 0;
                tmpShortStore = 0;
            }
        } // for pixelCount
    }
    else
    {
        rasterByte1 = 0;
        rasterByte2 = 0;
        inputPtr  += ( numLoop-1 );
        outputPtr1 += ( numLoop/8 - 1 );
        outputPtr2 += ( numLoop/8 - 1 );
        diffusionErrorPtr += ( numLoop-1 );

        tmpShortStore = *diffusionErrorPtr;

        *diffusionErrorPtr = 0;

        for (pixelCount = numLoop + 8; (pixelCount -= 8) > 0; )
        {
            if (pixelCount > 16) // if next 16 pixels are white, skip 8
            {
                doNext8Pixels = Backward16PixelsNonWhite(inputPtr);
            }
            else
            {
                doNext8Pixels = HPTRUE;
            }

            if (doNext8Pixels)
            {
                thValue = HPRand();
                BACKWARD_FED( thValue, 0x01 );
                thValue = HPRand();
                BACKWARD_FED( thValue, 0x02 );
                thValue = HPRand();
                BACKWARD_FED( thValue, 0x04 );
                thValue = HPRand();
                BACKWARD_FED( thValue, 0x08 );
                thValue = HPRand();
                BACKWARD_FED( thValue, 0x10 );
                thValue = HPRand();
                BACKWARD_FED( thValue, 0x20 );
                thValue = HPRand();
                BACKWARD_FED( thValue, 0x40 );
                thValue = HPRand();
                BACKWARD_FED( thValue, 0x80 );

                *outputPtr1-- = rasterByte1;
                rasterByte1 = 0;

                if (hifipe)
                {
                    *outputPtr2-- = rasterByte2;
                    rasterByte2 = 0;
                }
            }
            else  // Do white space skipping
            {
                inputPtr -= 8;
                *outputPtr1-- = 0;
                if (hifipe)
                {
                    *outputPtr2-- = 0;
                }
                *diffusionErrorPtr-- = 0;
                *diffusionErrorPtr-- = 0;
                *diffusionErrorPtr-- = 0;
                *diffusionErrorPtr-- = 0;
                *diffusionErrorPtr-- = 0;
                *diffusionErrorPtr-- = 0;
                *diffusionErrorPtr-- = 0;
                *diffusionErrorPtr-- = 0;
                rasterByte1 = 0;
                rasterByte2 = 0;
                tmpShortStore = 0;
            }
        }
    }
} //HTEDiffOpen

//////////////////////////////////////////////////////////
void Halftoner_Open::FORWARD_FED
(
    HPInt16 thresholdValue,
    unsigned int bitMask
)
{
    tone = (*inputPtr++ );
    fedResPtr = fedResTbl + (tone << 2);
    level = *fedResPtr++;
    if (tone != 0)
    {
    tone = ( tmpShortStore + (HPInt16)(*fedResPtr++) );
    if (tone >= thresholdValue)
        {
        tone -= 255;
        level++;
        }
        switch (level)
        {
            case 0:
            break;
            case 1:
            rasterByte1 |= bitMask;
            break;
            case 2:
            rasterByte2 |= bitMask;
            break;
            case 3:
            rasterByte2 |= bitMask; rasterByte1 |= bitMask;
            break;
            case 4:
            rasterByte3 |= bitMask;
            break;
            case 5:
            rasterByte3 |= bitMask; rasterByte1 |= bitMask;
            break;
            case 6:
            rasterByte3 |= bitMask; rasterByte2 |= bitMask;
            break;
            case 7:
            rasterByte3 |= bitMask; rasterByte2 |= bitMask; rasterByte1 |= bitMask;
            break;
        }
    }
    else
    {
    tone = tmpShortStore;
    }
    *diffusionErrorPtr++ = tone >> 1;
    tmpShortStore = *diffusionErrorPtr + (tone - (tone >> 1));
} //FORWARD_FED


void Halftoner_Open::BACKWARD_FED
(
    HPInt16 thresholdValue,
    unsigned int bitMask
)
{
    tone = (*inputPtr-- );
    fedResPtr = fedResTbl + (tone << 2);
    level = *fedResPtr++;
    if (tone != 0)
    {
    tone = ( tmpShortStore + (HPInt16)(*fedResPtr++) );
    if (tone >= thresholdValue)
        {
        tone -= 255;
        level++;
        }
        switch (level)
        {
            case 0:
            break;
            case 1:
            rasterByte1 |= bitMask;
            break;
            case 2:
            rasterByte2 |= bitMask;
            break;
            case 3:
            rasterByte2 |= bitMask; rasterByte1 |= bitMask;
            break;
            case 4:
            rasterByte3 |= bitMask;
            break;
            case 5:
            rasterByte3 |= bitMask; rasterByte1 |= bitMask;
            break;
            case 6:
            rasterByte3 |= bitMask; rasterByte2 |= bitMask;
            break;
            case 7:
            rasterByte3 |= bitMask; rasterByte2 |= bitMask; rasterByte1 |= bitMask;
            break;
        }
    }
    else
    {
    tone = tmpShortStore;
    }
    *diffusionErrorPtr-- = tone >> 1;
    tmpShortStore = *diffusionErrorPtr + (tone - (tone >> 1));

} //BACKWARD_FED

APDK_END_NAMESPACE
