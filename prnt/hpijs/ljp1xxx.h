/*****************************************************************************\
  ljp1xxx.h : Interface for the LJP1XXX class

  Copyright (c) 2008, HP Co.
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


#ifndef APDK_LJP1XXX_H
#define APDK_LJP1XXX_H

#ifdef APDK_LJM1005

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapGRAY_K_6x6x1[9 * 9 * 9];

class LJP1XXXBestGrayMode : public GrayMode
{
public:
    LJP1XXXBestGrayMode () : GrayMode (ulMapGRAY_K_6x6x1)

    {
    //  Data sent at 600x600x2, printer outputs at 600x600x2

        ResolutionX[0] =
        ResolutionY[0] = 600;
        BaseResX =
        BaseResY = 600;
	    TextRes  = 600;
        MixedRes = FALSE;
        bFontCapable = FALSE;
        Config.bCompress = FALSE;
        theQuality = qualityPresentation;
        pmQuality  = QUALITY_BEST;
    }
}; // LJP1XXXBestGrayMode

/*!
\internal
*/
class LJP1XXX : public LJM1005
{
public:
    LJP1XXX (SystemServices* pSS, int numfonts = 0, BOOL proto = FALSE) : LJM1005 (pSS)
    {
        pMode[ModeCount] = new LJP1XXXBestGrayMode ();
        ModeCount++;
    }
protected:

#ifdef APDK_HP_UX
    virtual LJP1XXX & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

private:
    int GetOutputResolutionY ()
    {
        return 600;
    }
}; // LJP1XXX

//! LJP1XXXProxy
/*!
******************************************************************************/
class LJP1XXXProxy : public PrinterProxy
{
public:
    LJP1XXXProxy() : PrinterProxy(
        "LJP1XXX",
        "HP LaserJet P1005\0"
        "HP LaserJet P1006\0"
        "HP LaserJet P1007\0"
        "HP LaserJet P1008\0"
        "HP LaserJet P1009\0"
    ) {m_iPrinterType = eLJP1XXX;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new LJP1XXX(pSS); }
    inline PRINTER_TYPE GetPrinterType() const { return eLJP1XXX;}
    inline unsigned int GetModelBit() const { return 0x40;}
};

APDK_END_NAMESPACE

#endif // APDK_LJP1XXX
#endif //APDK_LJP1XXX_H
