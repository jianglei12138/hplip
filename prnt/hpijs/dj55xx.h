/*****************************************************************************\
  dj55xx.h : Interface for the generic VIP printer class

  Copyright (c) 2001 - 2015, HP Co.
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


#ifndef APDK_DJ_55XX_H
#define APDK_DJ_55XX_H

APDK_BEGIN_NAMESPACE

//DJ55xx
//!
/*!
\internal
******************************************************************************/
class DJ55xx : public DJGenericVIP
{
public:
    DJ55xx (SystemServices* pSS, BOOL proto = FALSE) : DJGenericVIP (pSS, proto)
    {
    }
    virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
    {
	    return FALSE;
	}
};

#if defined(APDK_DJGENERICVIP) && defined (APDK_DJ9xxVIP)
//! DJ55xxProxy
/*!
******************************************************************************/
class DJ55xxProxy : public PrinterProxy
{
public:
    DJ55xxProxy() : PrinterProxy(
        "DJ55xx",                // family name
        "Deskjet 460\0"
        "Deskjet 470\0"
        "deskjet 5550\0"
        "deskjet 5551\0"
		"OfficeJet 6100\0"
		"OfficeJet 6150\0"
        "Officejet H470\0"
        "PSC 21\0"              
        "PSC 2200\0"

    ) {m_iPrinterType = eDJ55xx;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ55xx(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ55xx;}
	inline unsigned int GetModelBit() const { return 0x200;}
};
#endif

APDK_END_NAMESPACE

#endif  // APDK_DJ55xx_H
