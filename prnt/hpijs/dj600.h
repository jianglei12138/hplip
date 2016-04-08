/*****************************************************************************\
  dj600.h : Interface for the DJ600 class

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


#ifndef APDK_DJ600_H
#define APDK_DJ600_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class DJ600 : public DJ6XX
{
public:
    DJ600(SystemServices* pSS, BOOL proto=FALSE);

    Header* SelectHeader(PrintContext* pc) ;
    DRIVER_ERROR VerifyPenInfo();
    virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    virtual PEN_TYPE DefaultPenSet();

}; //DJ600


class Mode600 : public PrintMode
{
public:
    Mode600();

}; //Mode600

#ifdef APDK_EXTENDED_MEDIASIZE
class Mode600DraftGrayK : public GrayMode
{
public:
    Mode600DraftGrayK();
}; //Mode600DraftGrayK

class Mode600DraftColor : public PrintMode
{
public:
    Mode600DraftColor();
}; //Mode600DraftColor

class Mode600BestGrayK : public GrayMode
{
public:
    Mode600BestGrayK();
}; //Mode600BestGrayK
#endif // APDK_EXTENDED_MEDIASIZE
#ifdef APDK_DJ600
//! DJ600Proxy
/*!
******************************************************************************/
class DJ600Proxy : public PrinterProxy
{
public:
    DJ600Proxy() : PrinterProxy(
        "DJ600",                    // family name
        "DESKJET 600\0"                             // DeskJet 600
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = eDJ6xx;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ600(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ6xx;}
	inline unsigned int GetModelBit() const { return 0x200000;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_DJ600_H
