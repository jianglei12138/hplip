/*****************************************************************************\
  pscript.h : Interface for the PScript class

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


#ifndef APDK_PSCRIPT_H
#define APDK_PSCRIPT_H

#ifdef APDK_PSCRIPT

extern "C"
{
#include "jpeglib.h"
}
#include <setjmp.h>

APDK_BEGIN_NAMESPACE

#ifdef  APDK_HIGH_RES_MODES
#define PS_BASE_RES 300
#else
#define PS_BASE_RES 150
#endif


typedef struct _StrList
{
	char *pPSString;
	struct _StrList *next;
} StrList;

/*!
\internal
*/
class PScript : public Printer
{
public:
    PScript (SystemServices* pSS,int numfonts=0, BOOL proto=FALSE);
	~PScript ();

    PrintMode* GetMode (int index);

    virtual Header* SelectHeader (PrintContext* pc);
    virtual DRIVER_ERROR VerifyPenInfo ();
    virtual DRIVER_ERROR ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter=TRUE);
    virtual DISPLAY_STATUS ParseError (BYTE status_reg);
    virtual DRIVER_ERROR Encapsulate (const RASTERDATA* InputRaster, BOOL bLastPlane);
    inline virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode) {return FALSE;}
    virtual DRIVER_ERROR Flush (int FlushSize);
    DRIVER_ERROR SkipRasters (int nBlankRasters);

    virtual int PrinterLanguage ()
    {
        return 10;	// PostScript
    }

    virtual DRIVER_ERROR SaveText (const char *psStr, int iPointSize, int iX, int iY,
                                   const char *pTextString, int iTextStringLen,
                                   BOOL bUnderline);

    virtual BOOL UseCMYK(unsigned int iPrintMode) { return FALSE;}

    DRIVER_ERROR SendText (int iCurYPos);
    void FreeList ();
    void            JpegData (BYTE *buffer, int iSize);

protected:

#ifdef APDK_HP_UX
    virtual PScript & operator = (Printer& rhs)
    {
        return *this;
    }
#endif

private:
    DRIVER_ERROR    SendImage ();
    void            AddRaster (BYTE *pbyRow, int iLength);
    void            JpegRaster ();
    int             EncodeJpeg ();
    DRIVER_ERROR    StartJpegCompression ();
    void            FinishJpegCompression ();
    StrList         *m_pHeadPtr;
    StrList         *m_pCurItem;
    PrintContext    *m_pPC;
    struct jpeg_compress_struct m_cinfo;
    struct jpeg_error_mgr       m_jerr;
    jmp_buf                     m_setjmp_buffer;

    unsigned int     m_iRowNumber;
    int     m_iRowWidth;
    int     m_iJpegBufferPos;
    int     m_iJpegBufferSize;
    BYTE    *m_pbyInputRaster;
    BYTE    *m_pbyJpegBuffer;
    BYTE    *m_pbyEncodedBuffer;
    int     m_iPageNumber;
    COLORMODE    m_eColorMode;
}; // PScript

class PScriptDraftMode : public PrintMode
{
public:
	PScriptDraftMode ();
};	// PScriptDraftMode

class PScriptNormalMode : public PrintMode
{
public:
    PScriptNormalMode ();
}; // PScriptNormalMode

class PScriptGrayMode : public PrintMode
{
public:
	PScriptGrayMode ();
}; // PScriptGrayMode

#ifdef APDK_PSCRIPT
//! PScriptProxy
/*!
******************************************************************************/
class PScriptProxy : public PrinterProxy
{
public:
    PScriptProxy() : PrinterProxy(
    "PostScript",                   // family name
    "postscript\0"                  // models
    "HP Color LaserJet 2605\0"
    "HP Color LaserJet 2605dn\0"
#ifdef APDK_MLC_PRINTER
#endif
    ) {m_iPrinterType = ePScript;}
    inline Printer* CreatePrinter(SystemServices* pSS) const { return new PScript(pSS); }
    inline PRINTER_TYPE GetPrinterType() const { return ePScript;}
    inline unsigned int GetModelBit() const { return 0x10;}
};
#endif

APDK_END_NAMESPACE

#endif // APDK_PSCRIPT
#endif //APDK_PSCRIPT_H
