/*****************************************************************************\
  dj3600.h : Interface for the DJ3600 printer class

  Copyright (c) 2003 - 2003, HP Co.
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


#ifndef APDK_DJ3600_H
#define APDK_DJ3600_H

APDK_BEGIN_NAMESPACE

//DJ3600
//!
/*!
\internal
******************************************************************************/
class DJ3600 : public DJ3320
{
public:
    DJ3600 (SystemServices* pSS, BOOL proto = FALSE);
    virtual BOOL FullBleedCapable (PAPER_SIZE ps, FullbleedType  *fbType, float *xOverSpray, float *yOverSpray,
                                   float *fLeftOverSpray, float *fTopOverSpray);
}; //DJ3600


#if defined(APDK_DJ3600) && defined (APDK_DJ3320)
//! DJ3600Proxy
/*!
******************************************************************************/
class DJ3600Proxy : public PrinterProxy
{
public:
    DJ3600Proxy() : PrinterProxy(
        "DJ3600",                               // family name
		"deskjet 3600\0"                        // DeskJet 3600
        "Deskjet 3740\0"
        "Deskjet 3745\0"
		"Deskjet 3840\0"
        "Deskjet 3845\0"
        "Deskjet F300\0"
        "Deskjet D2360\0"
        "Deskjet D24\0"
        "Deskjet F41\0"
		"officejet 4200\0"
        "910\0"
        "HP 910\0"
        "915\0"
        "HP 915\0"
#ifdef APDK_MLC_PRINTER
        "Officejet 4300\0"
		"officejet 5500\0"
        "Officejet 5600\0"
		"psc 1300\0"
		"psc 1310\0"
        "PSC 1400\0"
#endif
    ) {m_iPrinterType = eDJ3600;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ3600(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ3600;}
	inline unsigned int GetModelBit() const { return 0x2;}
};
#endif

APDK_END_NAMESPACE

#endif  // APDK_DJ3600_H
