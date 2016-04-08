/*****************************************************************************\
  djd2600.h : Interface for the DJ D2600 printer class

  Copyright (c) 2003 - 2015, HP Co.
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


#ifndef APDK_DJD2600_H
#define APDK_DJD2600_H
#if defined(APDK_DJ3600) && defined (APDK_DJ3320)

APDK_BEGIN_NAMESPACE
//DJ2600
//!
/*!
\internal
******************************************************************************/
class DJD2600 : public DJ4100
{
public:
    DJD2600 (SystemServices* pSS, BOOL proto = FALSE) : DJ4100 (pSS, proto)
    {
        m_iBlackPenResolution = 600;
        m_iNumBlackNozzles    = 336;
    }
}; //DJD2600


#if defined(APDK_DJ3600) && defined (APDK_DJ3320)
//! DJ4100Proxy
/*!
******************************************************************************/
class DJD2600Proxy : public PrinterProxy
{
public:
    DJD2600Proxy() : PrinterProxy(
        "DJD2600",                               // family name
		"Deskjet D26\0"                        // DeskJet D2600
        "Deskjet D16\0"
    ) {m_iPrinterType = eDJD2600;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJD2600(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJD2600;}
	inline unsigned int GetModelBit() const { return 0x2;}
};
#endif

APDK_END_NAMESPACE

#endif // defined(APDK_DJ3600) && defined (APDK_DJ3320)

#endif  // APDK_DJD2600_H
