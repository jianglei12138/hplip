/*****************************************************************************\
  dj850.h : Interface for the DJ850 class

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


#ifndef APDK_DJ850_H
#define APDK_DJ850_H

APDK_BEGIN_NAMESPACE

//extern char *ModelString[MAX_ID_STRING];

/*!
\internal
*/
class DJ850 : public Printer
{
public:
    DJ850(SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);

    virtual Header* SelectHeader(PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo();
    virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    virtual DISPLAY_STATUS ParseError(BYTE status_reg);

    Compressor* CreateCompressor(unsigned int RasterSize);

    virtual BOOL UseGUIMode(PrintMode* pPrintMode);

#ifdef APDK_HP_UX
protected:
    virtual DJ850 & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

}; //DJ850


class DJ850Mode1 : public PrintMode
{
public:
    DJ850Mode1();
}; //DJ850Mode1

class DJ850Mode3 : public PrintMode
{
public:
    DJ850Mode3();
}; //DJ850Mode3

class DJ850Mode4 : public GrayMode
{
public:
    DJ850Mode4();
}; //DJ850Mode4

class DJ850Mode5 : public GrayMode
{
public:
    DJ850Mode5();
}; //DJ850Mode5

#if defined(APDK_DJ850)
//! DJ850Proxy
/*!
******************************************************************************/
class DJ850Proxy : public PrinterProxy
{
public:
    DJ850Proxy() : PrinterProxy(
        "DJ850",                    // family name
        "DESKJET 85\0"                          // DeskJet 85x Series
        "DESKJET 87\0"                          // DeskJet 87x Series
#ifdef APDK_MLC_PRINTER
        "OFFICEJET PRO 115\0"                   // OfficeJet PRO 1150
#endif
    ) {m_iPrinterType = eDJ850;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ850(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ850;}
	inline unsigned int GetModelBit() const { return 0x8;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_DJ850_H
