/*****************************************************************************\
  dj400.h : Interface for the DJ400 class

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


#ifndef APDK_DJ400_H
#define APDK_DJ400_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class DJ400 : public Printer
{
public:
    DJ400(SystemServices* pSS,BOOL proto=FALSE);

#if defined(APDK_FONTS_NEEDED)
    Font* RealizeFont(const int index,const BYTE bSize,
                        const TEXTCOLOR eColor=BLACK_TEXT,
                        const BOOL bBold=FALSE,const BOOL bItalic=FALSE,
                        const BOOL bUnderline=FALSE);
#endif
    Header* SelectHeader(PrintContext* pc);
    DRIVER_ERROR VerifyPenInfo();
    virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    BOOL TopCoverOpen(BYTE status_reg);
    DRIVER_ERROR CleanPen();
    virtual DISPLAY_STATUS ParseError(BYTE status_reg);
    virtual PEN_TYPE DefaultPenSet();
	virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
	{
		if (ps == A4)
			fMargins[0] = (float) 0.135;
		else
			fMargins[0] = (float) 0.25;	// Left Margin
		fMargins[1] = fMargins[0];		// Right Margin
		fMargins[2] = (float) 0.125;	// Top Margin
		fMargins[3] = (float) 0.67;		// Bottom Margin
		return TRUE;
	}

protected:

#ifdef APDK_HP_UX
    virtual DJ400& operator = (Printer& rhs)
    {
        return *this;
    }
#endif

    virtual DRIVER_ERROR GetC32Status(BYTE *pStatusString, int *pLen);
    unsigned long last_C32_status;	/* XXX unused? */

}; //DJ400


class Mode400 : public PrintMode
{
public:
    Mode400();

}; //Mode400

#ifdef APDK_DJ400

//! DJ400Proxy
/*!
******************************************************************************/
class DJ400Proxy : public PrinterProxy
{
public:
    DJ400Proxy() : PrinterProxy(
        "DJ400",                    // family name
        "HP DeskJet 4\0"                            // DeskJet 4xx Series
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = eDJ400;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ400(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ400;}
	inline unsigned int GetModelBit() const { return 0x800000;}
};

#endif

APDK_END_NAMESPACE

#endif //APDK_DJ400_H
