/*****************************************************************************\
  dj9xx.h : Interface for the DJ9xx class

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


#ifndef APDK_DJ9XX_H
#define APDK_DJ9XX_H

APDK_BEGIN_NAMESPACE

//extern char *ModelString[MAX_ID_STRING];

/*!
\internal
*/
class DJ9xx : public Printer
{
public:
    DJ9xx(SystemServices* pSS, BOOL proto=FALSE);

    virtual ~DJ9xx();

    BOOL UseGUIMode(PrintMode* pPrintMode);
    Header* SelectHeader(PrintContext* pc);
    Compressor* CreateCompressor(unsigned int RasterSize);
    DISPLAY_STATUS ParseError(BYTE status_reg);
    DRIVER_ERROR VerifyPenInfo();

#if defined(APDK_FONTS_NEEDED)
    Font* RealizeFont(const int index,const BYTE bSize,
                        const TEXTCOLOR eColor=BLACK_TEXT,
                        const BOOL bBold=FALSE,const BOOL bItalic=FALSE,
                        const BOOL bUnderline=FALSE);
#endif

    virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);

    PAPER_SIZE MandatoryPaperSize();

    DRIVER_ERROR CleanPen();

    virtual BOOL PhotoTrayPresent(BOOL bQueryPrinter);

    virtual PHOTOTRAY_STATE PhotoTrayEngaged (BOOL bQueryPrinter);
    inline virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode)
    {
        if (pCurrentMode->medium != mediaPlain)
        {
            return FALSE;
        }
        return TRUE;
    }

protected:

#ifdef APDK_HP_UX
    virtual DJ9xx & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

    virtual BYTE PhotoTrayStatus(BOOL bQueryPrinter);

}; //DJ9xx


class DJ970Mode1 : public PrintMode
{
public:
    DJ970Mode1();
}; //DJ970Mode1


class DJ970Mode2 : public PrintMode
{
public:
    DJ970Mode2();
}; //DJ970Mode2


#ifdef APDK_EXTENDED_MEDIASIZE

class DJ970Mode3 : public GrayMode
{
public:
    DJ970Mode3 ();
}; //DJ970Mode3

class DJ970Mode4 : public PrintMode
{
public:
    DJ970Mode4 ();
};

class DJ970Mode5 : public PrintMode
{
public:
    DJ970Mode5 ();
};

class DJ970ModePres : public PrintMode
{
public:
    DJ970ModePres();

};

class DJ970ModePhotoPres : public PrintMode
{
public:
    DJ970ModePhotoPres();

};

#endif // APDK_EXTENDED_MEDIASIZE

#ifdef APDK_DJ9xx
//! DJ9xxProxy
/*!
******************************************************************************/
class DJ9xxProxy : public PrinterProxy
{
public:
    DJ9xxProxy() : PrinterProxy(
        "DJ9xx",                    // family name
        "DESKJET 91\0"              // DeskJet 91x Series
        "DESKJET 92\0"              // DeskJet 920
        "DESKJET 93\0"              // DeskJet 93x Series
        "DESKJET 94\0"              // DeskJet 94x Series
        "DESKJET 95\0"              // DeskJet 95x Series
        "DESKJET 97\0"              // DeskJet 97x Series
        "DESKJET 1120\0"            // DeskJet 1120
        "DESKJET 1125\0"            // DeskJet 1125
        "DESKJET 3810\0"            // DeskJet 3810
        "DESKJET 3816\0"            // DeskJet 3816
        "DESKJET 3820\0"            // DeskJet 3820
        "DESKJET 3822\0"            // DeskJet 3822
        "PHOTOSMART P1000\0"        // PSP 1000
        "PHOTOSMART P1100\0"        // PSP 1100
        "DESKJET 1220\0"            // DeskJet 1220
        "Deskjet 1280\0"         // Deskjet 1280
		"hp deskjet 9300\0"         // deskjet 9300
#ifdef APDK_MLC_PRINTER
        "OfficeJet K\0"             // OfficeJet K Series
        "OfficeJet V\0"             // OfficeJet V Series
        "OfficeJet G\0"             // OfficeJet G Series
        "PSC 7\0"                   // PSC 750
        "PSC 9\0"                   // PSC 900 Series
		"officejet 5100 series\0"   // officejet 5100 series
        "HP 2000C\0"
#endif
    ) {m_iPrinterType = eDJ9xx;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ9xx(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ9xx;}
	inline unsigned int GetModelBit() const { return 0x20000;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_DJ9XX_H
