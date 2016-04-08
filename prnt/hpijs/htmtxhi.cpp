/*****************************************************************************\
  htmtxhi.cpp : Implimentation for Multilevel (HiFipe) dither matrix

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

//===========================================================================
//
//  Filename     :  HTMTXHI.CPP
//
//  Module       :  Interlaken
//
//  Description  :  Multilevel(HiFipe) dither matrix based halftoning
//
//============================================================================

//=============================================================================
//  Header file dependencies
//=============================================================================

/*
#ifndef INCLUDED_HPENVTYP
#include "HPEnvTyp.h"
#endif
#ifndef INCLUDED_HTINTF
#include "HTIntf.h"
#endif
#ifndef INCLUDED_HTWORLD
#include "HTWorld.h"
#endif
#ifndef INCLUDED_HPASSERT
#include "HPAssert.h"
#endif
#ifndef INCLUDED_HTMATRIX
#include "htmatrix.h"
#endif
*/

#include "header.h"
#include "halftoner.h"

APDK_BEGIN_NAMESPACE

#define Dither4LevelHiFipe(matrix,bitMask)\
{\
    tone = (*inputPtr++ );\
    fedResPtr = fedResTbl + (tone << 2);\
    level = *fedResPtr++;\
    if (*(fedResPtr) >= (HPByte)matrix )\
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
                           HPUInt16          count)
{
#if HPUNUSEDVAR
#pragma unused(envP1)
#endif

    THTDitherParmsPtr       ditherParms = ditherParmsPtr+count;
    HPByte                  tone;
    HPUInt8                 rasterByte1,rasterByte2, rasterByte3;
    HPUInt8                 level;
    HPInt16                 pixelCount;

    HPUInt16                numLoop = ditherParms->fNumPix;
    HPBytePtr               inputPtr = ditherParms->fInput;
    HPBytePtr               outputPtr1, outputPtr2, outputPtr3;

    HPBytePtr               fedResTbl = (HPBytePtr)ditherParms->fFEDResPtr;
    HPBytePtr               fedResPtr;

    HPBytePtr       matrixV    = ditherParms->fMatrixV1;
    HPBytePtr       matrixH;
    HPUInt16        index;
    HPUInt16        matrixRowSize = ditherParms->fMatrixRowSize;

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

APDK_END_NAMESPACE


