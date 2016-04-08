/*****************************************************************************\
  ljcolor.h : Interface for the LJColor class

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


#ifndef APDK_LJCOLOR_H
#define APDK_LJCOLOR_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class LJColor : public Printer
{
public:
    LJColor (SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);
    ~LJColor ();

    virtual Header* SelectHeader (PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    virtual DISPLAY_STATUS ParseError (BYTE status_reg);
	inline virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode) {return FALSE;}
    DRIVER_ERROR SkipRasters (int nBlankRasters);
	virtual DRIVER_ERROR Flush (int FlushSize)
	{
		return NO_ERROR;
	}

    virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
    {
        fMargins[0] = (float) 0.25;
        fMargins[1] = (float) 0.25;
        fMargins[2] = (float) 0.2;
        fMargins[3] = (float) 0.2;
        return TRUE;
    }

    virtual BOOL UseCMYK (unsigned int iPrintMode) { return FALSE;}
	virtual DRIVER_ERROR Encapsulate (const RASTERDATA* InputRaster, BOOL bLastPlane);

#ifdef APDK_AUTODUPLEX

/*
 *  Note that we are returning TRUE here not to say there is a Hagaki Feed tray
 *  and Hagaki Card duplexer. Some non-HP Lasers number their input trays
 *  differently and try 5 (sourceHagakiFeedNDuplexer) can be a valid input tray.
 *  In order to allow selection of this tray, we will return TRUE for all lasers.
 */

    virtual BOOL HagakiFeedDuplexerPresent (BOOL bQueryPrinter)
    {
        return TRUE;
    }
#endif

    Compressor* CreateCompressor (unsigned int RasterSize);
	BOOL	bFGColorSet;
    BOOL    bGrey_K;
    int     m_iYPos;

protected:

#ifdef APDK_HP_UX
    virtual LJColor & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

    BOOL        m_bJobStarted;
    int         m_iYResolution;
    Compressor  *m_pCompressor;

}; // LJColor

class LJColorKDraftMode : public GrayMode
{
public:
	LJColorKDraftMode ();
};	// LJColorDraftMode

class LJColorGrayMode : public GrayMode
{
public:
	LJColorGrayMode ();
}; // LJColorGrayMode

class LJColor150DPIMode : public PrintMode
{
public:
    LJColor150DPIMode ();
}; // LJColor150DPIMode

class LJColor300DPIMode : public PrintMode
{
public:
    LJColor300DPIMode ();
};   // LJColor300DPIMode

class LJColor600DPIMode : public PrintMode
{
public:
    LJColor600DPIMode ();
};   // LJColor600DPIMode

#ifdef APDK_EXTENDED_MEDIASIZE
class LJColorPlainBestMode : public PrintMode
{
public:
    LJColorPlainBestMode ();
};
#endif

#ifdef APDK_LJCOLOR
//! LJColorProxy
/*!
******************************************************************************/
class LJColorProxy : public PrinterProxy
{
public:
    LJColorProxy() : PrinterProxy(
        "ColorLaser",                   // family name
        "hp color LaserJet\0"
		"HP Color LaserJet\0"
		"hp business inkjet 2600\0"
		"hp business inkjet 3000\0"
		"hp business inkjet 2300\0"
		"Officejet 9100\0"
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = eLJColor;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new LJColor(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eLJColor;}
	inline unsigned int GetModelBit() const { return 0x20;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_LJCOLOR_H
