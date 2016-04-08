/*****************************************************************************\
  printer.h : Interface for the Printer class

  Copyright (c) 1996 - 2008, HP Co.
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


#ifndef APDK_PRINTER_H
#define APDK_PRINTER_H

#include "modes.h"

APDK_BEGIN_NAMESPACE

////////////////////////////////////////////////////////////////////////////
// PRINTER classes

#define MAX_PRINTER_FONTS 4
#define NUM_DJ400_FONTS 3  // ultimately obsolete these values since
#define NUM_DJ6XX_FONTS 4  // number of fonts is flexible

#define MAX_COMPATIBLE_PENS MAX_PEN_TYPE + 1

#define CANCEL_BUTTON_CHECK_THRESHOLD 150000  // will poll printer's cancel button state every
                                              // time this quantity of bytes is written out

class Compressor;

//ClassName
//!
/*!
\internal
******************************************************************************/
class PrintMode
{
friend class Job;
friend class Header;
friend class PrintContext;
friend class Translator;
friend class GraphicsTranslator;
friend class Halftoner;
friend class Printer;
friend class Tester;
#ifdef APDK_DJ9xxVIP
friend class DJGenericVIP;
#endif
#ifdef APDK_DJ3320
friend class DJ4100;
#endif
public:
    PrintMode (uint32_t *map1,uint32_t *map2=(uint32_t*)NULL);
    virtual ~PrintMode ()
    {
    }

    BOOL Compatible(PEN_TYPE pens);
    inline BOOL ColorCompatible(COLORMODE color) { return (color == pmColor); }
    inline BOOL QualityCompatible(QUALITY_MODE eQuality) { return (eQuality == pmQuality); }
    virtual inline BOOL MediaCompatible(MEDIATYPE eMedia) { return (eMedia == pmMediaType); }
    inline QUALITY_MODE GetQualityMode() { return pmQuality;}
    inline MEDIATYPE GetMediaType() { return pmMediaType; }
    void GetValues (QUALITY_MODE& eQuality, MEDIATYPE& eMedia, COLORMODE& eColor, BOOL& bDeviceText);

// The resolutions can be different for different planes
    unsigned int ResolutionX[MAXCOLORPLANES];
    unsigned int ResolutionY[MAXCOLORPLANES];

    unsigned int ColorDepth[MAXCOLORPLANES];

    MediaType medium;

    BOOL bFontCapable;

//    char ModeName[12];

    PipeConfigTable Config;
    unsigned int dyeCount;      // number of inks in the pen(s)
#ifdef APDK_AUTODUPLEX
    BOOL bDuplexCapable;
    void SetDuplexMode (DUPLEXMODE duplexmode)
    {
        DuplexMode = duplexmode;
        if (DuplexMode != DUPLEXMODE_NONE)
            bFontCapable = FALSE;
    };

    DUPLEXMODE QueryDuplexMode ()
    {
        return DuplexMode;
    };
#endif // APDK_AUTODUPLEX

    DRIVER_ERROR SetMediaType (MEDIATYPE eMediaType)
    {
        if (MediaCompatible (eMediaType))
        {
            pmMediaType = eMediaType;
            return NO_ERROR;
        }
        return WARN_MODE_MISMATCH;
    }

protected:
    Quality theQuality;
#ifdef APDK_AUTODUPLEX
    DUPLEXMODE DuplexMode;
#endif

    ColorMap cmap;

    unsigned int BaseResX,BaseResY, TextRes;
    BOOL MixedRes;

    unsigned char* BlackFEDTable;
    unsigned char* ColorFEDTable;


    PEN_TYPE CompatiblePens[MAX_COMPATIBLE_PENS];
    QUALITY_MODE pmQuality;
    MEDIATYPE pmMediaType;
    COLORMODE pmColor;

    unsigned int myIndex;   // record what index this mode has within the Printer

}; //PrintMode


//ClassName
//!
/*!
******************************************************************************/
class GrayMode : public PrintMode
{
public:
    GrayMode(uint32_t *map);
}; //GrayMode


