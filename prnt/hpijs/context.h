/*****************************************************************************\
  context.h : Interface/Implimentation for the PrintContext class

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


#ifndef APDK_PRINTCONTEXT_H
#define APDK_PRINTCONTEXT_H

#define MEDIASIZE_PCL 0
typedef struct
{
    int      pcl_id;
    float    fPhysicalWidth;
    float    fPhysicalHeight;
    float    fPrintableWidth;
    float    fPrintableHeight;
    float    fPrintableStartX;
    float    fPrintableStartY;
} MediaAttributes;
typedef struct
{
    MediaAttributes    media_attributes;
} JobAttributes;

#include "printerfactory.h"

APDK_BEGIN_NAMESPACE

class PrintMode;
class Pipeline;
class Compressor;
class Header;
class Printer;
class ModeSet;

//PrintContext
//! Allows setting options of a print job based on specific device
/*! \class PrintContext context.h "hpprintapi.h"
This object serves two purposes. First, it encapsulates knowledge about all
devices supported in available driver components. It can be queried at runtime
to determine capabilities of the attached device. Second, it allows for
setting optional parameters of a print job, such as media size and margins,
use of color and data-resolution, etc.

The PrintContext is the second item created in the driver calling sequence,
following SystemServices. It provides the interface for inquiring about
supported devices, and for setting all optional properties of the imaging
pipeline as determined by given devices.

\sa Job SystemServices
******************************************************************************/
class PrintContext
{
friend class Job;         // access to private (non-instrumented) versions of routines
friend class Header;      // access to private (non-instrumented) versions of routines
friend class RasterSender;    // access to current printmode
friend class HeaderDJ990;  // to use GUITopMargin
friend class Header895;
friend class Tester;
friend class Header850;
friend class LDLEncap;
public:
    PrintContext(SystemServices *pSysServ,
                 unsigned int InputPixelsPerRow=0,
                 unsigned int OutputPixelsPerRow=0,
                 PAPER_SIZE ps = LETTER,
                 QUALITY_MODE eQuality = QUALITY_NORMAL,
                 MEDIATYPE eMedia = MEDIA_PLAIN,
                 COLORMODE eColorMode = COLOR,
                 BOOL bDeviceText = FALSE);

    virtual ~PrintContext();

    DRIVER_ERROR constructor_error;


    void Flush(int FlushSize);  // used in connection with SendPrinterReadyData

    // used when constructor couldn't instantiate printer (no DevID) -- instantiate now
    DRIVER_ERROR SelectDevice(const PRINTER_TYPE Model);
    DRIVER_ERROR SelectDevice(const char* szDeviceIDString);

    unsigned int GetModeCount();
    DRIVER_ERROR SelectPrintMode(const unsigned int index);
    DRIVER_ERROR SelectPrintMode(
        QUALITY_MODE eQuality = QUALITY_NORMAL,
        MEDIATYPE eMedia = MEDIA_PLAIN,
        COLORMODE eColorMode = COLOR,
        BOOL bDeviceText = FALSE
    );
    DRIVER_ERROR SetPenSet(PEN_TYPE ePen);

    // need to seriously consider the future of this method.  Returning the index of the
    // current print mode may not always work and certainly won't in the future with
    // more dynamic print modes. - JLM
    /*!
    \deprecated
    Retrieves an index of the currently selected print mode.
    */
    unsigned int CurrentPrintMode();

    // obsolete function retained for compatibility
    /*!
    \deprecated
    Returns the identifying string for the currently selected print mode.
    This method now returns an empty string for every print mode.
    */
    const char *GetModeName ()
    {
        return ("");
    }

    DRIVER_ERROR GetPrintModeSettings(
        QUALITY_MODE& eQuality,
        MEDIATYPE& eMedia,
        COLORMODE& eColorMode,
        BOOL& bDeviceText
    );

    PRINTER_TYPE SelectedDevice();


#if defined(APDK_FONTS_NEEDED)
    ReferenceFont* EnumFont(int& iCurrIdx);
        //  { return thePrinter->EnumFont(iCurrIdx); }
    virtual Font* RealizeFont(const int index,const BYTE bSize,
                            const TEXTCOLOR eColor=BLACK_TEXT,
                            const BOOL bBold=FALSE,const BOOL bItalic=FALSE,
                            const BOOL bUnderline=FALSE);
        //  { return thePrinter->RealizeFont(eFont,bSize,eColor,
        //                                  bBold,bItalic,bUnderline); }
#endif
    // return the enum for the next model(UNSUPPORTED when finished)
    PRINTER_TYPE EnumDevices(FAMILY_HANDLE& familyHandle) const;

    // PerformPrinterFunction (clean pen, etc.)
    // this is the preferred function to call
    DRIVER_ERROR PerformPrinterFunction(PRINTER_FUNC eFunc);

