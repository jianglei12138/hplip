/*****************************************************************************\
  psp470.h : Interface for the PSP470 class

  Copyright (c) 2001-2015, HP Co.
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


#ifndef APDK_PSP470_H
#define APDK_PSP470_H
// Photosmart 470 series

APDK_BEGIN_NAMESPACE

//PSPFastNormalMode
/*
******************************************************************************/
class PSPFastNormalMode : public PrintMode
{
public:
    PSPFastNormalMode ();
}; //PSPFastNormalMode
/******************************************************************************/

PSPFastNormalMode::PSPFastNormalMode () : PrintMode (NULL)
{

    BaseResX = BaseResY = TextRes = ResolutionX[0] = ResolutionY[0] = 600;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = TRUE;
#endif

    Config.bColorImage = FALSE;

    medium = mediaAuto;
    theQuality = qualityDraft;
	pmQuality   = QUALITY_DRAFT;
    pmMediaType = MEDIA_PHOTO;
    CompatiblePens[0] = COLOR_PEN;
    bFontCapable = FALSE;
} //PSP100Mode

//PSP470
//!
/*!
\internal
******************************************************************************/
class PSP470 : public PSP100
{
public:
    PSP470 (SystemServices* pSS, BOOL proto = FALSE) : PSP100 (pSS, proto)
    {
        pMode[ModeCount] = new PSPFastNormalMode ();
        ModeCount++;
    }

    PAPER_SIZE    MandatoryPaperSize ()
    {
        return PHOTO_5x7;
    }
    virtual BOOL FullBleedCapable (PAPER_SIZE ps, FullbleedType  *fbType, float *xOverSpray, float *yOverSpray,
                                   float *fLeftOverSpray, float *fTopOverSpray)
    {
        *xOverSpray = (float) 0.12;
        *yOverSpray = (float) 0.06;

        if (fLeftOverSpray)
            *fLeftOverSpray = (float) 0.05;
        if (fTopOverSpray)
            *fTopOverSpray  = (float) 0.03;

		*fbType = fullbleed4EdgeAllMedia;
        return TRUE;
    }

}; //PSP470

#if defined(APDK_PSP100) && defined (APDK_DJ9xxVIP)
//! PSP470Proxy
/*!
******************************************************************************/
class PSP470Proxy : public PrinterProxy
{
public:
    PSP470Proxy() : PrinterProxy(
        "PS470",                // family name
        "Photosmart 470\0"
        "Photosmart 475\0"
        "Photosmart A610\0"
        "Photosmart A620\0"
        "Photosmart A710\0"
        "Photosmart A820\0"
    ) {m_iPrinterType = ePSP470;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new PSP470(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return ePSP470;}
	inline unsigned int GetModelBit() const { return 0x200;}
};
#endif

APDK_END_NAMESPACE

#endif  // APDK_PSP470_H
