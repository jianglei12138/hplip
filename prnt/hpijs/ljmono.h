/*****************************************************************************\
  ljmono.h : Interface for the LJMono class

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


#ifndef APDK_LJMONO_H
#define APDK_LJMONO_H

APDK_BEGIN_NAMESPACE

#ifdef  APDK_HIGH_RES_MODES
#define LJ_BASE_RES 300
#else
#define LJ_BASE_RES 150
#endif

/*!
\internal
*/
class LJMono : public Printer
{
public:
    LJMono (SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);
    ~LJMono ();

    virtual Header* SelectHeader (PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    virtual DISPLAY_STATUS ParseError (BYTE status_reg);
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

protected:

#ifdef APDK_HP_UX
    virtual LJMono & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

    BOOL    m_bJobStarted;

}; // LJMono

class LJMonoDraftMode : public GrayMode
{
public:
	LJMonoDraftMode ();
};	// LJMonoDraftMode

class LJMonoNormalMode : public GrayMode
{
public:
    LJMonoNormalMode ();
}; // LJMonoNormalMode

class LJMonoBestMode : public GrayMode
{
public:
    LJMonoBestMode ();
}; // LJMonoBestMode

#ifdef APDK_LJMONO
//! LJMonoProxy
/*!
******************************************************************************/
class LJMonoProxy : public PrinterProxy
{
public:
    LJMonoProxy() : PrinterProxy(
        "Mono Laser",               // family name
        "HP LaserJet\0"
		"hp LaserJet\0"
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = eLJMono;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new LJMono(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eLJMono;}
	inline unsigned int GetModelBit() const { return 0x40;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_LJMono_H