    DRIVER_ERROR PagesPrinted(unsigned int& count);
    ///////////////////////////////////////////////////////////////////////
    // routines to change settings
    DRIVER_ERROR SetPaperSize(PAPER_SIZE ps, BOOL bFullBleed = FALSE);
    // these are dependent on printer model in use, thus can err
    DRIVER_ERROR SetPixelsPerRow(unsigned int InputPixelsPerRow,
                                 unsigned int OutputPixelsPerRow=0);
    //
    // routines to query selections ///////////////////////////////////////

    //! Returns TRUE if printer has been selected.
    BOOL PrinterSelected() { return !(thePrinter==NULL); }

    //! Returns TRUE if the selected printer (and the current build) provides font support.
    BOOL PrinterFontsAvailable();    // return FALSE if no printer
	
	//! Returns TRUE if the selected printer (and the current build) provides separate 1 bit balck channel
	BOOL SupportSeparateBlack();

    //!Returns the width as set by SetPixelsPerRow.
    unsigned int InputPixelsPerRow() { return InputWidth; }

    //! Returns the current setting for output width.
    unsigned int OutputPixelsPerRow() { return OutputWidth; }

    //! Returns the currently set paper size.
    PAPER_SIZE GetPaperSize();

    //! Returns TRUE if a photo tray is present in printer.
    BOOL PhotoTrayPresent(BOOL bQueryPrinter);

    //!Returns current state of phototray, one of UNKNOWN, DISENGAGED or ENGAGED.

    PHOTOTRAY_STATE PhotoTrayEngaged (BOOL bQueryPrinter);

    //! Returns TRUE if a hagaki feed is present in printer.
    BOOL HagakiFeedPresent(BOOL bQueryPrinter);

#ifdef APDK_AUTODUPLEX
    //!Returns TRUE if duplexer and hagaki feed (combined) unit is present in printer.

    BOOL HagakiFeedDuplexerPresent(BOOL bQueryPrinter);
#endif

    const char* PrinterModel();
    const char* PrintertypeToString(PRINTER_TYPE pt); // returns string for use in UI

    unsigned int EffectiveResolutionX();       // res we need in current mode
    unsigned int EffectiveResolutionY();       // res we need in current mode

    // get settings pertaining to the printer
    // note:these return zero if no printer selected
    // all results in inches
    float PrintableWidth();
    float PrintableHeight();
    float PhysicalPageSizeX();
    float PhysicalPageSizeY();
    float PrintableStartX();
    float PrintableStartY();

    // SPECIAL API -- NOT TO BE USED IN CONNECTION WITH JOB
    DRIVER_ERROR SendPrinterReadyData(BYTE* stream, unsigned int size);

    DeviceRegistry* DR;     // unprotected for replay system

    DRIVER_ERROR SetMediaSource(MediaSource num);

	// GetMediaSource
	//! Return input media source bin
	/*!
	Used to get the bin number from which media will be loaded by the printer. This
	is relevant for those printers that have multiple input bins. All other printers
	will ignore the bin number. The typical bin numbers are
		1 - Upper Tray
		4 - Lower Tray
		7 - Auto Select
	Any value between 1 and 50 is valid where there are more than 2 trays.
	*****************************************************************************
	*/
    MediaSource GetMediaSource() { return m_MediaSource; }

    void            SetMediaSubtype (int iMediaSubtype)
    {
        m_iMediaSubtype = iMediaSubtype;
    }
    int             GetMediaSubtype ()
    {
        return m_iMediaSubtype;
    }
    void            SetMechOffset (int iMechOffset)
    {
        m_iMechOffset = iMechOffset;
    }

    // needed for testing
//    PEN_TYPE GetCompatiblePen(unsigned int num);    // get CompatiblePens of Printer

#ifdef APDK_AUTODUPLEX
    //! Set two-sided printing option.
    /*!  This has no effect on printers that do not support an automatic duplexer mechanism.
    Two sided-printing option can be one of
        DUPLEXMODE_NONE   - one sided printing
        DUPLEXMODE_TABLET - binding on short edge
        DUPLEXMODE_BOOK   - binding on long edge
	*****************************************************************************
	*/   
    BOOL  SelectDuplexPrinting (DUPLEXMODE duplexmode);

    //! Returns current setting for two-sided printing option
    DUPLEXMODE QueryDuplexMode ();

    //! Does back page needs to be rotated?
    /*! When automatic two-sided printing is enabled for long edge binging, application may have to
    rotate the back page by 180 degrees.
	*****************************************************************************
	*/   
    BOOL  RotateImageForBackPage ();
#endif

#ifdef APDK_EXTENDED_MEDIASIZE
    BOOL  SetCustomSize (float width, float height);
#endif
    unsigned int GetCurrentDyeCount();
    PEN_TYPE GetDefaultPenSet();
    PEN_TYPE GetInstalledPens();

    void    ResetIOMode (BOOL bDevID, BOOL bStatus);

    //! Get number of copies to print
    int     GetCopyCount ()
    {
        return m_iCopyCount;
    }

