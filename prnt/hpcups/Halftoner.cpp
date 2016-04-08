/*****************************************************************************\
  Halftoner.cpp : Implimentation for the Halftoner class

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

#include "CommonDefinitions.h"
#include "Processor.h"
#include "Halftoner.h"

Halftoner::Halftoner
(
    PrintMode* pPM,
    unsigned int iInputWidth,
    int iNumRows[],
    int HiResFactor,
    bool matrixbased
) : ColorPlaneCount(pPM->dyeCount),
    InputWidth(iInputWidth),
    ResBoost(HiResFactor),
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
    originalKData = NULL;

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

    for (i = StartPlane; i <= EndPlane; i++)
    {
        ErrBuff[i] = (short *) new char[((OutputWidth[i] + 2) * sizeof(short))];
        if (ErrBuff[i] == NULL)
        {
            goto MemoryError;
        }
    }

    if (OutputWidth[K] > AdjustedInputWidth)
    // need to expand input data (easier than expanding bit-pixels after) on K row
    {
        tempBuffer = (BYTE*) new BYTE[(OutputWidth[K])];
        if (tempBuffer == NULL)
        {
            goto MemoryError;
        }
        if (EndPlane > Y)
        {
            tempBuffer2 = (BYTE*) new BYTE[(OutputWidth[K])];
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
                ColorPlane[i][j][k] = (BYTE*) new BYTE[(PlaneSize)];
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
        originalKData = (BYTE*) new BYTE[(PlaneSize)];
        if (originalKData == NULL)
        {
            goto MemoryError;
        }
        memset(originalKData, 0, PlaneSize);
    }
    return;

MemoryError:

    FreeBuffers();

    for (i=0; i < ColorPlaneCount; i++)
    {
        for (j=0; j < NumRows[i]; j++)
        {
            for (k=0; k < ColorDepth[i]; k++)
            {
                if (ColorPlane[i][j][k])
                {
                    delete [] (ColorPlane[i][j][k]);
                }
            }
        }
    }
    if (originalKData)
    {
        delete [] (originalKData);
    }

} //Halftoner


Halftoner::~Halftoner()
{
    FreeBuffers();

    for (int i=0; i < MAXCOLORPLANES; i++)
    {
        for (int j=0; j < NumRows[i]; j++)
        {
            for (int k=0; k < ColorDepth[i]; k++)
            {
                if (ColorPlane[i][j][k])
                {
                    delete [] (ColorPlane[i][j][k]);
                }
            }
        }
    }
    if (originalKData)
    {
        delete [] (originalKData);
    }
} //~Halftoner


void Halftoner::Restart()
{
    nNextRaster = 0;

    for (unsigned int i = StartPlane; i <= EndPlane; i++)
      {
        memset(ErrBuff[i], 0, (OutputWidth[i]+2) * sizeof(short));
      }

    started = false;
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
        delete [] (ErrBuff[i]);
    }
    if (tempBuffer)
    {
        delete [] (tempBuffer);
    }
    if (tempBuffer2)
    {
        delete [] (tempBuffer2);
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

unsigned int Halftoner::getOutputWidth()
{
// return size of data in the plane being delivered (depends on iRastersDelivered)
// (will be used in connection with compression seedrow)
    unsigned int colorplane, tmp;
    // figure out which colorplane we're on
    unsigned int rasterd = iRastersDelivered;
    // we come after increment of iRastersDelivered
    if (rasterd > 0)
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
} // getOutputWidth

bool Halftoner::NextOutputRaster(RASTERDATA &next_raster)
{
    if (iRastersReady == 0)
    {
        return false;
    }

    if (iColor == (ColorPlaneCount+StartPlane))
    {
        return false;
    }

    if (iPlane == ColorDepth[iColor])
    {
        iPlane = 0;
        iRow++;
        return NextOutputRaster(next_raster);
    }

    if (iRow == NumRows[iColor])
    {
        iRow = 0;
        iColor++;
        return NextOutputRaster(next_raster);
    }

    iRastersDelivered++;
    iRastersReady--;
    next_raster.rasterdata[COLORTYPE_COLOR] = ColorPlane[iColor][iRow][iPlane++];
    next_raster.rastersize[COLORTYPE_COLOR] = getOutputWidth();
    next_raster.rasterdata[COLORTYPE_BLACK] = raster.rasterdata[COLORTYPE_BLACK];
    next_raster.rastersize[COLORTYPE_BLACK] = 0;
    return true;
} //NextOutputRaster


bool Halftoner::LastPlane()
{
      return ((iColor == (ColorPlaneCount+StartPlane-1)) &&
              (iRow == (unsigned int)(NumRows[iColor] - 1)) &&
              (iPlane == ColorDepth[iColor])        // was pre-incremented
              );
} //LastPlane


bool Halftoner::FirstPlane()
{
      return ((iColor == StartPlane) &&
              (iRow == 0) &&
              (iPlane == 1)        // was pre-incremented
              );
} //FirstPlane

unsigned int Halftoner::GetMaxOutputWidth()
// This is needed by Configure, since the output-width for Halftoner is variable
// depending on the colorplane
{
    if (myplane == COLORTYPE_COLOR)
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
} // CleanOddBits


extern unsigned char BayerMatrix[];

bool Halftoner::Process
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
        return false;   // no output
    }
    started=true;

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
            fDitherParms[i].fSymmetricFlag = true;   // Symmetric only
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
                BYTE * matrixptr = (BYTE *)(
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
    return true;   // one raster in, one raster out
} //Process


void Halftoner::HTEDiffOpen
(
    THTDitherParmsPtr ditherParmsPtr,
    unsigned short count
)
{


    ditherParms = ditherParmsPtr+count;
    errPtr = ditherParms->fErr;
    numLoop = ditherParms->fNumPix;
    inputPtr = ditherParms->fInput;
    fedResTbl = ditherParms->fFEDResPtr;
    symmetricFlag = ditherParms->fSymmetricFlag;
    doNext8Pixels = true;
    hifipe = ditherParms->fHifipe;
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
                doNext8Pixels = true;
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
                doNext8Pixels = true;
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
void Halftoner::FORWARD_FED
(
    short thresholdValue,
    unsigned int bitMask
)
{
    tone = (*inputPtr++ );
    fedResPtr = fedResTbl + (tone << 2);
    level = *fedResPtr++;
    if (tone != 0)
    {
    tone = ( tmpShortStore + (short)(*fedResPtr++) );
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


void Halftoner::BACKWARD_FED
(
    short thresholdValue,
    unsigned int bitMask
)
{
    tone = (*inputPtr-- );
    fedResPtr = fedResTbl + (tone << 2);
    level = *fedResPtr++;
    if (tone != 0)
    {
    tone = ( tmpShortStore + (short)(*fedResPtr++) );
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

} // BACKWARD_FED

#define Dither4LevelHiFipe(matrix,bitMask)\
{\
    tone = (*inputPtr++ );\
    fedResPtr = fedResTbl + (tone << 2);\
    level = *fedResPtr++;\
    if (*(fedResPtr) >= (BYTE)matrix )\
    {\
            level++;\
    }\
        switch (level)\
        {\
            case 0:\
                break;\
            case 1:\
                rasterByte1 |= bitMask;\
                break;\
            case 2:\
                rasterByte2 |= bitMask;\
                break;\
            case 3:\
                rasterByte2 |= bitMask; rasterByte1 |= bitMask;\
                break;\
            case 4:\
                rasterByte3 |= bitMask;\
                break;\
            case 5:\
                rasterByte3 |= bitMask; rasterByte1 |= bitMask;\
                break;\
            case 6:\
                rasterByte3 |= bitMask; rasterByte2 |= bitMask;\
                break;\
            case 7:\
                rasterByte3 |= bitMask; rasterByte2 |= bitMask; rasterByte1 |= bitMask;\
                break;\
        }\
}

// Each dither matrix has a header of five bytes
//    1st byte    (Size (in bytes) of row in the matrix) - 1
//    2nd byte    Size (in bytes) of row in the matrix
//    3rd byte    Black offset
//    4th byte    Cyan offset
//    5th byte    Magenta offset
//
//    With matrix actually starting on the 6th Byte.
//
//    Black, cyan, and magenta are all offset into different
//    locations in the dither matrix.  (Yellow uses the same offsets as
//    magenta)
//

// Ditherparms should be set up as Fed with the following additions
// ditherParms->fSqueezeOffset = first dirty pixel, if white has been skipped at the beginning of the row
// ditherParmsPtr->fMatrixRowSize = Byte 2 of the Header
// ditherParmsPtr->fDitherCellOffset = Black, Cyan, or Magenta Offset respectively

//    To select the right row of the matrix for each color
//
//    ditherParms->fMatrixV1 = ((matrixStart + rasterIndex + DitherCellOffset) % (MatrixRowSize)) * (MatrixRowSize)

DRIVER_ERROR Halftoner::HTMATRIXHI_KCMY   (THTDitherParmsPtr ditherParmsPtr,
                           unsigned short          count)
{
    THTDitherParmsPtr       ditherParms = ditherParmsPtr+count;
    BYTE                  tone;
    BYTE                 rasterByte1,rasterByte2, rasterByte3;
    BYTE                 level;
    short                 pixelCount;

    unsigned short                numLoop = ditherParms->fNumPix;
    BYTE *                inputPtr = ditherParms->fInput;
    BYTE *                outputPtr1;
    BYTE *                outputPtr2;
    BYTE *                outputPtr3;

    BYTE *                fedResTbl = (BYTE * )ditherParms->fFEDResPtr;
    BYTE *                fedResPtr;

    BYTE *        matrixV    = ditherParms->fMatrixV1;
    BYTE *        matrixH;
    unsigned short        index;
    unsigned short        matrixRowSize = ditherParms->fMatrixRowSize;

    rasterByte1 = 0;
    rasterByte2 = 0;
    rasterByte3 = 0;

    outputPtr1 = ditherParms->fOutput1;
    outputPtr2 = ditherParms->fOutput2;
    outputPtr3 = ditherParms->fOutput3;

    if (!ditherParms->fSymmetricFlag)
        return SYSTEM_ERROR;    // matrixHI_KCMY asymetric not supported

    index = (ditherParms->fDitherCellOffset + ditherParms->fSqueezeOffset) % (matrixRowSize);
    matrixH = matrixV + index;

    for (pixelCount = numLoop + 8; (pixelCount -= 8) > 0; )
    {
        // if we've reached end of matrix we need to reset
        // our row pointer to the start of the row
        if ( index == matrixRowSize )
        {
            matrixH = matrixV;
            index = 0;
        }

        Dither4LevelHiFipe(*matrixH,0x80);
        matrixH++;
        Dither4LevelHiFipe(*matrixH,0x40);
        matrixH++;
        Dither4LevelHiFipe(*matrixH,0x20);
        matrixH++;
        Dither4LevelHiFipe(*matrixH,0x10);
        matrixH++;
        Dither4LevelHiFipe(*matrixH,0x08);
        matrixH++;
        Dither4LevelHiFipe(*matrixH,0x04);
        matrixH++;
        Dither4LevelHiFipe(*matrixH,0x02);
        matrixH++;
        Dither4LevelHiFipe(*matrixH,0x01);
        matrixH++;

        *outputPtr1++ |= rasterByte1;
        if (outputPtr2)
            *outputPtr2++ |= rasterByte2;
        rasterByte1 = 0;
        rasterByte2 = 0;

        index += 8;
    }
    return NO_ERROR;
}

unsigned char BayerMatrix[] =
{
    0x07,   0x08,   0x00,   0x00,   0x00,
    0x2,    0x82,   0x22,   0xa2,   0xa,    0x8a,   0x2a,   0xaa,
    0xc2,   0x42,   0xe2,   0x62,   0xca,   0x4a,   0xea,   0x6a,
    0x32,   0xb2,   0x12,   0x92,   0x3a,   0xba,   0x1a,   0x9a,
    0xf2,   0x72,   0xd2,   0x52,   0xfa,   0x7a,   0xda,   0x5a,
    0xe,    0x8e,   0x2e,   0xae,   0x6,    0x86,   0x26,   0xa6,
    0xce,   0x4e,   0xee,   0x6e,   0xc6,   0x46,   0xe6,   0x66,
    0x3e,   0xbe,   0x1e,   0x9e,   0x36,   0xb6,   0x16,   0x96,
    0xfe,   0x7e,   0xde,   0x5e,   0xf6,   0x76,   0xd6,   0x56,
};

