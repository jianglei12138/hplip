/*****************************************************************************\
  dj890.h : Interface for the DJ890 class

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


#ifndef APDK_DJ890_H
#define APDK_DJ890_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class DJ890 : public DJ8xx
{
public:
    DJ890(SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);
    virtual Header *SelectHeader (PrintContext *pc);

}; //DJ890

#if defined(APDK_DJ890)
//! DJ890Proxy
/*!
******************************************************************************/
class DJ890Proxy : public PrinterProxy
{
public:
    DJ890Proxy() : PrinterProxy(
        "DJ890",                    // family name
        "DESKJET 890\0"                         // DeskJet 890
#ifdef APDK_MLC_PRINTER
        "OFFICEJET PRO 117\0"                   // OfficeJet PRO 117
#endif
    ) {m_iPrinterType = eDJ890;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ890(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ890;}
	inline unsigned int GetModelBit() const { return 0x4;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_DJ890_H