    //! Set number of copies to print.
    /*! Copy count has no effect on printers that do not support this feature.
    Typically, most LaserJet printers support multiple copies, whereas, Inkjets do not.
	*****************************************************************************
	*/   
    void    SetCopyCount (int iNumCopies)
    {
        m_iCopyCount = iNumCopies;
    }
    DRIVER_ERROR    SetPrinterHint (PRINTER_HINT eHint, int iValue);

    //! Returns TRUE if borderless printing is enabled, FALSE otherwise
    BOOL    IsBorderless () { return bDoFullBleed; }

    //! SetMediaType
    /*! Typically, media type is bound to a print mode and is set when a printmode is
    selected. This is because the mediatype, printmode combination affects colormatching
    and requires a different colormap data. Some printers do colormatching in firmware
    and may support multiple mediatypes for a selected printmode.
    Returns NO_ERROR if requested mediatype is supported for the selected printmode, otherwise
    returns WARN_PRINTMODE_MISMATCH and the mediatype remains unchanged in the printmode.
	*****************************************************************************
	*/
    DRIVER_ERROR    SetMediaType (MEDIATYPE eMediaType);
    int             GetJobAttributes (int getWhat);
    void            SetJobAttributes (JobAttributes *pJA);

private:

    SystemServices* pSS;
    Printer* thePrinter;
    PrintMode* CurrentMode;
//    unsigned int CurrentModeIndex;

    unsigned int PageWidth;             // pixel width of printable area
    unsigned int InputWidth;
    unsigned int OutputWidth;           // after scaling
    PAPER_SIZE thePaperSize;
    BOOL UsePageWidth;

    //!\internal
    struct PaperSizeMetrics
    {
        // all values are in inches
        float   fPhysicalPageX;
        float   fPhysicalPageY;
        float   fPrintablePageX;
        float   fPrintablePageY;
        float   fPrintableStartY;
    };
    static const PaperSizeMetrics PSM[MAX_PAPER_SIZE];  // the size of this struct is directly related to the PAPER_SIZE enum

    // internal versions of public functions
    float printablewidth();
    float printableheight();
    unsigned int printerunitsY();

    DRIVER_ERROR QualitySieve(ModeSet*& Modes, QUALITY_MODE& eQuality);
    DRIVER_ERROR SetCompGrayMode(PrintMode*& resPM);
    BOOL ModeAgreesWithHardware(BOOL QueryPrinter);

    DRIVER_ERROR setpixelsperrow(unsigned int InputPixelsPerRow,
                                 unsigned int OutputPixelsPerRow);
    DRIVER_ERROR selectprintmode(const unsigned int index);

    BOOL MadeCompGrayMode;

    BOOL bDoFullBleed;
    BOOL InputIsPageWidth;

    // code savers
    //DRIVER_ERROR SetMode(unsigned int ModeIndex);
    DRIVER_ERROR SelectDefaultMode();
    unsigned int GUITopMargin();
/*
        // we take the hard unprintable top to be .04 (see define in Header.cpp)
        // so here start out at 1/3"-.04" = 88 if dpi=300
        { return 88 * (EffectiveResolutionY() / 300); }
*/

    MEDIATYPE   m_mtReqMediaType;   // for use by Header - Malibu defect
    MEDIATYPE   m_eMediaType;

#ifdef APDK_AUTODUPLEX
    DUPLEXMODE  DuplexMode;
#endif

#ifdef APDK_EXTENDED_MEDIASIZE
    float CustomWidth;       // custom physical page width
    float CustomHeight;      // custom physical page height
#endif

    MediaSource m_MediaSource;

    JobAttributes    *m_job_attributes;

#ifdef APDK_CAPTURE
    void Capture_PrintContext(unsigned int InputPixelsPerRow, unsigned int OutputPixelsPerRow,
                              PAPER_SIZE ps,IO_MODE IOMode);
    void Capture_SelectDevice(const PRINTER_TYPE Model);
    void Capture_SelectDevice(const char* szDevIdString);
    void Capture_SelectPrintMode(unsigned int modenum);
    void Capture_SetPaperSize(PAPER_SIZE ps, BOOL bFullBleed);
    void Capture_RealizeFont(const unsigned int ptr,const unsigned int index,const BYTE bSize,
                            const TEXTCOLOR eColor=BLACK_TEXT,
                            const BOOL bBold=FALSE,const BOOL bItalic=FALSE,
                            const BOOL bUnderline=FALSE);
    void Capture_SetPixelsPerRow(unsigned int InputPixelsPerRow,unsigned int OutputPixelsPerRow);
    void Capture_SetInputResolution(unsigned int Res);
    void Capture_dPrintContext();

#endif

    int m_iCopyCount;

    int         m_iMediaSubtype;
    int         m_iMechOffset;
}; //PrintContext

APDK_END_NAMESPACE

#endif //APDK_PRINTCONTEXT_H
