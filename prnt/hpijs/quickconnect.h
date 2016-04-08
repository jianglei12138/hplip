/*****************************************************************************\
  quickconnect.h : Interface for the QuickConnect class

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


#ifndef APDK_QUICKCONNECT_H
#define APDK_QUICKCONNECT_H

extern "C"
{
#include "jpeglib.h"
}

APDK_BEGIN_NAMESPACE

/*!
\internal
*/
class QuickConnect : public Printer
{
public:
    QuickConnect (SystemServices* pSS, BOOL proto=FALSE);
    ~QuickConnect ();

    Header *SelectHeader (PrintContext* pc);
    DRIVER_ERROR VerifyPenInfo ();
    DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
	inline virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode)
    {
        return FALSE;
    }

    virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
    {
        fMargins[0] = (float) 0.0;
        fMargins[1] = (float) 0.0;
        fMargins[2] = (float) 0.0;
        fMargins[3] = (float) 0.0;
        return TRUE;
    }
    virtual BOOL FullBleedCapable (PAPER_SIZE ps, FullbleedType  *fbType,
                                   float *xOverSpray, float *yOverSpray,
                                   float *fLeftOverSpray, float *fTopOverSpray)
    {
        *xOverSpray = (float) 0.0;
        *yOverSpray = (float) 0.0;
        if (fLeftOverSpray)
            *fLeftOverSpray = (float) 0.0;
        if (fTopOverSpray)
            *fTopOverSpray = (float) 0.0;
        *fbType = fullbleed4EdgeAllMedia;
        return TRUE;
    }

    virtual DRIVER_ERROR    SetHint (PRINTER_HINT eHint, int iValue);
    void    JpegData (BYTE *buffer, int size);
    DRIVER_ERROR Encapsulate (const RASTERDATA *InputRaster, BOOL bLastPlane);
    DRIVER_ERROR Flush (int iBufferSize);
    PAPER_SIZE MandatoryPaperSize ()
    {
        return PHOTO_5x7;
    }

protected:

#ifdef APDK_HP_UX
    virtual QuickConnect & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

private:
    int     MapPaperSize (PAPER_SIZE ePaperSize);
    int     MapMediaType (MEDIATYPE eMediaType);
    int     MapQualityMode (QUALITY_MODE eQualityMode);
    DRIVER_ERROR    AddExifHeader ();
    DRIVER_ERROR    Compress ();
    PrintContext    *m_pPC;

    int     m_iPhotoFix;
    int     m_iRemoveRedEye;
    int     m_iBorderless;
    int     m_iMaxFileSize;

    int     m_iRowNumber;
    int     m_iRowWidth;
    int     m_iJobId;
    int     m_iJpegBufferPos;
    int     m_iJpegBufferSize;
    int     m_iInputBufferSize;
    BYTE    *m_pbyInputBuffer;
    BYTE    *m_pbyJpegBuffer;
}; // QuickConnect


class QCAutomatic : public PrintMode
{
public:
    QCAutomatic ();
    inline BOOL MediaCompatible (MEDIATYPE eMedia)
    {
        return TRUE;
    }
}; // QCAutomatic


class QCFastNormal : public PrintMode
{
public:
    QCFastNormal ();
    inline BOOL MediaCompatible (MEDIATYPE eMedia)
    {
        return TRUE;
    }

}; // QCFastNormal

class QCNormal : public PrintMode
{
public:
    QCNormal ();
    inline BOOL MediaCompatible (MEDIATYPE eMedia)
    {
        return TRUE;
    }

}; // QCNormal

class QCBest : public PrintMode
{
public:
    QCBest ();
    inline BOOL MediaCompatible (MEDIATYPE eMedia)
    {
        return TRUE;
    }

}; // QCBest

#ifdef APDK_QUICKCONNECT
//! QuickConnectProxy
/*!
******************************************************************************/
class QuickConnectProxy : public PrinterProxy
{
public:
    QuickConnectProxy() : PrinterProxy (
        "QuickConnect",                     // family name
        "Photosmart A530\0"
        "Photosmart A630\0"
        "Photosmart A640\0"
    ) {m_iPrinterType = eQuickConnect;}
    inline Printer* CreatePrinter (SystemServices *pSS) const { return new QuickConnect (pSS); }
	inline PRINTER_TYPE GetPrinterType () const { return eQuickConnect;}
	inline unsigned int GetModelBit() const { return 0x10000;}
};
#endif

APDK_END_NAMESPACE

#endif //APDK_QUICKCONNECT_H
