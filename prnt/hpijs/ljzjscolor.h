/*****************************************************************************\
  ljzjscolor.h : Interface for the LJZjsColor class

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


#ifndef APDK_LJZJS_COLOR_H
#define APDK_LJZJS_COLOR_H

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class LJZjsColor : public LJZjs
{
public:
    LJZjsColor (SystemServices* pSS, int numfonts = 0, BOOL proto = FALSE);
    ~LJZjsColor ();

    virtual DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter = TRUE);
    virtual DRIVER_ERROR Encapsulate (const RASTERDATA *pRasterData, BOOL bLastPlane);

protected:

#ifdef APDK_HP_UX
    virtual LJZjsColor & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

private:
	bool IsLJZjsColor2Printer(SystemServices* pSS);
    virtual DRIVER_ERROR    EndPage ();
    virtual DRIVER_ERROR    SendPlaneData (int iPlaneNumber, HPLJZjsJbgEncSt *se, HPLJZjcBuff *pcBuff, BOOL bLastStride);
	virtual DRIVER_ERROR    SendPlaneData_LJZjsColor (int iPlaneNumber, HPLJZjsJbgEncSt *se, HPLJZjcBuff *pcBuff, BOOL bLastStride);
	virtual DRIVER_ERROR    SendPlaneData_LJZjsColor2 (int iPlaneNumber, HPLJZjsJbgEncSt *se, HPLJZjcBuff *pcBuff, BOOL bLastStride);		

}; // LJZjsColor

class LJZjsColorDraftGrayMode : public GrayMode
{
public:
	LJZjsColorDraftGrayMode ();
};	// LJZjsColorDraftGrayMode

class LJZjsColorNormalGrayMode : public GrayMode
{
public:
    LJZjsColorNormalGrayMode ();
}; // LJZjsColorNormalGrayMode

class LJZjsColorDraftColorMode : public PrintMode
{
public:
    LJZjsColorDraftColorMode ();
}; // LJZjsColorDraftColorMode

class LJZjsColorNormalColorMode : public PrintMode
{
public:
    LJZjsColorNormalColorMode ();
}; // LJZjsColorNormalColorMode

//! LJZjsColorProxy
/*!
******************************************************************************/
class LJZjsColorProxy : public PrinterProxy
{
public:
    LJZjsColorProxy() : PrinterProxy(
        "LJZjsColor",                   // family name
        "HP Color LaserJet 1600\0"     // models with null at end of each
        "HP Color LaserJet 2600n\0"
        "HP Color LaserJet CP1215\0"
		"HP LaserJet CP1025\0"
		"HP LaserJet CP1021\0"
		"HP LaserJet CP1022\0"
		"HP LaserJet CP1023\0"
		"HP LaserJet CP1025nw\0"
		"HP LaserJet CP1026nw\0"
		"HP LaserJet CP1027nw\0"
		"HP LaserJet CP1028nw\0"
    ) {m_iPrinterType = eLJZjsColor;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new LJZjsColor(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eLJZjsColor;}
	inline unsigned int GetModelBit() const { return 0x40;}
};

APDK_END_NAMESPACE

#endif //APDK_LJZJS_COLOR_H
