/*****************************************************************************\
  LJZjStream.h : Interface for the LJZjStream class

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

  Author: Naga Samrat Chowdary Narla,
\*****************************************************************************/


#ifndef LJZJSTREAM_H
#define LJZJSTREAM_H

#include "ModeJbig.h"

class LJZjStream : public Encapsulator
{
public:
    LJZjStream ();
    ~LJZjStream ();
    DRIVER_ERROR    Encapsulate (RASTERDATA *InputRaster, bool bLastPlane);
    DRIVER_ERROR    StartPage(JobAttributes *pJA);
    DRIVER_ERROR    StartPage_ljzjcolor2(JobAttributes *pJA);
    DRIVER_ERROR    Configure(Pipeline **pipeline);
    DRIVER_ERROR    FormFeed();
    DRIVER_ERROR    EndJob();
    DRIVER_ERROR    SendCAPy(int iOffset) {return NO_ERROR;}
    DRIVER_ERROR    preProcessRasterData(cups_raster_t **cups_raster, cups_page_header2_t* firstpage_cups_header, char* pSwapedPagesFileName);
    bool            CanSkipRasters() {return false;}

protected:
    DRIVER_ERROR flushPrinterBuffer() {return NO_ERROR;}
    virtual DRIVER_ERROR addJobSettings();
private:
    DRIVER_ERROR    encapsulateColor(RASTERDATA *input);
    DRIVER_ERROR    encapsulateColor2(RASTERDATA *input);
    DRIVER_ERROR    EndPage ();
    DRIVER_ERROR    sendBlankBands();
    PrintMode    m_PM;
    ModeJbig     *m_pModeJbig;
    bool         m_bNotSent;
    int          m_iCurRaster;
    int          m_iPlaneNumber;
    int          m_iBpp;
    int          m_iPlanes;
}; // LJZjStream

#endif // LJZJSTREAM_H

