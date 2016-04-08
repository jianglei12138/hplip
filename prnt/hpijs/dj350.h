/*****************************************************************************\
  dj350.h : Interface for the DJ350 class

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


#ifndef DJ350_H
#define DJ350_H

APDK_BEGIN_NAMESPACE

//extern char *ModelString[MAX_ID_STRING];

/*!
\internal
*/
class DJ350 : public DJ600 // Printer
{
public:
    DJ350(SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);

    virtual Header* SelectHeader(PrintContext* pc);

}; //DJ350

#ifdef APDK_DJ350
//! DJ350Proxy
/*!
******************************************************************************/
class DJ350Proxy : public PrinterProxy
{
public:
    DJ350Proxy() : PrinterProxy(
        "DJ350",                    // family name
        "DESKJET 350\0"                                 // DeskJet 350
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = eDJ350;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ350(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ350;}
	inline unsigned int GetModelBit() const { return 0x800;}
};
#endif

APDK_END_NAMESPACE

#endif
