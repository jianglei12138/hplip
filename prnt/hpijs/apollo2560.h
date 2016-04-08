/*****************************************************************************\
  apollo2560.h : Interface for the Apollo2560 class

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


#ifndef APDK_APOLLO2560_H
#define APDK_APOLLO2560_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class Apollo2560 : public Apollo2xxx
{
public:
    Apollo2560(SystemServices* pSS, BOOL proto=FALSE);

}; //Apollo2560

#ifdef APDK_APOLLO2560
//! Apollo2560Proxy
/*!
******************************************************************************/
class Apollo2560Proxy : public PrinterProxy
{
public:
    Apollo2560Proxy() : PrinterProxy(
        "AP2560",                   // family name
        "APOLLO P2500/2600\0"                   // Apollo P2500 & P2600
    ) {m_iPrinterType = eAP2560;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new Apollo2560(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eAP2560;}
	inline unsigned int GetModelBit() const { return 0x1000;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_APOLLO2560_H