//ClassName
//!
/*!
******************************************************************************/
class CMYGrayMode : public GrayMode
{
public:
    CMYGrayMode(uint32_t *map);
}; //CMYGrayMode

//ClassName
//!
/*!
******************************************************************************/
class KCMYGrayMode : public GrayMode
{
public:
    KCMYGrayMode(uint32_t *map);
}; //KCMYGrayMode

//Printer
//! Encapulate printer functionality
/*!
\internal
******************************************************************************/
class Printer
{
friend class Job;
friend class Tester;
public:

    Printer(SystemServices* pSys, int numfonts,
            BOOL proto = FALSE);

    virtual ~Printer();

    DRIVER_ERROR constructor_error;

    virtual unsigned int GetModeCount(void) { return ModeCount; }
    virtual PrintMode* GetMode(unsigned int index);


#if defined(APDK_FONTS_NEEDED)
    ReferenceFont* EnumFont(int& iCurrIdx);

    virtual Font* RealizeFont(const int index, const BYTE bSize,
                        const TEXTCOLOR eColor = BLACK_TEXT,
                        const BOOL bBold = FALSE,
                        const BOOL bItalic = FALSE,
                        const BOOL bUnderline = FALSE);
#endif

    virtual DRIVER_ERROR Send(const BYTE* pWriteBuff);
    virtual DRIVER_ERROR Send(const BYTE* pWriteBuff, DWORD dwWriteLen);

    virtual DRIVER_ERROR Flush(int FlushSize = MAX_RASTERSIZE); // flush printer input buffer

    // select Header component of Translator
    virtual Header* SelectHeader(PrintContext* pc) = 0;


    virtual Compressor* CreateCompressor(unsigned int RasterSize);

	virtual Compressor* CreateBlackPlaneCompressor (unsigned int RasterSize, BOOL bVIPPrinter = FALSE);

    virtual DISPLAY_STATUS ParseError(BYTE status_reg);

    virtual DRIVER_ERROR CleanPen();

    virtual DRIVER_ERROR PagesPrinted(unsigned int& count)  // read pagecount from printer memory
          { return UNSUPPORTED_FUNCTION; }   // default behavior for printers without the ability


    // DEVID stuff
    IO_MODE IOMode;

    int iNumFonts;          // size of fontarray
    BOOL bCheckForCancelButton;
    unsigned long ulBytesSentSinceCancelCheck;	/* XXX unused? */

    virtual BOOL UseGUIMode(PrintMode* pPrintMode) { return FALSE; }
    virtual BOOL UseCMYK(unsigned int iPrintMode) { return TRUE;}

    virtual BOOL PhotoTrayPresent(BOOL bQueryPrinter)
        { return FALSE; }

    virtual PHOTOTRAY_STATE PhotoTrayEngaged (BOOL bQueryPrinter)
        { return DISENGAGED; }

    //! Returns TRUE if a hagaki feed is present in printer.
    virtual BOOL HagakiFeedPresent(BOOL bQueryPrinter) 
		{ return FALSE; }

#ifdef APDK_AUTODUPLEX
    //!Returns TRUE if duplexer and hagaki feed (combined) unit is present in printer.
    virtual BOOL HagakiFeedDuplexerPresent(BOOL bQueryPrinter)
		{ return FALSE; }
#endif

    virtual PAPER_SIZE MandatoryPaperSize()
        { return UNSUPPORTED_SIZE; }    //code for "nothing mandatory"

