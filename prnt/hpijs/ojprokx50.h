/*****************************************************************************\
  ojprokx50.h : Interface for the generic VIP printer class

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


#ifndef APDK_OJ_PROKX50_H
#define APDK_OJ_PROKX50_H

APDK_BEGIN_NAMESPACE

//OJProKX50
//!
/*!
\internal
******************************************************************************/
class OJProKx50 : public DJ9xxVIP
{
public:
    OJProKx50 (SystemServices* pSS, BOOL proto = FALSE) : DJ9xxVIP (pSS, proto)
    {
        // Papertype for marvellous mode must be mediaGlossy (3) for these printers
        pMode[4]->medium = mediaGlossy;
    }
    virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
    {
        fMargins[0] = (float) 0.125;
        fMargins[1] = (float) 0.125;
        fMargins[2] = (float) 0.125;
        fMargins[3] = (float) 0.5;
        if (ps == SUPERB_SIZE)
        {
            fMargins[0] = (float) 0.125;
            fMargins[1] = (float) 0.5;
            fMargins[2] = (float) 0.125;
            fMargins[3] = (float) 0.75;
        }
	    return TRUE;
	}
};

#if defined (APDK_DJ9xxVIP)
//! OJProKX50Proxy
/*!
******************************************************************************/
class OJProKx50Proxy : public PrinterProxy
{
public:
    OJProKx50Proxy() : PrinterProxy(
        "OJProKx50",                // family name
        "Officejet Pro K550\0"
        "Officejet Pro K850\0"
    ) {m_iPrinterType = eOJProKx50;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new OJProKx50(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eOJProKx50;}
	inline unsigned int GetModelBit() const { return 0x200;}
};
#endif

APDK_END_NAMESPACE

#endif  // APDK_OJ_PROKX50_H
