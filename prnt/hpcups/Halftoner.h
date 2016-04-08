/*****************************************************************************\
  Halftoner.h : Interface for the Halftoner class

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


#ifndef HALFTONER_H
#define HALFTONER_H

////////////////////////////////////////////////////////////////////////////
// UMPQUA
//
// Encapsulation of buffers and data needed by Halftoner color-matching and
// halftoning code.

class Halftoner : public Processor
{
public:
    Halftoner(PrintMode* pPM,
        unsigned int iInputWidth,
        int iNumRows[],        // for mixed-res cases
        int HiResFactor,        // when base-res is multiple of 300
        bool matrixbased
        );
    virtual ~Halftoner();

    bool Process(RASTERDATA* pbyInputKRGBRaster);
    void Flush();
    void Restart();         // set up for new page or blanks

    // items required by Processor
    unsigned int GetMaxOutputWidth();
    bool    NextOutputRaster(RASTERDATA &next_raster);
    bool LastPlane();
    bool FirstPlane();
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
    int ResBoost;

private:
    void Interpolate(const uint32_t *start,const unsigned long i,
                     unsigned char r,unsigned char g,unsigned char b,
                     unsigned char *blackout, unsigned char *cyanout,
                      unsigned char *magentaout, unsigned char *yellowout, bool);
    void FreeBuffers();

    bool Forward16PixelsNonWhite(BYTE * inputPtr)
    {
//        return ((*(uint32_t *)(inputPtr) != 0x0) || (*(((uint32_t *)(inputPtr)) + 1) != 0x0)  ||
//            (*(((uint32_t *)(inputPtr)) + 2) != 0x0) || (*(((uint32_t *)(inputPtr)) + 3) != 0x0));
        for (int i=0; i < 16; i++)
        {
            if ((*inputPtr++) != 0)
            {
                return true;
            }
        }
        return false;
    }

    bool Backward16PixelsNonWhite(BYTE * inputPtr)
    {
//        return ((*(uint32_t *)(inputPtr) != 0x0) || (*(((uint32_t *)(inputPtr)) - 1) != 0x0)  ||
//            (*(((uint32_t *)(inputPtr)) - 2) != 0x0) || (*(((uint32_t *)(inputPtr)) - 3) != 0x0));
        for (int i=0; i < 16; i++)
        {
            if ((*inputPtr--) !=0 )
            {
                return true;
            }
        }
        return false;
    }
    unsigned int getOutputWidth();

    short* ErrBuff[6];

    short   nNextRaster;

    short          fRasterOdd;
    const unsigned char* fBlackFEDResPtr;
    const unsigned char* fColorFEDResPtr;

    unsigned int AdjustedInputWidth;    // InputWidth padded to be divisible by 8
    void PixelMultiply(unsigned char* buffer, unsigned int width, unsigned int factor);

    unsigned int iColor, iRow, iPlane;
    unsigned int PlaneCount();          // tells how many layers (colors,hifipe,multirow)

    bool started;

    BYTE* tempBuffer;
    BYTE* tempBuffer2;
    BYTE* originalKData;
    unsigned int oddbits;
    void CleanOddBits(unsigned int iColor, unsigned int iRow);

    uint32_t hold_random;
    inline BYTE RandomNumber()
    {
       hold_random = (hold_random * 214013) + 2531011;
       return (BYTE)((hold_random >> 16) & 0xFF);
    } //RandomNumber


    typedef struct THTDitherParms
    {
        unsigned short    fNumPix;            // Dirty Pixels to be dithered
        BYTE *   fInput;             // Pixel array to dither
        BYTE *   fOutput1;           // Output raster binary & hifipe plane 1
        BYTE *   fOutput2;           // Output raster hifipe plane 2 (2-bit)
        BYTE *   fOutput3;           // Output raster hifipe plane 3 (3-bit)

        const BYTE *  fFEDResPtr;         // brkpnt table

        short *    fErr;            // Current error buffer
        short                 fRasterEvenOrOdd;// Serpentine (Forward/Backward)

        bool                  fSymmetricFlag;   // Are we symmetric

        bool                  fHifipe;          // Are we doing Hifipe?

        unsigned short       fMatrixRowSize;
        BYTE *               fMatrixV1;
        unsigned short       fDitherCellOffset;
        unsigned short       fSqueezeOffset;

        short *             fWeightTablePtr; // Error Diffusion threshold table
        short                 fOffsetPick;     // Random# offset for threshold
        bool                  fVerticalExpFlag; // Are we vertically expanding
    } THTDitherParms, *THTDitherParmsPtr;

    bool usematrix;

    DRIVER_ERROR HTMATRIXHI_KCMY(THTDitherParmsPtr ditherParmsPtr, unsigned short count);

    BYTE HPRand() // normalize to 5..79
    { BYTE b=RandomNumber() % 74; b+= 5; return b; }


    void HTEDiffOpen   (THTDitherParmsPtr ditherParmsPtr,
        unsigned short          count);

    THTDitherParms  fDitherParms[6];

    THTDitherParmsPtr    ditherParms;
    short                tone;
    short                *diffusionErrorPtr;
    short                tmpShortStore;
    BYTE                 rasterByte1, rasterByte2, rasterByte3;
    BYTE                 level;
    short                pixelCount;
    short                thValue;

    short                *errPtr;
    unsigned short       numLoop;

    BYTE                 *inputPtr;
    BYTE                 *outputPtr1;
    BYTE                 *outputPtr2;
    BYTE                 *outputPtr3;

    const BYTE           *fedResTbl;
    const BYTE           *fedResPtr;

    bool                  symmetricFlag;

    bool                  doNext8Pixels;

    bool                  hifipe;

    void BACKWARD_FED(short thresholdValue, unsigned int bitMask);
    void FORWARD_FED(short thresholdValue, unsigned int bitMask);
}; // Halftoner

#endif // HALFTONER_H

