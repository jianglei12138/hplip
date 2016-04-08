/*****************************************************************************\
  halftoner.h : Interface for the Halftoner class

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


#ifndef APDK_HALFTONER_H
#define APDK_HALFTONER_H
//===========================================================================
//
//  Filename     :  Halftoner.h
//
//  Module       :  Open Source Imaging
//
//  Description  :  This file contains the class declaration for Imaging.
//
//===========================================================================

#ifndef HPTYPES_H
#include "hptypes.h"
#endif

APDK_BEGIN_NAMESPACE

// used to encourage consistent ordering of color planes
#define K   0
#define C   1
#define M   2
#define Y   3
#define Clight  4
#define Mlight  5

#define RANDSEED 77

////////////////////////////////////////////////////////////////////////////
// UMPQUA
//
// Encapsulation of buffers and data needed by Halftoner color-matching and
// halftoning code.

class Halftoner : public Processor
{
public:
    Halftoner(SystemServices* pSys,
        PrintMode* pPM,
        unsigned int iInputWidth,
        unsigned int iNumRows[],        // for mixed-res cases
        unsigned int HiResFactor,        // when base-res is multiple of 300
        BOOL matrixbased
        );
    virtual ~Halftoner();

    virtual BOOL Process(RASTERDATA* pbyInputKRGBRaster=NULL) = 0;
    virtual void Flush();

    DRIVER_ERROR constructor_error;

    virtual void Restart();         // set up for new page or blanks

    // items required by Processor
    unsigned int GetOutputWidth(COLORTYPE  rastercolor);
    unsigned int GetMaxOutputWidth(COLORTYPE  rastercolor);
    BYTE* NextOutputRaster(COLORTYPE  rastercolor);
    BOOL LastPlane();
    BOOL FirstPlane();
    unsigned int PlaneSize()
        { return OutputWidth[iColor] / 8 + (OutputWidth[iColor] % 8); }

    unsigned int ColorPlaneCount;
    unsigned int InputWidth;    // # of pixels input per colorplane
    unsigned int OutputWidth[MAXCOLORPLANES];   // # of pixels output per colorplane
    unsigned char ColorDepth[MAXCOLORPLANES];
    // how many rows needed relative to base resolution -- all 1 unless mixed-res
    unsigned char NumRows[MAXCOLORPLANES];
    // color plane data
    // for current interface, we must maintain mapping of
    //  0=K, 1=C, 2=M, 3=Y
    BYTE* ColorPlane[MAXCOLORPLANES][MAXCOLORROWS][MAXCOLORDEPTH];

    unsigned int StartPlane;    // since planes are ordered KCMY, if no K, this is 1
    unsigned int EndPlane;      // usually Y, could be Mlight
    unsigned int ResBoost;

protected:

    SystemServices* pSS;    // needed for memory management

    void FreeBuffers();

    HPBool Forward16PixelsNonWhite(HPBytePtr inputPtr)
    {
//        return ((*(HPUInt32Ptr)(inputPtr) != 0x0) || (*(((HPUInt32Ptr)(inputPtr)) + 1) != 0x0)  ||
//            (*(((HPUInt32Ptr)(inputPtr)) + 2) != 0x0) || (*(((HPUInt32Ptr)(inputPtr)) + 3) != 0x0));
        for (int i=0; i < 16; i++)
        {
            if ((*inputPtr++) != 0)
            {
                return TRUE;
            }
        }
        return FALSE;
    }

    HPBool Backward16PixelsNonWhite(HPBytePtr inputPtr)
    {
//        return ((*(HPUInt32Ptr)(inputPtr) != 0x0) || (*(((HPUInt32Ptr)(inputPtr)) - 1) != 0x0)  ||
//            (*(((HPUInt32Ptr)(inputPtr)) - 2) != 0x0) || (*(((HPUInt32Ptr)(inputPtr)) - 3) != 0x0));
        for (int i=0; i < 16; i++)
        {
            if ((*inputPtr--) !=0 )
            {
                return TRUE;
            }
        }
        return FALSE;
    }


    short* ErrBuff[6];

    short   nNextRaster;

    short          fRasterOdd;
    unsigned char* fBlackFEDResPtr;
    unsigned char* fColorFEDResPtr;

    unsigned int AdjustedInputWidth;    // InputWidth padded to be divisible by 8
    void PixelMultiply(unsigned char* buffer, unsigned int width, unsigned int factor);

    unsigned int iColor, iRow, iPlane;
    unsigned int PlaneCount();          // tells how many layers (colors,hifipe,multirow)

    BOOL started;

    BYTE* tempBuffer;
    BYTE* tempBuffer2;
	BYTE* originalKData;
    unsigned int oddbits;
    void CleanOddBits(unsigned int iColor, unsigned int iRow);

    HPUInt32 hold_random;
    inline BYTE RandomNumber()
    {
       hold_random = (hold_random * 214013) + 2531011;
       return (BYTE)((hold_random >> 16) & 0xFF);
    } //RandomNumber


    typedef struct THTDitherParms
    {
        HPUInt16    fNumPix;            // Dirty Pixels to be dithered
        HPBytePtr   fInput;             // Pixel array to dither
        HPBytePtr   fOutput1;           // Output raster binary & hifipe plane 1
        HPBytePtr   fOutput2;           // Output raster hifipe plane 2 (2-bit)
        HPBytePtr   fOutput3;           // Output raster hifipe plane 3 (3-bit)

        HPCBytePtr  fFEDResPtr;         // brkpnt table

        kSpringsErrorTypePtr    fErr;            // Current error buffer
        HPInt16                 fRasterEvenOrOdd;// Serpentine (Forward/Backward)

        HPBool                  fSymmetricFlag;   // Are we symmetric

        HPBool                  fHifipe;          // Are we doing Hifipe?

        HPUInt16                fMatrixRowSize;
        HPBytePtr               fMatrixV1;
        HPUInt16                fDitherCellOffset;
        HPUInt16                fSqueezeOffset;

        HPCInt16Ptr             fWeightTablePtr; // Error Diffusion threshold table
        HPInt16                 fOffsetPick;     // Random# offset for threshold
        HPBool                  fVerticalExpFlag; // Are we vertically expanding
    } THTDitherParms, ENVPTR(THTDitherParmsPtr);

    BOOL usematrix;

    DRIVER_ERROR HTMATRIXHI_KCMY(THTDitherParmsPtr ditherParmsPtr, HPUInt16 count);

}; //Halftoner

APDK_END_NAMESPACE

#endif //APDK_HALFTONER_H
