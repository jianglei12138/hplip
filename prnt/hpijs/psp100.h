/*****************************************************************************\
  psp100.h : Interface for the PSP100 class

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


#ifndef APDK_PSP100_H
#define APDK_PSP100_H
// PhotoSmart 100

APDK_BEGIN_NAMESPACE

//PSP100
//!
/*!
\internal
******************************************************************************/
class PSP100 : public DJ9xxVIP
{
public:
    PSP100 (SystemServices* pSS, BOOL proto = FALSE);

    DRIVER_ERROR VerifyPenInfo ();
//    DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter = TRUE);
    virtual PEN_TYPE DefaultPenSet();
	inline virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode) {return FALSE;}
    virtual BOOL UseGUIMode (PrintMode* pPrintMode);
    PAPER_SIZE    MandatoryPaperSize ();
    virtual BOOL FullBleedCapable (PAPER_SIZE ps, FullbleedType  *fbType, float *xOverSpray, float *yOverSpray,
                                   float *fLeftOverSpray, float *fTopOverSpray)
    {
        *xOverSpray = (float) 0.12;
        *yOverSpray = (float) 0.06;

        if (fLeftOverSpray)
            *fLeftOverSpray = (float) 0.05;
        if (fTopOverSpray)
            *fTopOverSpray  = (float) 0.03;

		if (ps == PHOTO_SIZE || ps == A6_WITH_TEAR_OFF_TAB ||
            ps == PHOTO_5x7)
		{
			*fbType = fullbleed4EdgeAllMedia;
		}
		else
		{
			*fbType = fullbleed3EdgeAllMedia;
		}

        return TRUE;
    }

}; //PSP100


//PSP100Mode
/*
******************************************************************************/
class PSP100Mode : public PrintMode
{
public:
    PSP100Mode ();
}; //PSP100Mode

class PSP100NormalMode : public PrintMode
{
public:
    PSP100NormalMode ();
}; //PSP100NormalMode

#ifdef APDK_EXTENDED_MEDIASIZE
//PSP1002400Mode
/*
******************************************************************************/
class PSP1002400Mode : public PrintMode
{
public:
    PSP1002400Mode ();
}; //PSP1002400Mode
#endif

//GrayModePSP100
/*
******************************************************************************/
class GrayModePSP100 : public PrintMode
{
public:
    GrayModePSP100 ();

}; //GrayModePSP100


#if defined(APDK_PSP100) && defined (APDK_DJ9xxVIP)
//! PSP100Proxy
/*!
******************************************************************************/
class PSP100Proxy : public PrinterProxy
{
public:
    PSP100Proxy() : PrinterProxy(
        "PS100",                // family name
        "PHOTOSMART 100\0"
        "PHOTOSMART 130\0"
        "PHOTOSMART 230\0"
		"photosmart 240\0"
		"photosmart 140\0"
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = ePSP100;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new PSP100(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return ePSP100;}
	inline unsigned int GetModelBit() const { return 0x200;}
};
#endif

APDK_END_NAMESPACE

#endif  // APDK_PSP100_H
