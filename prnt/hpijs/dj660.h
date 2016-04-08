/*****************************************************************************\
  dj660.h : Interface for the DJ660 class

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


#ifndef APDK_DJ660_H
#define APDK_DJ660_H

APDK_BEGIN_NAMESPACE

//extern char *ModelString[MAX_ID_STRING];

/*!
\internal
*/
class DJ660 : public DJ6XX
{
public:
    DJ660(SystemServices* pSS, BOOL proto=FALSE);

    Header* SelectHeader(PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo();
    virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);

}; //DJ660


class Mode660Draft : public PrintMode
{
public:
    Mode660Draft();
}; //Mode660Draft

#ifdef APDK_EXTENDED_MEDIASIZE
class Mode660DraftGrayK : public GrayMode
{
public:
    Mode660DraftGrayK();
}; //Mode660DraftGrayK

class Mode660BestGrayK : public GrayMode
{
public:
    Mode660BestGrayK();
}; //Mode660BestGrayK
#endif // APDK_EXTENDED_MEDIASIZE

#ifdef APDK_DJ6xx
//! DJ660Proxy
/*!
******************************************************************************/
class DJ660Proxy : public PrinterProxy
{
public:
    DJ660Proxy() : PrinterProxy(
        "DJ6xx",                    // family name
        "DESKJET 66\0"                          // DeskJet 66x Series
        "DESKJET 67\0"                          // DeskJet 67x Series
        "DESKJET 68\0"                          // DeskJet 68x Series
        "DESKJET 60\0"                           // DeskJet 6xx Series
        "e-printer e20\0"                       // ePrinter
#ifdef APDK_MLC_PRINTER
        "OfficeJet Series 5\0"                  // OfficeJet Series 500
        "OfficeJet Series 6\0"                  // OfficeJet Series 600
        "Printer/Scanner/Copier 3\0"            // PSC 300 Series
        "OfficeJet Series 7\0"                  // OfficeJet Series 700
#endif
    ) {m_iPrinterType = eDJ6xx;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ660(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ6xx;}
	inline unsigned int GetModelBit() const { return 0x100000;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_DJ660_H
