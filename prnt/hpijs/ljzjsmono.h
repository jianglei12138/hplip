/*****************************************************************************\
  ljzjsmono.h : Interface for the LJZjsMono class

  Copyright (c) 1996 - 2007, HP Co.
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


#ifndef APDK_LJZJS_MONO_H
#define APDK_LJZJS_MONO_H

#ifdef APDK_LJZJS_MONO

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class LJZjsMono : public LJZjs
{
public:
    LJZjsMono (SystemServices* pSS, int numfonts = 0, BOOL proto = FALSE);
    ~LJZjsMono ();

    virtual DRIVER_ERROR Encapsulate (const RASTERDATA *pRasterData, BOOL bLastPlane);
    virtual DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter = TRUE);

protected:

#ifdef APDK_HP_UX
    virtual LJZjsMono & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

private:
    virtual DRIVER_ERROR    EndPage ();
    virtual DRIVER_ERROR    SendPlaneData (int iPlaneNumber, HPLJZjsJbgEncSt *se, HPLJZjcBuff *pcBuff, BOOL bLastStride);

}; // LJZjsMono

class LJZjsMonoDraftGrayMode : public GrayMode
{
public:
	LJZjsMonoDraftGrayMode ();
};	// LJZjsMonoDraftGrayMode

class LJZjsMonoNormalGrayMode : public GrayMode
{
public:
    LJZjsMonoNormalGrayMode ();
}; // LJZjsMonoNormalGrayMode

//! LJZjsMonoProxy
/*!
******************************************************************************/
class LJZjsMonoProxy : public PrinterProxy
{
public:
    LJZjsMonoProxy() : PrinterProxy(
        "LJZjsMono",
        "HP LaserJet 1000\0"
        "HP LaserJet 1005\0"
        "HP LaserJet 1018\0"
        "HP LaserJet 1020\0"
        "HP LaserJet 1022\0"
        "HP LaserJet P2035\0"
        "HP LaserJet P1102\0"
		"HP LaserJet P1566\0"
		"HP LaserJet P1606\0"
        "HP LaserJet Professional M1136\0"
        "HP LaserJet Professional M1132\0"
        "HP LaserJet Professional M1212nf\0"
    ) {m_iPrinterType = eLJZjsMono;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new LJZjsMono(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eLJZjsMono;}
	inline unsigned int GetModelBit() const { return 0x40;}
};

APDK_END_NAMESPACE

#endif // APDK_LJZJS_MONO
#endif //APDK_LJZJS_MONO_H