    // tells currently installed pen type
    inline virtual DRIVER_ERROR ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter = TRUE)
    {
        ePen = NO_PEN;
        return NO_ERROR;
    }

    inline BOOL VIPPrinter() {return m_bVIPPrinter;}

	inline virtual BOOL SupportSeparateBlack (PrintMode *pCurrentMode) {return TRUE;}

    PEN_TYPE ePen;
    uint32_t *CMYMap;

    virtual DRIVER_ERROR CheckInkLevel() { return NO_ERROR; }

    virtual PEN_TYPE DefaultPenSet() { return BOTH_PENS; }
    // need ParsePenInfo to remember this
    PEN_TYPE ActualPens() { return ePen; }
    virtual DRIVER_ERROR SetPens(PEN_TYPE eNewPen);

    // function to set PrintMode indices, which can't be done in base constructor
    // so is called from PrintContext::SelectDevice
    void SetPMIndices();

    virtual DRIVER_ERROR SkipRasters (int nBlankRasters)    // needed for crossbow
    {
        return NO_ERROR;
    }

    virtual DRIVER_ERROR Encapsulate (const RASTERDATA* InputRaster, BOOL bLastPlane);

    virtual BOOL FullBleedCapable (PAPER_SIZE ps, FullbleedType  *fbType, float *xOverSpray, float *yOverSpray,
                                   float *fLeftOverSpray = NULL, float *fTopOverSpray = NULL)
    {
		*fbType = fullbleedNotSupported;
        return FALSE;
    }

/*
 *	Some printers may require different margins than the default initialized in
 *	page size metrics table. If so, Getmargins should return TRUE and return the
 *	margin values in Left, Right, Top and Bottom in that order in the array fMargins.
 *	If default values are acceptable, return FALSE.
 *	At present, all 6xx family have larger bottom margin than the default 0.5 inch.
 */

	virtual BOOL GetMargins (PAPER_SIZE ps, float *fMargins)
	{
		return FALSE;
	}

/*
 *	Malibu based printers do not handle media sense and full bleed simultaneously.
 *	For those printers, media type and quality have to be set explicitly.
 */

	virtual void AdjustModeSettings (BOOL bDoFullBleed, MEDIATYPE ReqMedia,
									 MediaType *medium, Quality *quality)
	{
		return;
	}

	virtual int	PrinterLanguage ()
	{
		return 0;		// 0 - PCL, 10 - PostScript
	}

	virtual DRIVER_ERROR SaveText (const char *psStr, int iPointSize, int iX, int iY,
								   const char *pTextString, int iTextStringLen,
								   BOOL bUnderline)
	{
		return NO_ERROR;
	}


#ifdef APDK_AUTODUPLEX
    BOOL    bDuplexCapable;
    BOOL    RotateImageForBackPage ()
    {
        return m_bRotateBackPage;
    };

#endif

    virtual DRIVER_ERROR    SendPerPageHeader (BOOL bLastPage)
    {
        return NO_ERROR;
    }
    virtual DRIVER_ERROR    SetHint (PRINTER_HINT eHint, int iValue)
    {
        return NO_ERROR;
    }
    virtual int GetHint (PRINTER_HINT eHint)
    {
        return 0;
    }

    virtual DRIVER_ERROR    AddPJLHeader ()
    {
        return NO_ERROR;
    }

protected:
    SystemServices* pSS;
#if defined(APDK_FONTS_NEEDED)
    ReferenceFont* fontarray[MAX_PRINTER_FONTS+1];
#endif

    int InSlowPollMode;
    int iTotal_SLOW_POLL_Count; // total SLOW_POLLs (multiple of iMax_SLOW_POLL_Count)
    int iMax_SLOW_POLL_Count;   // time-out value at which point we do error check
    BOOL ErrorTerminationState;

    // buffering stuff
    const unsigned int iBuffSize;
    BYTE* pSendBuffer;
    unsigned int iCurrBuffSize;
    BOOL EndJob;

    virtual DRIVER_ERROR VerifyPenInfo() = 0;
    virtual BOOL TopCoverOpen(BYTE status_reg);
	virtual DATA_FORMAT GetDataFormat() { return RASTER_LINE; }
    DRIVER_ERROR SetPenInfo(char*& pStr, BOOL QueryPrinter);

    PrintMode* pMode[MAX_PRINTMODES];
    unsigned int ModeCount;
    BOOL m_bVIPPrinter;
    BOOL m_bStatusByPJL;
#ifdef  APDK_AUTODUPLEX
    BOOL m_bRotateBackPage;
#endif

    int m_iNumPages;

    virtual Printer& operator=(const Printer& rhs) {return *this;} // don't allow assignment

}; //Printer

APDK_END_NAMESPACE

#endif //APDK_PRINTER_H
