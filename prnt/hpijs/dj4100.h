/*****************************************************************************\
  dj4100.h : Interface for the DJ4100 printer class

  Copyright (c) 2003 - 2015, HP Co.
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


#ifndef APDK_DJ4100_H
#define APDK_DJ4100_H
#if defined(APDK_DJ3600) && defined (APDK_DJ3320)

APDK_BEGIN_NAMESPACE
extern unsigned char ucMapDJ4100_KCMY_6x6x1[];
extern unsigned char ucMapDJ4100_KCMY_Photo_BestV_12x12x1[];
extern unsigned char ucMapDJ4100_KCMY_Photo_BestA_12x12x1[];
extern BYTE *GetHT12x12x1_4100_Photo_Best ();
//DJ4100
//!
/*!
\internal
******************************************************************************/
class DJ4100 : public DJ3600
{
public:
    DJ4100 (SystemServices* pSS, BOOL proto = FALSE) : DJ3600 (pSS, proto)
    {
        m_iBytesPerSwing = 4;
        m_iLdlVersion = 2;
        m_iColorPenResolution = 600;
        AdjustResolution ();
        if (ePen == BOTH_PENS)
        {
            pMode[DEFAULTMODE_INDEX]->cmap.ulMap1 =
            pMode[DEFAULTMODE_INDEX]->cmap.ulMap2 = NULL;
            pMode[DEFAULTMODE_INDEX]->cmap.ulMap3 = (unsigned char *) ucMapDJ4100_KCMY_6x6x1;

            pMode[SPECIALMODE_INDEX]->cmap.ulMap1 =
            pMode[SPECIALMODE_INDEX]->cmap.ulMap2 = NULL;
            pMode[SPECIALMODE_INDEX]->cmap.ulMap3 = (unsigned char *) ucMapDJ4100_KCMY_Photo_BestA_12x12x1;
            pMode[SPECIALMODE_INDEX]->ColorFEDTable = GetHT12x12x1_4100_Photo_Best ();
        }
    }
    virtual void AdjustResolution ()
    {
        for (int i = 0; i < (int) ModeCount; i++)
        {
            if (pMode[i] && pMode[i]->ResolutionX[0] == 300)
            {
                for (int j = 0; j < 4; j++)
                {
                    pMode[i]->ResolutionX[j] = 
                    pMode[i]->ResolutionY[j] = 600;
                }
                pMode[i]->BaseResX       =
                pMode[i]->BaseResY       = 600;
            }
        }

    }
}; //DJ4100


#if defined(APDK_DJ3600) && defined (APDK_DJ3320)
//! DJ4100Proxy
/*!
******************************************************************************/
class DJ4100Proxy : public PrinterProxy
{
public:
    DJ4100Proxy() : PrinterProxy(
        "DJ4100",                               // family name
        "Deskjet D4100\0"                       // DeskJet 4100
        "Deskjet D4160\0"
        "Deskjet D42\0"
        "Deskjet D43\0"
        "Deskjet D16\0"
        "Deskjet D26\0"
        "Deskjet Ink Advant K109\0"
        "Deskjet Ink Advant K209\0"
    ) {m_iPrinterType = eDJ4100;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new DJ4100(pSS); }
	inline PRINTER_TYPE GetPrinterType() const { return eDJ4100;}
	inline unsigned int GetModelBit() const { return 0x2;}
};
#endif

APDK_END_NAMESPACE

#endif // defined(APDK_DJ3600) && defined (APDK_DJ3320)

#endif  // APDK_DJ4100_H
