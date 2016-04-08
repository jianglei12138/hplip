/*****************************************************************************\
  dj8xx.h : Interface for the DJ8xx class

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


#ifndef APDK_DJ8XX_H
#define APDK_DJ8XX_H

APDK_BEGIN_NAMESPACE

//extern char *ModelString[MAX_ID_STRING];

/*!
\internal
*/
class DJ8xx : public Printer
{
public:
    DJ8xx(SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);

    virtual Header* SelectHeader(PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo();
    virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    virtual DISPLAY_STATUS ParseError(BYTE status_reg);

    Compressor* CreateCompressor(unsigned int RasterSize);

    virtual BOOL UseGUIMode(PrintMode* pPrintMode);
    DRIVER_ERROR CleanPen();

#ifdef APDK_HP_UX
protected:
    virtual DJ8xx & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

}; //DJ8xx


class DJ895Mode1 : public PrintMode
{
public:
    DJ895Mode1();
}; //DJ895Mode1


class DJ895Mode2 : public PrintMode
{
public:
    DJ895Mode2();
}; //DJ895Mode2


class DJ895Mode3 : public PrintMode
{
public:
    DJ895Mode3();
}; //DJ895Mode3

class DJ895Mode4 : public GrayMode
{
public:
    DJ895Mode4();
}; //DJ895Mode4

#ifdef APDK_EXTENDED_MEDIASIZE

class DJ895Mode5 : public GrayMode
{
public:
    DJ895Mode5();
}; //DJ895Mode5

#endif //APDK_EXTENDED_MEDIASIZE


#if defined(APDK_DJ8xx)|| defined(APDK_DJ9xx)
//! DJ8xxProxy
/*!
******************************************************************************/
class DJ8xxProxy : public PrinterProxy
{
public:
    DJ8xxProxy() : PrinterProxy(
        "DJ8xx",                    // family name
        "DESKJET 81\0"                          // DeskJet 81x Series
        "DESKJET 83\0"                          // DeskJet 83x Series
        "DESKJET 84\0"                          // DeskJet 84x Series
        "DESKJET 88\0"                          // DeskJet 88x Series
        "DESKJET 895\0"                         // DeskJet 895
#ifdef APDK_MLC_PRINTER
        "OfficeJet T\0"                         // OfficeJet T Series
        "OFFICEJET R\0"                         // OfficeJet R Series
        "PSC 5\0"                               // PSC 500
#endif
    ) {m_iPrinterType = eDJ8xx;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ8xx(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ8xx;}
	inline unsigned int GetModelBit() const { return 0x40000;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_DJ8XX_H
