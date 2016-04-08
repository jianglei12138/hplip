/*****************************************************************************\
  apollo21xx.h : Interface for the Apollo21xx class

  Copyright (c) 2000, 2015, HP Co.
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


#ifndef APDK_APOLLO21xx_H
#define APDK_APOLLO21xx_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class Apollo21xx : public Apollo2xxx
{
public:
    Apollo21xx(SystemServices* pSS, BOOL proto=FALSE);
    Header* SelectHeader(PrintContext* pc);
    virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);

}; //Apollo21xx

#ifdef APDK_APOLLO21XX
//! Apollo21xxProxy
/*!
******************************************************************************/
class Apollo21xxProxy : public PrinterProxy
{
public:
    Apollo21xxProxy() : PrinterProxy(
        "AP21xx",                   // family name
        "P-2000U\0"                             // Apollo P2000U
		) {m_iPrinterType = eAP21xx;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new Apollo21xx(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eAP21xx;}
	inline unsigned int GetModelBit() const { return 0x2000;}
};
#endif

APDK_END_NAMESPACE

#endif // APDK_APOLLO21xx_H
