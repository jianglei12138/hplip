/*****************************************************************************\
  Hbpl1.h : Interface for the Hbpl1 class

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

  Author(s): Suma Byrappa
\*****************************************************************************/


#ifndef HBPL1_H
#define HBPL1_H
#include "CommonDefinitions.h"
#include "Pipeline.h"
#include "Encapsulator.h"
#include "ColorMatcher.h"
#include "Halftoner.h"
#include "resources.h"
#include "ColorMaps.h"
#include "PrinterCommands.h"
#include "Utils.h"
#include "Hbpl1_Wrapper.h"
#define HBPL1_DEFAULT_STRIP_HEIGHT 128

class Hbpl1Wrapper;

enum COLORTHEME
{
    NONE            = 0,
    DEFAULTSRGB     = 1,
    PHOTOSRGB       = 2,
    ADOBERGB        = 3,
    VIVIDSRGB       = 4
};

class Hbpl1 : public Encapsulator
{

    friend class Hbpl1Wrapper;

public:
    Hbpl1 ();
    ~Hbpl1 ();
    DRIVER_ERROR    Encapsulate (RASTERDATA *InputRaster, bool bLastPlane);
    DRIVER_ERROR    StartPage(JobAttributes *pJA);
    DRIVER_ERROR    StartJob(SystemServices*, JobAttributes*);
    DRIVER_ERROR    Configure(Pipeline **pipeline);
    DRIVER_ERROR    FormFeed();
    DRIVER_ERROR    EndJob();
    DRIVER_ERROR    SendCAPy(int iOffset) {return NO_ERROR;}
    bool            CanSkipRasters() {return false;}

protected:
    DRIVER_ERROR flushPrinterBuffer() {return NO_ERROR;}
    virtual DRIVER_ERROR addJobSettings();

private:
    DRIVER_ERROR    EndPage ();
    DRIVER_ERROR    sendBlankBands();
    PrintMode       m_PM;
    int             m_iCurRaster;
    int             m_iPlaneNumber;
    int             m_iBpp;
    int             m_iPlanes;
    long long       m_nStripSize;
    long long       m_nStripHeight;
    int             m_nBandCount;
    long long       m_numScanLines;
    int             m_nScanLineCount;
    long long       m_nScanLineSize;
    int             m_numStrips;
    BYTE           *m_pbyStripData;
    BYTE           *m_pOutBuffer;
    int             m_OutBuffSize;
    JobAttributes   m_JA;
    void           *m_hHPLibHandle;
    bool            m_init;
    bool            m_Economode;
    bool            m_PrintinGrayscale;
    COLORTHEME      m_ColorTheme;
    COLORTYPE       m_ColorMode;
    Hbpl1Wrapper   *m_pHbpl1Wrapper;
}; // Hbpl1

#endif // HBPL1_H

