/*****************************************************************************\
  ljm1005.h : Interface for the LJM1005 class

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


#ifndef APDK_LJM1005_H
#define APDK_LJM1005_H

#ifdef APDK_LJM1005

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class LJM1005 : public LJZjs
{
public:
    LJM1005 (SystemServices* pSS, int numfonts = 0, BOOL proto = FALSE);
    ~LJM1005 ();

    virtual DRIVER_ERROR Encapsulate (const RASTERDATA *pRasterData, BOOL bLastPlane);
    virtual DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter = TRUE);
    virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
    {
        fMargins[0] = (float) 0.156;
        fMargins[1] = (float) 0.156;
        fMargins[2] = (float) 0.156;
        fMargins[3] = (float) 0.2;
        return TRUE;
    }

protected:

#ifdef APDK_HP_UX
    virtual LJM1005 & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

private:
    virtual DRIVER_ERROR    EndPage ();
    virtual DRIVER_ERROR    SendPlaneData (int iPlaneNumber, HPLJZjsJbgEncSt *se, HPLJZjcBuff *pcBuff, BOOL bLastStride);
    virtual int             GetOutputResolutionY ()
    {
        return 600;
    }

}; // LJM1005

class LJM1005DraftGrayMode : public GrayMode
{
public:
    LJM1005DraftGrayMode ();
};    // LJM1005DraftGrayMode

class LJM1005NormalGrayMode : public GrayMode
{
public:
    LJM1005NormalGrayMode ();
}; // LJM1005NormalGrayMode

//! LJM1005Proxy
/*!
******************************************************************************/
class LJM1005Proxy : public PrinterProxy
{
public:
    LJM1005Proxy() : PrinterProxy(
        "LJM1005",
        "LaserJet M1005\0"
        "HP LaserJet M1005\0"
        "HP LaserJet M1120\0"
        "HP LaserJet M1319\0"
        "HP LaserJet P1505\0"
        "HP LaserJet P2010\0"
        "HP LaserJet P2014\0"
        "HP LaserJet P2014n\0"
        "M1005\0"
    ) {m_iPrinterType = eLJM1005;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new LJM1005(pSS); }
    inline PRINTER_TYPE GetPrinterType() const { return eLJM1005;}
    inline unsigned int GetModelBit() const { return 0x40;}
};

APDK_END_NAMESPACE

#endif // APDK_LJM1005
#endif //APDK_LJM1005_H
