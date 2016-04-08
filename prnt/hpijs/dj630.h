/*****************************************************************************\
  dj630.h : Interface for the DJ630 class

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


#ifndef APDK_DJ630_H
#define APDK_DJ630_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class DJ630 : public Printer
{
public:
    DJ630(SystemServices* pSS, BOOL proto=FALSE);

    Header* SelectHeader(PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo();
    virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    virtual PEN_TYPE DefaultPenSet();
	virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
	{
		if (ps == A4)
			fMargins[0] = (float) 0.135;
		else
			fMargins[0] = (float) 0.25;	// Left Margin
		fMargins[1] = fMargins[0];      // Right Margin
		fMargins[2] = (float) 0.125;	// Top Margin
		fMargins[3] = (float) 0.67;		// Bottom Margin
		return TRUE;
	}

#ifdef APDK_HP_UX
protected:
    virtual DJ630& operator = (Printer& rhs)
    {
        return *this;
    }
#endif

}; //DJ630


class Mode630Photo : public PrintMode
{
public:
    Mode630Photo();
}; //Mode630Photo


class Mode630Color : public PrintMode
{
public:
    Mode630Color();
}; //Mode630Color


class GrayMode630 : public GrayMode
{
public:
    GrayMode630(uint32_t *map);
}; //GrayMode630

#ifdef APDK_EXTENDED_MEDIASIZE
class Mode630DraftGrayK : public GrayMode
{
public:
    Mode630DraftGrayK();
}; //Mode630DraftGrayK

class Mode630DraftColorKCMY : public PrintMode
{
public:
    Mode630DraftColorKCMY();
}; //Mode630DraftKCMYColor

class Mode630DraftColorCMY : public PrintMode
{
public:
    Mode630DraftColorCMY();
}; //Mode630DraftCMYColor

class Mode630BestGrayK : public GrayMode
{
public:
    Mode630BestGrayK();
}; //Mode630BestGrayK
#endif // APDK_EXTENDED_MEDIASIZE

#ifdef APDK_DJ630
//! DJ630Proxy
/*!
******************************************************************************/
class DJ630Proxy : public PrinterProxy
{
public:
    DJ630Proxy() : PrinterProxy(
        "DJ630",                    // family name
        "DESKJET 63\0"                                  // DeskJet 63x Series
        "DESKJET 65\0"                                  // DeskJet 65x Series
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = eDJ630;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ630(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ630;}
	inline unsigned int GetModelBit() const { return 0x8000;}
};
#endif

APDK_END_NAMESPACE

#endif // APDK_DJ630_H
