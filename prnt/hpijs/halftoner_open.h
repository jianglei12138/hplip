/*****************************************************************************\
  halftoner_open.h : Open Source Imaging Halftoning Interface

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


#ifndef HALFTONER_OPEN_H
#define HALFTONER_OPEN_H

APDK_BEGIN_NAMESPACE

class Halftoner_Open : public Halftoner
{
public:
    Halftoner_Open
    (
        SystemServices* pSys,
        PrintMode* pPM,
        unsigned int iInputWidth,
        unsigned int iNumRows[],        // for mixed-res cases
        unsigned int HiResFactor,        // when base-res is multiple of 300
        BOOL matrixbased
    );
    virtual ~Halftoner_Open();

    virtual BOOL Process(RASTERDATA* pbyInputKRGBRaster=NULL);


protected:

    void Interpolate(const uint32_t *start,const unsigned long i,
        unsigned char r,unsigned char g,unsigned char b,
        unsigned char *blackout, unsigned char *cyanout,
        unsigned char *magentaout, unsigned char *yellowout, HPBool);


    HPByte HPRand() // normalize to 5..79
    { BYTE b=RandomNumber() % 74; b+= 5; return b; }


    void HTEDiffOpen   (THTDitherParmsPtr ditherParmsPtr,
        HPUInt16          count);

    THTDitherParms  fDitherParms[6];

    THTDitherParmsPtr       ditherParms;
    kSpringsErrorType       tone;
    kSpringsErrorTypePtr    diffusionErrorPtr;
    kSpringsErrorType       tmpShortStore;
    HPUInt8                 rasterByte1, rasterByte2, rasterByte3;
    HPUInt8                 level;
    HPInt16                 pixelCount;
    HPInt16                 thValue;

    kSpringsErrorTypePtr    errPtr;
    HPUInt16                numLoop;

    HPBytePtr               inputPtr;
    HPBytePtr               outputPtr1, outputPtr2, outputPtr3;

    HPCBytePtr               fedResTbl;
    HPCBytePtr               fedResPtr;

    HPBool                  symmetricFlag;

    HPBool                  doNext8Pixels;

    HPBool                  hifipe;

    void BACKWARD_FED( HPInt16 thresholdValue, unsigned int bitMask );
    void FORWARD_FED( HPInt16 thresholdValue, unsigned int bitMask );

};

APDK_END_NAMESPACE

#endif //HALFTONER_OPEN_H
