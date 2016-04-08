/*****************************************************************************\
  dj690.h : Interface for the DJ6xxPhoto class

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


#ifndef APDK_DJ690_H
#define APDK_DJ690_H

APDK_BEGIN_NAMESPACE

//extern char *ModelString[MAX_ID_STRING];

/*!
\internal
*/
class DJ6xxPhoto : public Printer
{
public:
    DJ6xxPhoto(SystemServices* pSS, BOOL proto=FALSE);

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

protected:
#ifdef APDK_HP_UX
    virtual DJ6xxPhoto & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

    BOOL PhotoPenOK;

}; //DJ6xxPhoto


class Mode690 : public PrintMode
{
public:
    Mode690();
}; //Mode690


class GrayMode690 : public PrintMode
{
public:
    GrayMode690();
}; //GrayMode690

#ifdef APDK_EXTENDED_MEDIASIZE
class Mode690DraftGrayK : public GrayMode
{
public:
    Mode690DraftGrayK();
}; //Mode690DraftGrayK

class Mode690DraftColor : public PrintMode
{
public:
    Mode690DraftColor();
}; //Mode690DraftColor

class Mode690BestGrayK : public GrayMode
{
public:
    Mode690BestGrayK();
}; //Mode690BestGrayK
#endif // APDK_EXTENDED_MEDIASIZE

#ifdef APDK_DJ6xxPhoto
//! DJ690Proxy
/*!
******************************************************************************/
class DJ6xxPhotoProxy : public PrinterProxy
{
public:
    DJ6xxPhotoProxy() : PrinterProxy(
        "DJ6xxPhoto",                   // family name
        "DESKJET 61\0"                                      // DeskJet 61x Series
        "DESKJET 64\0"                                      // DeskJet 64x Series
        "DESKJET 69\0"                                      // DeskJet 69x Series
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = eDJ6xxPhoto;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ6xxPhoto(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ6xxPhoto;}
	inline unsigned int GetModelBit() const { return 0x80000;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_DJ690_H
