/*****************************************************************************\
  apollo2xxx.h : Interface for the Apollo2xxx class

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


#ifndef APDK_APOLLO2xxx_H
#define APDK_APOLLO2xxx_H

APDK_BEGIN_NAMESPACE
/*!
\internal
*/
class Apollo2xxx : public Printer
{
public:
    Apollo2xxx(SystemServices* pSS, BOOL proto=FALSE);

    Header* SelectHeader(PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo();
    virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
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

#ifdef APDK_HP_UX
protected:
    virtual Apollo2xxx & operator = (Printer &rhs)
    {
        return *this;
    }
#endif

}; //Apollo2xxx


class Mode2xxxPhoto : public PrintMode
{
public:
    Mode2xxxPhoto();
}; //Mode2xxxPhoto


class Mode2xxxColor : public PrintMode
{
public:
    Mode2xxxColor();

}; //Mode2xxxColor


class GrayMode2xxx : public PrintMode
{
public:
    GrayMode2xxx();
}; //GrayMode2xxx

#ifdef APDK_EXTENDED_MEDIASIZE
class Mode2xxxDraftGrayK : public PrintMode
{
public:
    Mode2xxxDraftGrayK();
}; //Mode2xxxDraftGrayK

class Mode2xxxDraftColorKCMY : public PrintMode
{
public:
    Mode2xxxDraftColorKCMY();
}; //Mode2xxxDraftKCMYColor

class Mode2xxxDraftColorCMY : public PrintMode
{
public:
    Mode2xxxDraftColorCMY();
}; //Mode2xxxDraftCMYColor

class Mode2xxxBestGrayK : public GrayMode
{
public:
    Mode2xxxBestGrayK();
}; //Mode2xxxBestGrayK
#endif // APDK_EXTENDED_MEDIASIZE

#ifdef APDK_APOLLO2XXX
//! Apollo2xxxProxy
/*!
******************************************************************************/
class Apollo2xxxProxy : public PrinterProxy
{
public:
    Apollo2xxxProxy() : PrinterProxy(
        "AP2xxx",                       // family name
        "APOLLO P-22\0"                                 // Apollo P-22xx series
    ) {m_iPrinterType = eAP2xxx;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new Apollo2xxx(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eAP2xxx;}
	inline unsigned int GetModelBit() const { return 0x4000;}
};
#endif

APDK_END_NAMESPACE

#endif  //APDK_APOLLO2xx_H
