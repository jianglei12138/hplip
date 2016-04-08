/*****************************************************************************\
  context.cpp : Implimentation for the PrintContext class

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

// PrintContext

#include "header.h"
#include "pmselect.h"
#include "printerfactory.h"

#include "halftoner.h"
#include "colormatch.h"
//#include "bug.h"

APDK_BEGIN_NAMESPACE
extern ColorMatcher* Create_ColorMatcher
(
    SystemServices* pSys,
    ColorMap cm,
    unsigned int DyeCount,
    unsigned int iInputWidth
);
APDK_END_NAMESPACE

APDK_BEGIN_NAMESPACE

extern PAPER_SIZE MediaSizeToPaper(MediaSize msize);
extern MediaSize PaperToMediaSize(PAPER_SIZE psize);

// this array is directly linked to PAPER_SIZE enum
// note: fPrintablePageY is related to fPrintableStartY to allow for a 2/3" bottom margin.
//  If fPrintableStartY is altered, fPrintablePageY should be correspondingly updated.
//
// Changed PrintableStartY from 1/3" to 1/8". Also changed PrintablePageY to allow
// a 1/2" bottom margin. Note, on the 6xx series bottom margin is .46" for black and .587"
// for color. So, for 6xx series color printing to within 1/2" bottom margin is not guaranteed. des
const PrintContext::PaperSizeMetrics PrintContext::PSM[MAX_PAPER_SIZE] =
{
    //PhysicalPageX, PhysicalPageY, PrintablePageX, PrintablePageY, PrintableStartY
    // LETTER
    {
        (float)8.5, (float)11.0,    (float)8.0, (float)10.375,    (float)0.125
    },

    // A4 = 210 x 297mm = 8.27 x 11.69 in.
    {
        (float)8.27, (float)11.69,  (float)8.0, (float)11.065,    (float)0.125
    },

    // LEGAL
    {
        (float)8.5, (float)14.0,    (float)8.0, (float)13.375,    (float)0.125
    },

    // PHOTO_SIZE
    // Corresponds to 4x6" photo paper used in the 9xx series photo tray.
    // The apparent 1/8" bottom margin is allowed because of a pull-off tab on the media.
    {
        (float)4.0, (float)6.0,     (float)3.75, (float)5.75,   (float)0.125
    },

    // A6 = 105mm x 148mm = 4.13 x 5.83 in.
    // postcards -- dimensions are half A4
    {
        (float)4.13, (float)5.83,   (float)3.88,  (float)5.205,   (float)0.125
    },

    // CARD_4x6 - 4x6 index card/photo without tear-off tab
    {
        (float)4.0, (float)6.0, (float)3.75, (float)5.375, (float)0.125
    },

    // B4 = 257 x 364 mm. = 10.126 x 14.342 in.
    {
        (float)10.126, (float)14.342,   (float)9.626, (float)13.717,   (float)0.125
    },

    // B5 = 182 x 257 mm. = 7.17 x 10.126 in.
    {
        (float)7.17, (float)10.126,   (float)6.67, (float)9.5,   (float)0.125
    },

    // OUFUKU   Oufuku-Hagaki = 148 x 200 mm = 5.83 x 7.87 in.
    {
        (float)5.83, (float)7.87,  (float)5.33, (float)7.37,    (float)0.125
    },

    //  HAGAKI = 100 x 148mm = 3.94 x 5.83 in.
    {
        (float)3.94, (float)5.83,  (float)3.69, (float)5.58,    (float)0.125
    },

    // A6_WITH_TEAR_OFF_TAB = 105mm x 148mm = 4.13 x 5.83 in.
    {
        (float)4.13, (float)5.83,   (float)3.88,  (float)5.705,   (float)0.125
    },
#ifdef APDK_EXTENDED_MEDIASIZE
    // A3 = 294mm x 419.8mm = 11.69 x 16.53 in.
    {
        (float)11.69, (float)16.53,   (float)11.29,  (float)15.905,   (float)0.125
    },

    // A5 = 148mm x 210mm = 5.83 x 8.27 in.
    {
        (float)5.83, (float)8.27,   (float)5.58,  (float)7.645,   (float)0.125
    },

    // LEDGER = 11 x 17 in.
    {
        (float)11.00, (float)17.00,   (float)10.6,  (float)16.375,   (float)0.125
    },

    // SUPERB = 13 x 19 in.
    {
        (float)13.00, (float)19.00,   (float)12.6,  (float)18.375,   (float)0.125
    },

    // EXECUTIVE = 7.25 x 10.5 in.
    {
        (float)7.25, (float)10.5,   (float)6.75,  (float)9.875,   (float)0.125
    },

    // FLSA = 8.5 x 13 in.
    {
        (float)8.5, (float)13,   (float)8.00,  (float)12.375,   (float)0.125
    },

    // CUSTOM_SIZE
    {
        (float)0.0, (float)0.0,   (float)0.0,  (float)0.0,   (float)0.125
    },

    // No. 10 Envelope (4.12 x 9.5 in.)
    {
        (float)4.12, (float)9.5, (float)3.875, (float)8.875,  (float)0.125
    },

    // A2 Envelope (4.37 x 5.75 in.)
    {
        (float)4.37, (float)5.75, (float)4.12, (float)5.125,  (float)0.125
    },

    // C6 Envelope (114 x 162 mm)
    {
        (float)4.49, (float)6.38, (float)4.24, (float)5.755,  (float)0.125
    },

    // DL Envelope (110 x 220 mm)
    {
        (float)4.33, (float)8.66, (float)4.08, (float)8.035,  (float)0.125
    },

    // Japanese Envelope #3 (120 x 235 mm)
    {
        (float)4.72, (float)9.25, (float)4.47, (float)8.625,  (float)0.125
    },

    // Japanese Envelope #4 (90 x 205 mm)
    {
        (float)3.54, (float)8.07, (float)3.29, (float)7.445,  (float)0.125
    },

#endif // APDK_EXTENDED_MEDIASIZE

    // PHOTO_5x7 = 5in x 7in = 127 mm x 177.8 mm
    {
        (float) 5.0, (float) 7.0, (float) 4.75, (float) 6.375, (float) 0.125
    },
    // CDDVD_80 = 80 mm (3 inch) CD/DVD
    {
        (float) 3.3, (float) 3.3, (float) 3.3, (float) 3.3, (float) 0.0
    },
    // CDDVD_120 = 120 mm (5 inch) CD/DVD
    {
        (float) 5.0, (float) 5.0, (float) 5.0, (float) 5.0, (float) 0.0
    }

#ifdef APDK_EXTENDED_MEDIASIZE
    // PHOTO_4x8 - 4x8 panorama photo
    ,{
        (float) 4.0, (float) 6.0, (float) 3.75, (float) 7.75, (float) 0.125
    },

    // PHOTO_4x12 - 4x12 panorama photo
    {
        (float) 4.0, (float) 12.0, (float) 3.75, (float) 11.75, (float) 0.125
    },
    // L - Japanese Card, 3.5 x 5 in
    {
        (float) 3.5, (float) 5.0, (float) 3.25, (float) 4.75, (float) 0.125
    }
#endif // APDK_EXTENDED_MEDIASIZE

}; //PSM


//PrintContext::PrintContext
//! Construct a context for the print device
/*!
In the normal case where bidirectional communication was established by
SystemServices, successful construction results in instantiation of the
proper Printer class; otherwise client will have to complete the process
with a subsequent call to SelectDevice. The next two parameters are optional.
In the case where InputPixelsPerRow has the default setting of zero, the value
of printablewith will be used. If the desired print mode is known in advance,
(i.e. The user won't be selecting a print mode at any point) and the image
size is also known, then the print mode can be selected at during construction.
The reason why the last four parameters were added was because if InputPixelsPerRow
and OutputPixelsPerRow were in a resolution greater than 300 dpi, it might
cause the constructor to set constructor_error to ILLEGAL_COORDS. The constructor
checks to see if the OutputPixelsPerRow will fit on the default print mode's
page width (via a call to SelectPrintMode where the real check is done),
and if it doesn't fit, the error was given even if it was still a legal output size in
the resolution. Using the last four parameters will ensure that the image
size is checked using the proper print mode.
******************************************************************************/
PrintContext::PrintContext
(
    SystemServices * pSysServ,          //!< Your previously created SystemServices
    unsigned int InputPixelsPerRow,     //!< Input pixel witdth per row (do not exceed pagewidth * resolution)
    unsigned int OutputPixelsPerRow,    //!< Usually set to 0 (zero) for scaling up to pagewidth
    PAPER_SIZE ps,                      //!< Paper size that will be used
    QUALITY_MODE eQuality,              //!< Quality of output: DRAFT, NORMAL, BEST (default NORMAL)
    MEDIATYPE eMedia,                   //!< Media type: PLAIN, PREMIUM, PHOTO (default PLAIN)
    COLORMODE eColorMode,               //!< Color Mode: GREY_K, GREY_CMY, COLOR (default COLOR)
    BOOL bDeviceText                    //!< Support Device Text: TRUE, FALSE (dafault FALSE)
) :
    constructor_error(NO_ERROR),
    pSS(pSysServ),
    thePrinter((Printer*)NULL),
    CurrentMode((PrintMode*)NULL),
    InputWidth(InputPixelsPerRow),
    OutputWidth(OutputPixelsPerRow),
    thePaperSize(ps),
    MadeCompGrayMode(FALSE)
#ifdef APDK_AUTODUPLEX
    , DuplexMode(DUPLEXMODE_NONE)
#endif
#ifdef APDK_EXTENDED_MEDIASIZE
    , CustomWidth(0.0),
    CustomHeight(0.0)
#endif

{
#ifdef APDK_CAPTURE
    Capture_PrintContext(InputPixelsPerRow,OutputPixelsPerRow,ps,pSS->IOMode);
#endif

    DR = pSS->DR;

    m_job_attributes = NULL;
    bDoFullBleed = FALSE;

    UsePageWidth = (OutputPixelsPerRow == 0);     // flag to set width to width of page
    InputIsPageWidth = (InputPixelsPerRow == 0);  // InputWidth defaults to current page width

    m_mtReqMediaType = eMedia;  // for use by Header - Malibu defect

    m_MediaSource = sourceTrayAuto;

    m_iCopyCount = 1;

    if (!pSS->IOMode.bDevID)     // SystemServices couldn't establish good DevID
    {
        // changed - DWK & JLM  setpixelsperrow always returns NO_ERROR when CurrentMode
        // is NULL - which it always is at this point.
        //constructor_error = setpixelsperrow(InputWidth,OutputWidth);

        return;        // leave in incomplete state - constructor_error = NO_ERROR
    }

    if ( (constructor_error = DR->SelectDevice(pSS->strModel,&(pSS->VIPVersion),pSS->strPens,pSS)) != NO_ERROR)
    {
        if (constructor_error == UNSUPPORTED_PRINTER)
        {
            pSS->DisplayPrinterStatus(DISPLAY_PRINTER_NOT_SUPPORTED);
            //wait to read message
            while (pSS->BusyWait(500) != JOB_CANCELED)
            {;   // nothing.....
            }
            return;
        }
        else
        {
            DBG1("PrintContext - error in SelectDevice\n");
            return;
        }
    }

    // Device selected... now instantiate a printer object
    if ( (constructor_error = DR->InstantiatePrinter(thePrinter,pSS)) != NO_ERROR)
    {
        DBG1("PrintContext - error in InstantiatePrinter\n");
        return;
    }


    pSS->AdjustIO(thePrinter->IOMode);

    //at this point, papersize has already been set. However, if there is a manditory papersize
    //(like one enforced by a photo tray) we need to check to see if the size the user
    //wants is smaller than or equal to the papersize that is manditory. DWK
    PAPER_SIZE mandatoryPS = thePrinter->MandatoryPaperSize();
    if (mandatoryPS != UNSUPPORTED_SIZE)
    {
        if ((PSM[mandatoryPS].fPhysicalPageX < PSM[thePaperSize].fPhysicalPageX))
        {
            // they asked for a paper size larger then the mandatory size
            thePaperSize = mandatoryPS; //set the papersize to the manditory size -DWK
        }  //end if

    }//end if

    thePrinter->SetPMIndices();

    constructor_error = SelectPrintMode(eQuality, eMedia, eColorMode, bDeviceText);

    CERRCHECK;

    constructor_error = thePrinter->CheckInkLevel();
} //PrintContext



//CurrentPrintMode
//! Returns the current print mode index
/*!
******************************************************************************/
unsigned int PrintContext::CurrentPrintMode()
{
    ASSERT(CurrentMode);                // It can't be NULL
    if (CurrentMode != NULL)
    {
        return CurrentMode->myIndex;
    }
    else
    {
        return 0;                       // will be confused with Mode 0
    }
} //CurrentPrintMode

/*
DRIVER_ERROR PrintContext::SetMode(unsigned int ModeIndex)
{
    if (ModeIndex>=GetModeCount())
        return INDEX_OUT_OF_RANGE;

    CurrentModeIndex=ModeIndex;
    CurrentMode = thePrinter->GetMode(ModeIndex);
    if (CurrentMode==NULL)
        return SYSTEM_ERROR;

 return NO_ERROR;
}
*/


//SelectDefaultMode
//! Selects the default print mode for the printer
/*!

\return DRIVER_ERROR

\sa SelectPrintMode
******************************************************************************/
DRIVER_ERROR PrintContext::SelectDefaultMode()
{
//    return SelectPrintMode(QUALITY_NORMAL, MEDIA_PLAIN, COLOR, FALSE);
    DRIVER_ERROR err = SelectPrintMode (QUALITY_NORMAL, MEDIA_PLAIN, COLOR, FALSE);
    if (err == WARN_MODE_MISMATCH)
    {
        QUALITY_MODE    eq;
        MEDIATYPE       em;
        BOOL            bText;
        COLORMODE    eColorMode = COLOR;
        GetPrintModeSettings (eq, em, eColorMode, bText);
        if (eColorMode == GREY_K)
            err = NO_ERROR;
    }
    return err;
} //SelectDefaultMode


//SelectPrintMode
//!Select a print mode based on independent parameters
/*!
Select a print mode base on Quality, Media, ColorMode, and support of
DeviceText.  Method may change parameters depending on what the printer
supports.
\sa GetPrintModeSettings
******************************************************************************/
DRIVER_ERROR PrintContext::SelectPrintMode
(
    QUALITY_MODE eQuality,          //!< Quality of output: DRAFT, NORMAL, BEST
    MEDIATYPE eMedia,               //!< Media type: PLAIN, PREMIUM, PHOTO
    COLORMODE eColorMode,           //!< Color Mode: GREY_K, GREY_CMY, COLOR
    BOOL bDeviceText                //!< Support Device Text: TRUE, FALSE

)
{
    if (thePrinter == NULL)
    {
        return NO_PRINTER_SELECTED;
    }

    // variables of  printmode container class
    ModeSet* Modes;
    ModeSet* tempModeSet;
    ModeSet* oldModeSet;

    // save so we can return warning if any changed
    QUALITY_MODE requestedQuality = eQuality;
    MEDIATYPE requestedMedia = eMedia;
    COLORMODE requestedColor = eColorMode;
    BOOL requestedText = bDeviceText;

    PEN_TYPE pens;
    DRIVER_ERROR err;
    PrintMode* tmpPM = NULL;

//  This is for use by Header - full-bleed and autodetect defect in Malibu

    m_mtReqMediaType = eMedia;

// cleanup in case of repeated calls to SPM using same PC
// might have extra compgraymode hanging around
    if (MadeCompGrayMode)
    {
        pSS->FreeMem((BYTE*)CurrentMode->cmap.ulMap1);
        delete CurrentMode;
        CurrentMode = NULL;
        MadeCompGrayMode = FALSE;
    }

// initialize list of possible modes based on Printer
    Modes = new ModeSet;
    unsigned int mc = thePrinter->GetModeCount();
    for (unsigned int i = 0; i < mc; i++)
    {
        if (Modes->Append(thePrinter->GetMode(i)))
        {
            return ALLOCMEM_ERROR;
        }
    }

    pens = thePrinter->ActualPens();    // either set through bidi or explicitly

    if (pens == NO_PEN)   // unidi and no penset call
    {
        thePrinter->SetPens(thePrinter->DefaultPenSet());
    }

    tempModeSet = Modes->PenCompatibleSubset(pens);

    if (tempModeSet == NULL)
    {
        return ALLOCMEM_ERROR;
    }

    if (tempModeSet->IsEmpty())   // should never get here
    {
            delete Modes;
            delete tempModeSet;
            ASSERT(0);          // If we should never get here then assert when debugging
            return SYSTEM_ERROR;
    }
    else    // we found modes for the specified penset
    {
        delete Modes;
        Modes = tempModeSet;
    }

    // colormode adjustment
    if (pens == BLACK_PEN || pens == MDL_PEN)
    {
        eColorMode = GREY_K;
    }
    else if ((pens == COLOR_PEN) && (eColorMode == GREY_K))
    {
        eColorMode = GREY_CMY;
    }

    tempModeSet = Modes->ColorCompatibleSubset(eColorMode);

    if (tempModeSet == NULL)
    {
        return ALLOCMEM_ERROR;
    }

    if (tempModeSet->IsEmpty())
    {
        delete tempModeSet;
        // may be lacking GREY_CMY
        if (eColorMode == GREY_CMY)
        {
            if (thePrinter->CMYMap != NULL)
            {
                err=SetCompGrayMode(tmpPM); // tmpPM points to new map
                ERRCHECK;                   // possible memory leak ???
                Modes->Append(tmpPM);
            }
            else      // can't do it without a CMYMap, so change colormode
            if (pens == BLACK_PEN)
            {
                eColorMode = GREY_K;
            }
            else
            {
                eColorMode = COLOR;
            }
        }
        else
        {
            return SYSTEM_ERROR;
        }
        tempModeSet = Modes->ColorCompatibleSubset(eColorMode);

        if (tempModeSet == NULL)
        {
            return ALLOCMEM_ERROR;
        }
    }

    delete Modes;
    if(tempModeSet->IsEmpty())
    {
        delete tempModeSet;
        return SYSTEM_ERROR;
    }

    Modes = tempModeSet;
    tempModeSet = NULL;

    // set is nonempty because every printer has virtual
    // black and composite-black modes in addition to some kind of color

    /// note: this order accords with the decision that text-support trumps quality;
    ///       to make quality trump fonts, move quality-selection code here

    // rule out gui-only if devicetext requested
    if (bDeviceText)
    {
        tempModeSet = Modes->FontCapableSubset();
        if (tempModeSet == NULL)
        {
            return ALLOCMEM_ERROR;
        }

        if (tempModeSet->IsEmpty())
        {
            delete tempModeSet;
            tempModeSet = NULL;
            bDeviceText = FALSE;  // change value of reference parameter
        }
        else    // we found font-capable modes
        {
            delete Modes;
            Modes = tempModeSet;
            tempModeSet = NULL;
        }
    }

    // find best fit for quality setting
    // 1. look for exact match
    // 2. if not, look for normal
    // 3. if not, use anything

    oldModeSet = new ModeSet(Modes);    // remember in case of media mismatch
    if (oldModeSet == NULL)
    {
        return ALLOCMEM_ERROR;
    }

    // find best fit for quality
    err = QualitySieve(Modes, eQuality);    // this changes Modes list
    ERRCHECK;

    // check compatibility of quality and media
    // note: this implementation assumes media trumps quality
    //  to make quality trump media, then don't let mediatest whittle down to nothing

    tempModeSet = Modes->MediaSubset(eMedia);
    if (tempModeSet == NULL)
    {
        return ALLOCMEM_ERROR;
    }
    if (tempModeSet->IsEmpty())
    {   // try changing quality to fit media
        delete tempModeSet;
        // we are retrieving oldModeSet, saved before quality sieve
        tempModeSet = oldModeSet->MediaSubset(eMedia);
        if (tempModeSet == NULL)
        {
            return ALLOCMEM_ERROR;
        }
        if (tempModeSet->IsEmpty())
        // there was never a mode that matched media, so change media
        {
            delete tempModeSet;
            eMedia = Modes->HeadPrintMode()->GetMediaType();
        }
        else    // we found modes with the specified mediatype
        {
            eQuality = tempModeSet->HeadPrintMode()->GetQualityMode();
            delete Modes;
            Modes = tempModeSet;
            // but now we may have to apply quality test again
            if (Modes->Size() > 1)
            {
                err = QualitySieve(Modes,eQuality); // changes modes
                ERRCHECK;
            }
        }
    }
    else // we found modes with specified mediatype
    {
        delete Modes;
        Modes = tempModeSet;
    }

    delete oldModeSet;
    // because there are no printers having multiple modes with
    // all 4 parameters the same, we are down to a single result
    // (check this!)
    if (Modes->Size() != 1)
    {
        ASSERT(0);                  // this is is not supposed to happen
        return SYSTEM_ERROR;
    }

    CurrentMode = Modes->HeadPrintMode();
    if (CurrentMode == tmpPM)
    {
        MadeCompGrayMode=TRUE;  // remember to delete it in PC destructor
    }

    delete Modes;

    //need to make sure that print context checks to see if size of image is ok for paper we want to use DWK

    unsigned int    iRasterWidth = InputIsPageWidth ? 0 : InputWidth;
    err = setpixelsperrow(iRasterWidth, OutputWidth);
    if(err != NO_ERROR)
    {
        return err;
    }//end if

    // return warning if any of the first 3 params changed,
    // or if text was requested but not available

    if ((requestedColor != eColorMode) ||
        (((requestedMedia != eMedia) ||
        (requestedQuality != eQuality)) && CurrentMode->medium != mediaAuto) ||
        (requestedText && !bDeviceText))
    {
        return WARN_MODE_MISMATCH;
    }

    return NO_ERROR;
} //SelectPrintMode


//GetPrintModeSettings
//! Returns pointer to print mode settings
/*!
Used to determine the currently selected print mode.  Print mode could be
different then requested values in SelectePrintMode because the printer may
not be able to support the requested combination.  In that case the
PrintContext will make an intelegent choice about changing the print mode.
This method can be used to determin the differences between the requested mode
and the selected mode.
******************************************************************************/
DRIVER_ERROR PrintContext::GetPrintModeSettings
(
    QUALITY_MODE& eQuality,          //!< Quality of output: DRAFT, NORMAL, BEST
    MEDIATYPE& eMedia,               //!< Media type: PLAIN, PREMIUM, PHOTO
    COLORMODE& eColorMode,           //!< Color Mode: GREY_K, GREY_CMY, COLOR
    BOOL& bDeviceText                //!< Support Device Text: TRUE, FALSE
)
{
    if (CurrentMode == NULL)
    {
        return SYSTEM_ERROR;
    }
    else
    {
        CurrentMode->GetValues(eQuality, eMedia, eColorMode, bDeviceText);
    }
    return NO_ERROR;
} //GetPrintModeSettings


/*!
Sets the pen set to be used (much match what is actually in the printer).
For use in uni-di mode.
*/
DRIVER_ERROR PrintContext::SetPenSet
(
    PEN_TYPE ePen
)
{
    if (thePrinter == NULL)
    {
        return NO_PRINTER_SELECTED;
    }
    return thePrinter->SetPens(ePen);
} //SetPenSet


BOOL PrintContext::ModeAgreesWithHardware
(
    BOOL QueryPrinter
)
// no check for null printer
{
    BOOL agree = FALSE;
    PEN_TYPE ePen;
/*
    This should not be nessesarry any more.  Each printer class now has a
    DefaultPenSet method that sets the pens to what the printer is shipped
    with for the unidi case. If that method has been called (which it should
    be in the unidi case then ModeAgreesWithHardware will work properly.

    Dave S. discovered this does not work yet so we must leave this here until
    another version and we are sure it works for unidi mode. - JLM
*/
    if (pSS->IOMode.bDevID == FALSE)
    {
        return TRUE;
    }

    if (thePrinter->ParsePenInfo(ePen,QueryPrinter) == NO_ERROR)
    {

        for (int i = 0; i < MAX_COMPATIBLE_PENS; i++)
        {
            if (ePen == CurrentMode->CompatiblePens[i])
            {
                agree = TRUE;
            }
        }
    }

    return agree;
} //ModeAgreesWithHardware


BOOL PrintContext::PhotoTrayPresent
(
    BOOL bQueryPrinter
)
{
    if (thePrinter == NULL)
    {
        return FALSE;
    }
    else
    {
        return thePrinter->PhotoTrayPresent (bQueryPrinter);
    }
} //PhotoTrayPresent

//! Return current status of PhotoTray - can be UNKNOWN, ENGAGED or DISENGAGED
/*!
******************************************************************************/

PHOTOTRAY_STATE PrintContext::PhotoTrayEngaged
(
    BOOL bQueryPrinter
)
{
    if (thePrinter == NULL)
    {
        return DISENGAGED;
    }
    else
    {
        return thePrinter->PhotoTrayEngaged (bQueryPrinter);
    }
} //PhotoTrayEngaged


//! Returns TRUE if a hagaki feed is present in printer.
BOOL PrintContext::HagakiFeedPresent(BOOL bQueryPrinter)
{
    if (thePrinter == NULL)
    {
        return FALSE;
    }
    else
    {
        return thePrinter->HagakiFeedPresent(bQueryPrinter);
    }
}

#ifdef APDK_AUTODUPLEX
//!Returns TRUE if duplexer and hagaki feed (combined) unit is present in printer.
BOOL PrintContext::HagakiFeedDuplexerPresent(BOOL bQueryPrinter)
{
    if (thePrinter == NULL)
    {
        return FALSE;
    }
    else
    {
        return thePrinter->HagakiFeedDuplexerPresent(bQueryPrinter);
    }
}
#endif

//~PrintContext
//! Destroy the print device context
/*!
******************************************************************************/
PrintContext::~PrintContext()
{
#ifdef APDK_CAPTURE
Capture_dPrintContext();
#endif
DBG1("deleting PrintContext\n");

    if (thePrinter)
    {
        delete thePrinter;
    }

    if (MadeCompGrayMode)
    {
        pSS->FreeMem((BYTE*)CurrentMode->cmap.ulMap1);
        delete CurrentMode;
    }
    if (m_job_attributes)
    {
        delete [] m_job_attributes;
    }
} //~PrintContext


///////////////////////////////////////////////////////////////////////
// Functions to report on device-dependent properties.
// Note that mixed-case versions of function names are used
// for the client API; lower-case versions are for calls
// made by the driver itself, to avoid the APDK_CAPTURE instrumentation.
///////////////////////////////////////////////////////////////////////

//! Retrieves information about the physical width of the currently selected paper size.
float PrintContext::PhysicalPageSizeX()   // returned in inches
{
    if (m_job_attributes)
    {
        return m_job_attributes->media_attributes.fPhysicalWidth;
    }

    if (thePrinter == NULL)
    {
        return 0.0;
    }

    float   xOverSpray, yOverSpray;
    FullbleedType    fbType;
#ifdef APDK_EXTENDED_MEDIASIZE
    float PhysicalPageX = (thePaperSize == CUSTOM_SIZE) ? CustomWidth : PSM[thePaperSize].fPhysicalPageX;
#else
    float PhysicalPageX = PSM[thePaperSize].fPhysicalPageX;
#endif

    if (bDoFullBleed && (thePrinter->FullBleedCapable (thePaperSize, &fbType, &xOverSpray, &yOverSpray)))
    {
        return (PhysicalPageX + xOverSpray);
    }

    return PhysicalPageX;
} //PhysicalPageSizeX


//! Retrieves information about the physical height of the currently selected paper size.
float PrintContext::PhysicalPageSizeY()   // returned in inches
{
    if (m_job_attributes)
    {
        return m_job_attributes->media_attributes.fPhysicalHeight;
    }

    if (thePrinter == NULL)
    {
        return 0.0;
    }

    float   xOverSpray, yOverSpray;
    FullbleedType   fbType;

#ifdef APDK_EXTENDED_MEDIASIZE
    float PhysicalPageY = (thePaperSize == CUSTOM_SIZE) ? CustomHeight : PSM[thePaperSize].fPhysicalPageY;
#else
    float PhysicalPageY = PSM[thePaperSize].fPhysicalPageY;
#endif

    if (bDoFullBleed && (thePrinter->FullBleedCapable (thePaperSize, &fbType, &xOverSpray, &yOverSpray)))
    {
        return (PhysicalPageY + yOverSpray);
    }

    return PhysicalPageY;
} //PhysicalPageSizeY


//! Returns width of printable region in inches.
float PrintContext::PrintableWidth()    // returned in inches
// for external use
{
    return printablewidth();
} //PrintableWidth

/////////////////////////////////////////////////////////////////////
// NOTE ON RESOLUTIONS: These functions access ResolutionX[C],
//  where C is the conventional index for Cyan. The assumption
//  is that Res[C]=Res[M]=Res[Y], AND that Res[K]>=Res[C]
/////////////////////////////////////////////////////////////////////

float PrintContext::printablewidth()
// for internal use
{
    if (m_job_attributes)
    {
        return m_job_attributes->media_attributes.fPrintableWidth;
    }

    if (thePrinter == NULL)
    {
        return 0.0;
    }

    float   xOverSpray, yOverSpray;
    FullbleedType  fbType;

#ifdef APDK_EXTENDED_MEDIASIZE
    float PhysicalPageX = (thePaperSize == CUSTOM_SIZE) ? CustomWidth : PSM[thePaperSize].fPhysicalPageX;
    float PrintablePageX = (thePaperSize == CUSTOM_SIZE) ? CustomWidth - (float) (0.25+0.25) : PSM[thePaperSize].fPrintablePageX;
#else
    float PhysicalPageX = PSM[thePaperSize].fPhysicalPageX;
    float PrintablePageX = PSM[thePaperSize].fPrintablePageX;
#endif

    if (bDoFullBleed && (thePrinter->FullBleedCapable (thePaperSize, &fbType, &xOverSpray, &yOverSpray)))
    {
        return (PhysicalPageX + xOverSpray);
    }

    float   fMargins[4];    // Left, Right, Top, Bottom margin values

    if (!(thePrinter->GetMargins (thePaperSize, fMargins)))
    {
        return PrintablePageX;
    }
    return (PhysicalPageX - (fMargins[0] + fMargins[1]));
} //printablewidth


//! Returns height of printable region in inches.
float PrintContext::PrintableHeight()     // returned in inches
// for external use
{
    return printableheight();
} //PrintableHeight


float PrintContext::printableheight()
// for internal use
{
    if (m_job_attributes)
    {
        return m_job_attributes->media_attributes.fPrintableHeight;
    }

    if (thePrinter == NULL)
    {
        return 0.0;
    }

/*
 *  Full bleed printing will be possible only on 3 sides for those media
 *  that do not have a tear-off tab. Currently, 4x6 and A6 are the only two
 *  media sizes that can have tear-off tab. So, for all the media that do not
 *  have a tear-off tab at the bottom, physical height is unaltered, but printable
 *  height is less by bottom margin.
 *  1/22/03 SY. 
 *  Starting from MalibuPlus, 4-edge full bleed is possible for most media.  A check
 *  for 4-edge full bleed capability is needed.
 */

    float   xOverSpray, yOverSpray;
    FullbleedType   fbType;
#ifdef APDK_EXTENDED_MEDIASIZE
    float PhysicalPageY = (thePaperSize == CUSTOM_SIZE) ? CustomHeight : PSM[thePaperSize].fPhysicalPageY;
    float PrintablePageY = (thePaperSize == CUSTOM_SIZE) ? CustomHeight - (float) (0.125+0.5) : PSM[thePaperSize].fPrintablePageY;
#else
    float PhysicalPageY = PSM[thePaperSize].fPhysicalPageY;
    float PrintablePageY = PSM[thePaperSize].fPrintablePageY;
#endif

    if (bDoFullBleed  && (thePrinter->FullBleedCapable (thePaperSize, &fbType, &xOverSpray, &yOverSpray)))
    {
        if (fbType == fullbleed3EdgeAllMedia || 
            fbType == fullbleed3EdgeNonPhotoMedia || 
            fbType == fullbleed3EdgePhotoMedia)
            return (PhysicalPageY - (float) 0.5);
        else if (fbType == fullbleed4EdgePhotoMedia || 
                 fbType == fullbleed4EdgeNonPhotoMedia || 
                 fbType == fullbleed4EdgeAllMedia)
            return (PhysicalPageY + yOverSpray);
        else
            return (PhysicalPageY - (float) 0.5);
    }

/*
 *  REVISIT:
 *  The paper size metrics table assumes a bottom margin of 0.5 inch.
 *  But bottom margin on 6xx based printers is a little larger, ~0.58 inch.
 *  We should do per-printer get bottom margin/left margin to adjust printable height
 *  and printable width.
 *
 *  (Note, a 0.67 inch bottom margin is now in GetMargins() for 6xx based printers. des)
 */

    float   fMargins[4];    // Left, Right, Top, Bottom margin values

    if (!(thePrinter->GetMargins (thePaperSize, fMargins)))
    {
        return PrintablePageY;
    }
    return (PhysicalPageY - (fMargins[2] + fMargins[3]));
} //printableheight


//! Returns the left margin or distance from the top edge of the page.
float PrintContext::PrintableStartX() // returned in inches
{
    if (m_job_attributes)
    {
        return m_job_attributes->media_attributes.fPrintableStartX;
    }

    if (thePrinter==NULL)
    {
        return 0;
    }

    float   xOverSpray, yOverSpray;
    FullbleedType  fbType;
    if (bDoFullBleed && (thePrinter->FullBleedCapable (thePaperSize, &fbType, &xOverSpray, &yOverSpray)))
    {
        return (0.0);
    }

    float   fMargins[4];    // Left, Right, Top, Bottom margin values

    if ((thePrinter->GetMargins (thePaperSize, fMargins)))
    {
        return (fMargins[0]);
    }

    // this return value centers the printable page horizontally on the physical page
#ifdef APDK_EXTENDED_MEDIASIZE
    float physwidth = (thePaperSize == CUSTOM_SIZE) ? CustomWidth : PSM[thePaperSize].fPhysicalPageX;
#else
    float physwidth = PSM[thePaperSize].fPhysicalPageX;
#endif
    float printable =  printablewidth (); // PSM[ thePaperSize ].fPrintablePageX;
    return ((physwidth - printable) / (float)2.0 );
} //PrintableStartX


//! Returns the top margin or distance from the top edge of the page.
float PrintContext::PrintableStartY() // returned in inches
{
    if (m_job_attributes)
    {
        return m_job_attributes->media_attributes.fPrintableStartY;
    }

    if (thePrinter == NULL)
    {
        return 0;
    }

    float   xOverSpray, yOverSpray;
    FullbleedType fbType;

    if (bDoFullBleed && (thePrinter->FullBleedCapable (thePaperSize, &fbType, &xOverSpray, &yOverSpray)))
    {
        return (0.0);
    }

    float   fMargins[4];    // Left, Right, Top, Bottom margin values

    if ((thePrinter->GetMargins (thePaperSize, fMargins)))
    {
        return (fMargins[2]);
    }

    return PSM[ thePaperSize ].fPrintableStartY;
} //PrintableStartY


unsigned int PrintContext::printerunitsY()
// internal version
{
    if (thePrinter == NULL)
    {
        return 0;
    }
    return CurrentMode->ResolutionY[C];
} //printerunitsY


/*!
Used outside the context of a Job, to perform functions such as pen cleaning.
*/
DRIVER_ERROR PrintContext::PerformPrinterFunction
(
    PRINTER_FUNC eFunc
)
{

    if (thePrinter == NULL)
    {
        return NO_PRINTER_SELECTED;
    }

    if (eFunc == CLEAN_PEN)
    {
        thePrinter->Flush();
        return thePrinter->CleanPen();
    }

    DBG1("PerformPrinterFunction: Unknown function\n");
    return UNSUPPORTED_FUNCTION;

} //PerformPrinterFunction

/*!
Returns the enum for the next supported model. This method is used when
bi-directional communication is missing, or for dynamic querying of the
properties of a given build. This method is presumably called within a loop,
which will exit when the return value equals UNSUPPORTED. Passing this return
value to SelectDevice will instantiate this printer object and therefore allow
further querying of the printer’s capabilities through other methods in
PrintContext.
\param familyHandle Caller starts at null; reference variable is incremented
automatically for next call.
\return A value matching an element of the enum PRINTER_TYPE, which can then
be passed to other methods such as SelectDevice.
*/
PRINTER_TYPE PrintContext::EnumDevices
(
    FAMILY_HANDLE& familyHandle
) const
{
    return pPFI->EnumDevices(familyHandle);
} //EnumDevices

BOOL PrintContext::PrinterFontsAvailable()
{
#if defined(APDK_FONTS_NEEDED)
    if (thePrinter == NULL)
    {
        return SYSTEM_ERROR;    // called too soon
    }
    // if a printer exists, so does a printmode; but check anyway
    if (CurrentMode == NULL)
    {
        return SYSTEM_ERROR;
    }
    return CurrentMode->bFontCapable;
#else
    return FALSE;
#endif
} //PrinterFontsAvailable


/*! Used by client when SystemServices couldn't get DevID.
This is the place where printer gets instantiated for unidi
******************************************************************************/
DRIVER_ERROR PrintContext::SelectDevice
(
    const PRINTER_TYPE Model
)
{
#ifdef APDK_CAPTURE
Capture_SelectDevice(Model);
#endif
    DRIVER_ERROR err;

    if (thePrinter)     // if printer exists due to bidi or previous call
    {
        delete thePrinter;
        thePrinter = NULL;
    }

    err = DR->SelectDevice(Model);
    ERRCHECK;

    if ( (err = DR->InstantiatePrinter(thePrinter,pSS)) != NO_ERROR)
    {
        DBG1("PrintContext - error in InstantiatePrinter\n");
        return err;
    }

    const char* model;
    if (strlen(pSS->strModel) > 0)  // if bidi so strModel got set in SS
    {
        model = pSS->strModel;
    }
    else
    {
        model = PrintertypeToString(Model);    // at least give something
    }

    pSS->AdjustIO(thePrinter->IOMode, model);

    PAPER_SIZE ps = thePrinter->MandatoryPaperSize();
    if (ps != UNSUPPORTED_SIZE)
    {
        if ((PSM[ps].fPhysicalPageX < PSM[thePaperSize].fPhysicalPageX))
        {
            thePaperSize = ps;
        }
    }//end if (p != UNSUPPORTED_SIZE)

    if ((err = SelectDefaultMode()) == NO_ERROR)
    {
        unsigned int    iRasterWidth = InputIsPageWidth ? 0 : InputWidth;
        err = setpixelsperrow(iRasterWidth, OutputWidth);

    }//end if((err = SelectDefaultMode()) == NO_ERROR)

    thePrinter->SetPMIndices();

 return err;
} //SelectDevice


//SelectDevice
//!Select device type using device id string returned from printer.
/*! \internal
Used by client when SystemServices couldn't get DevID.
This is the place where printer gets instantiated for unidi.
This allows a remote system to select a device based on the device ID
string it has when the APDK cannot communicate with the device. I.E.
remote rendering.
\todo Not implemented yet
******************************************************************************/
DRIVER_ERROR PrintContext::SelectDevice
(
    const char* szDeviceId
)
{
    ASSERT(szDeviceId);
#ifdef APDK_CAPTURE
    Capture_SelectDevice(szDeviceId);
#endif

    DRIVER_ERROR err;

    if (thePrinter)     // if printer exists due to bidi or previous call
    {
        delete thePrinter;
        thePrinter = NULL;
    }

    FAMILY_HANDLE familyHandle = pPFI->FindDevIdMatch(szDeviceId);
    if (familyHandle == NULL)
    {
        return UNSUPPORTED_PRINTER;
    }
	if(0 == strnlen((const char *)pSS->strDevID, DevIDBuffSize))
	{
		strncpy((char *)pSS->strDevID,szDeviceId,DevIDBuffSize);
	}
    thePrinter = pPFI->CreatePrinter (pSS, familyHandle);
    if (thePrinter->constructor_error != NO_ERROR)
    {
        return thePrinter->constructor_error;
    }

    const char* model = pPFI->GetFamilyName (familyHandle);

    pSS->AdjustIO (thePrinter->IOMode, model);

    PAPER_SIZE ps = thePrinter->MandatoryPaperSize ();
    if (ps != UNSUPPORTED_SIZE)
    {
        if ((PSM[ps].fPhysicalPageX < PSM[thePaperSize].fPhysicalPageX))
        {
            thePaperSize = ps;
        }
    }//end if (p != UNSUPPORTED_SIZE)

    if ((err = SelectDefaultMode()) == NO_ERROR)
    {
        unsigned int    iRasterWidth = InputIsPageWidth ? 0 : InputWidth;
        err = setpixelsperrow(iRasterWidth, OutputWidth);

    }//end if((err = SelectDefaultMode()) == NO_ERROR)

    thePrinter->SetPMIndices();

    return err;
} //SelectDevice


///////////////////////////////////////////////////////////////////////////
DRIVER_ERROR PrintContext::setpixelsperrow
(
    unsigned int InputPixelsPerRow,
    unsigned int OutputPixelsPerRow
)
// internal version without printer check
{
    unsigned int baseres;
    float printwidth;

    if (CurrentMode == NULL)
    {
        return NO_ERROR;
    }

    baseres = CurrentMode->BaseResX;
    int ResBoost = CurrentMode->BaseResX / CurrentMode->BaseResY;

    if (ResBoost == 0)
    {
        ResBoost = 1;
    }
    baseres /= ResBoost;    // account for expansion for asymmetrical modes (x!=y)

    printwidth = printablewidth();

    PageWidth = (unsigned int)(int)((float)baseres * printwidth + 0.5);

    if (InputPixelsPerRow == 0)
    {
        if (UsePageWidth)
        {
            InputPixelsPerRow = PageWidth;             // by convention
        }
        else
        {
            return BAD_INPUT_WIDTH;
        }
    }

    if (UsePageWidth)
    {
        OutputPixelsPerRow = PageWidth;           // by convention
    }

    if (OutputPixelsPerRow > PageWidth)
    {
        return OUTPUTWIDTH_EXCEEDS_PAGEWIDTH;
    }
    if (InputPixelsPerRow > OutputPixelsPerRow)
    {
        return UNSUPPORTED_SCALING;      // no downscaling
    }

    // new value is legal
    InputWidth = InputPixelsPerRow;
    OutputWidth = OutputPixelsPerRow;

/*
 *    Adjust OutputWidth to avoid fractional scaling.
 */

    int    iScaleFactor = (int) (((float) OutputWidth / (float) InputWidth) + 0.02);
    int    iDiff = OutputWidth - InputWidth * iScaleFactor;
    if (iDiff > 0 && iDiff < ((12 * (int) baseres) / 300))
    {
        OutputWidth = InputWidth * iScaleFactor;
        if (OutputWidth > PageWidth)
        {
            OutputWidth = PageWidth;
        }
    }

    return NO_ERROR;
} //setpixelsperrow


/*!
Sets the width of all rows on the page.
*/
DRIVER_ERROR PrintContext::SetPixelsPerRow
(
    unsigned int InputPixelsPerRow,
    unsigned int OutputPixelsPerRow
)
// set new in/out width
{
#ifdef APDK_CAPTURE
    Capture_SetPixelsPerRow(InputPixelsPerRow, OutputPixelsPerRow);
#endif
    if (thePrinter == NULL)
    {
        return NO_PRINTER_SELECTED;
    }

   return setpixelsperrow(InputPixelsPerRow, OutputPixelsPerRow);
} //SetPixelsPerRow


//! Returns the horizontal printer resolution to be used for currently selected print mode.
unsigned int PrintContext::EffectiveResolutionX()
{
    if (CurrentMode == NULL)
    {
        return 0;
    }

    return CurrentMode->BaseResX;
} //EffectiveResolutionX


//! Returns the vertical printer resolution to be used for currently selected print mode.
unsigned int PrintContext::EffectiveResolutionY()
{
    if (CurrentMode == NULL)
    {
        return 0;
    }

    return CurrentMode->BaseResY;
} //EffectiveResolutionY


//!\brief Returns number of supported print modes for the select printer model.
/*!
\deprecated
This method will return the number of print modes. The device must support
(and should return a value of) at least two print modes, gray and normal.
The general convention for interpretation of the indices is:
\li 0 = GrayMode - rendering for black pen
\li 1 = the \i basic mode for this printer, usually targeting plain paper and normal quality
\li 2,3... = special modes that may be available for this printer
\note Do not count on mode counts or mode indexes.
\see SelectPrintMode()
*/
unsigned int PrintContext::GetModeCount()
{
  if (thePrinter == NULL)
  {
      return 0;
  }

  return thePrinter->GetModeCount();
} //GetModeCount


DRIVER_ERROR PrintContext::selectprintmode
(
    const unsigned int index
)
{
    DRIVER_ERROR error = NO_ERROR;

    if (thePrinter == NULL)
    {
        return NO_PRINTER_SELECTED;
    }

    unsigned int count = GetModeCount();
    if (index > (count-1))
    {
        return INDEX_OUT_OF_RANGE;
    }

    CurrentMode= thePrinter->GetMode(index);
//    CurrentModeIndex = index;

    error = setpixelsperrow(InputWidth,OutputWidth);

    if (error != NO_ERROR)  // could be caused by changing to lower-res mode
    {

        error = setpixelsperrow(0,0);   // try again with default
    }

    if (error > NO_ERROR)
    {
        return error;
    }

    if (!ModeAgreesWithHardware(FALSE))
    {
        return WARN_MODE_MISMATCH;
    }
    // notice that requested mode is set even if it is wrong for the pen

    return error;
} //selectprintmode


/*!
\deprecated
Chooses amongst available print modes. Index of zero is grayscale, index of 1
is default mode. Use the new SelectePrintMode interface.
*/
DRIVER_ERROR PrintContext::SelectPrintMode
(
    const unsigned int index
)
{
#ifdef APDK_CAPTURE
    Capture_SelectPrintMode(index);
#endif
    return selectprintmode(index);
} //SelectPrintMode


//! Sets or changes the target paper size for a print job.
/*!
This method sets the target paper size to be used by the print job. This
would have already been set during PrintContext construction, but may be reset
using this function as long as the job object itself has not yet been created.
\see PAPER_SIZE for supported paper sizes.
*/
DRIVER_ERROR PrintContext::SetPaperSize (PAPER_SIZE ps, BOOL bFullBleed)
{
#ifdef APDK_CAPTURE
    Capture_SetPaperSize(ps, bFullBleed);
#endif

    DRIVER_ERROR    err;
    if (thePrinter == NULL)
    {
        return NO_PRINTER_SELECTED;
    }

    // Version 3.0.1 logic for allowing mandatory paper size or smaller
    PAPER_SIZE  psMandatoryPS;
    DRIVER_ERROR    psError = NO_ERROR;
    if ((psMandatoryPS = thePrinter->MandatoryPaperSize()) != UNSUPPORTED_SIZE)
    {
#ifdef APDK_EXTENDED_MEDIASIZE
        float ReqPhysWidth = (ps == CUSTOM_SIZE) ? CustomWidth : PSM[ps].fPhysicalPageX;
#else
        float ReqPhysWidth = PSM[ps].fPhysicalPageX;
#endif
        if ((ReqPhysWidth > PSM[psMandatoryPS].fPhysicalPageX))
              // only use the page width.  We are not so concerned about the height.
              // if we use both then we have a problem with paper sizes that shorter,
              // but wider or taller and narrower - example A6 and PHOTO.  If you set
              // one then the other is invalid of both dimentions are checked. JLM
//                || (PSM[ps].fPhysicalPageY > PSM[thePaperSize].fPhysicalPageY))
        {
                // they asked for a paper size larger then the mandatory size
//                return ILLEGAL_PAPERSIZE;
/*
 *          This is not really an error, but a warning. App may call here again
 *          with a different paper size if they don't like what we forced it to.
 */

            ps = psMandatoryPS;
            psError = WARN_ILLEGAL_PAPERSIZE;
        }
    }
    // in all other cases allow the change - even if it is the same
    thePaperSize = ps;

    bDoFullBleed = bFullBleed;

    unsigned int    iRasterWidth = InputIsPageWidth ? 0 : InputWidth;
    err = setpixelsperrow (iRasterWidth, OutputWidth);

/*
 *  If we forced the requested papersize to the mandatory papersize, return a warning
 */

    if (err == NO_ERROR && psError != NO_ERROR)
    {
        return WARN_ILLEGAL_PAPERSIZE;
    }

/*
 *  App is requesting full-bleed printing
 */

    if (err == NO_ERROR && bFullBleed)
    {
        float   x;
        FullbleedType   fbType;

//      Does this printer support full-bleed printing

        if (!(thePrinter->FullBleedCapable (ps, &fbType, &x, &x)))
            return WARN_FULL_BLEED_UNSUPPORTED;

//      Media with tear-off tab can do full-bleed on all 4 sides
        if (fbType == fullbleedNotSupported)
        {
            return WARN_FULL_BLEED_UNSUPPORTED;
        }
        else if (fbType == fullbleed3EdgeAllMedia)
        {
            return WARN_FULL_BLEED_3SIDES;
        }
        else if (fbType == fullbleed3EdgeNonPhotoMedia)  // Treat non photo case as all media
        {
            return WARN_FULL_BLEED_3SIDES;
        }
        else if (fbType == fullbleed3EdgePhotoMedia)
        {
            return WARN_FULL_BLEED_3SIDES_PHOTOPAPER_ONLY;
        }
        else if (fbType == fullbleed4EdgePhotoMedia)
        {
            return WARN_FULL_BLEED_PHOTOPAPER_ONLY;
        }
        else if (fbType == fullbleed4EdgeAllMedia)
        {
            return NO_ERROR;
        }
        else if (fbType == fullbleed4EdgeNonPhotoMedia) // Treat non photo case as all media
        {
            return NO_ERROR;
        }
    }

    return err;
} //SetPaperSize


PAPER_SIZE PrintContext::GetPaperSize()
{
    return thePaperSize;
} //GetPaperSize


/*!
This method returns the currently selected device. A device is selected
implicitly in the constructor if bi-directional communication is working.
SelectDevice() is used with unidirectional communication, or to override the
implicit selection.
\return PRINTER_TYPE Returns UNSUPPORTED if no Printer has been selected,
either implicitly or explicitly.
*/
PRINTER_TYPE PrintContext::SelectedDevice()
{
    if (thePrinter == NULL)
    {
        return UNSUPPORTED;
    }
    return (PRINTER_TYPE)DR->device;
} //SelectedDevice


/*!
Returns the model portion of the firmware ID string from the printer.
*/
const char* PrintContext::PrinterModel()
{
    if ((pSS == NULL) || (thePrinter == NULL))
    {
        return (const char*)NULL;
    }

    return pSS->strModel;
} //PrinterModel


/*!
Returns a string representing the printer model family signified by the parameter.
*/
const char* PrintContext::PrintertypeToString
(
    PRINTER_TYPE pt
)
{
    return ModelName[pt];
} //PrintertypeToString


/*!
Used outside the context of a job.
For use when pre-formatted data is available from an external source.
*/
DRIVER_ERROR PrintContext::SendPrinterReadyData
(
    BYTE* stream,
    unsigned int size
)
{
    if (stream == NULL)
    {
        return NULL_POINTER;
    }

    if (thePrinter == NULL)
    {
        return NO_PRINTER_SELECTED;
    }

    return thePrinter->Send(stream, size);
} //SendPrinterReadyData


/*!
Used outside the context of a job.
Flushes data in the printer's input buffer to prepare for new data stream.
*/
void PrintContext::Flush
(
    int FlushSize
)
{
    if(thePrinter != NULL)
    {
        thePrinter->Flush(FlushSize);
    }
} //Flush


/*!
\internal
Returns number of pages the device has printed.  Not for this job, but from
firmware counts in the printer.  Only supported on printers that keep track of
pages printed.  Default is to return UNSUPPORTED_FUNCTION.
*/
DRIVER_ERROR PrintContext::PagesPrinted
(
    unsigned int& count
)
{
    if (thePrinter == NULL)
    {
        return NO_PRINTER_SELECTED;           // matches result when printer doesn't keep counter
    }

    return thePrinter->PagesPrinted(count);
} //PagesPrinted

/*!
This is the method for use to check if the printer support a separate 1 bit black channel
*/
BOOL PrintContext::SupportSeparateBlack()
{
    if (thePrinter == NULL || CurrentMode == NULL)
    {
        return FALSE;
    }
    else
    {
        if (CurrentMode->dyeCount == 3 || CurrentMode->dyeCount == 6)
        {
            return FALSE;
        }
        else
        {
            return thePrinter->SupportSeparateBlack (CurrentMode);
        }
    }
}

#ifdef APDK_AUTODUPLEX

/*!
Request a duplexing mode if supported in current printer in current mode.
*/
BOOL PrintContext::SelectDuplexPrinting
(
    DUPLEXMODE duplexmode
)
{
    if (thePrinter == NULL || CurrentMode == NULL)
    {
        return FALSE;
    }
    if (!CurrentMode->bDuplexCapable)
    {
        return FALSE;
    }
    CurrentMode->SetDuplexMode (duplexmode);
    return TRUE;
} //SelectDuplexPrinting


/*!
Determine what duplexing mode is currently selected.
*/
DUPLEXMODE PrintContext::QueryDuplexMode ()
{
    if (thePrinter == NULL || CurrentMode == NULL)
    {
        return DUPLEXMODE_NONE;
    }
    return CurrentMode->QueryDuplexMode ();
} //QueryDuplexMode

/*!
Determine if the selected printer requires rasters to be rotated by 180 degrees
for printing on the back side in a duplex job.
*/
BOOL    PrintContext::RotateImageForBackPage ()
{
    if (thePrinter == NULL)
        return TRUE;
    return thePrinter->RotateImageForBackPage ();
}

#endif

#ifdef APDK_EXTENDED_MEDIASIZE

/*!
Set custom paper size physical width and height in inches. Used with custom paper size only.
*/
BOOL  PrintContext::SetCustomSize (float width, float height)
{
    CustomWidth=width;
    CustomHeight=height;
    return NO_ERROR;
}

#endif //APDK_EXTENDED_MEDIASIZE


unsigned int PrintContext::GUITopMargin()
{
    // we take the hard unprintable top to be .04 (see define in Header.cpp)
    // so here start out at 1/3"-.04" = 88 if dpi=300
  //
  // Changed 1/3" top margin to 1/8", 1/8"-.04" = 25.5 if dpi=300. des

    // symmetrical top and bottom margin of 0.5 inch in duplex mode
#ifdef APDK_AUTODUPLEX
    if (CurrentMode->QueryDuplexMode () != DUPLEXMODE_NONE)
    {
        return 138 * (EffectiveResolutionY() / 300); /* .5" */
    }
#endif
    return 26 * (EffectiveResolutionY() / 300);
} //GUITopMargin

/*
PEN_TYPE PrintContext::GetCompatiblePen
(
    unsigned int num
)
{
    if ((thePrinter == NULL) || (num >= MAX_COMPATIBLE_PENS))
    {
        return DUMMY_PEN;
    }

    return thePrinter->CompatiblePens[num];
} //GetCompatiblePen
*/

DRIVER_ERROR PrintContext::QualitySieve
(
    ModeSet*& Modes,
    QUALITY_MODE& eQuality
)
{
    ModeSet* tempModeSet = Modes->QualitySubset(eQuality);
    if (tempModeSet == NULL)
    {
        return ALLOCMEM_ERROR;
    }

    if (tempModeSet->IsEmpty())   // failed #1
    {
        delete tempModeSet;
        eQuality = QUALITY_NORMAL;    // change requested quality
        tempModeSet = Modes->QualitySubset(eQuality);

        if (tempModeSet == NULL)
        {
            return ALLOCMEM_ERROR;
        }

        if (tempModeSet->IsEmpty())   // failed #2 -- just get something
        {
            delete tempModeSet;
            tempModeSet = Modes->Head();

            if (tempModeSet == NULL)
            {
                return ALLOCMEM_ERROR;
            }

            if (tempModeSet->IsEmpty())
            {
                delete tempModeSet;
                return SYSTEM_ERROR;
            }

            eQuality = tempModeSet->HeadPrintMode()->GetQualityMode();
        }
    }

    delete Modes;
    Modes=tempModeSet;
    return NO_ERROR;
} //QualitySieve


DRIVER_ERROR PrintContext::SetCompGrayMode
(
    PrintMode*& resPM
)
{
    DRIVER_ERROR err;
    ColorMatcher* pColorMatcher;
    uint32_t* graymap = (uint32_t*)pSS->AllocMem(sizeof(uint32_t)*9*9*9);

    if (graymap==NULL)
    {
        return ALLOCMEM_ERROR;
    }

    ColorMap cm;
    cm.ulMap1 = thePrinter->CMYMap;

    if (cm.ulMap1==NULL)
    {
        return SYSTEM_ERROR;
    }
    cm.ulMap2 = NULL;
    cm.ulMap3 = NULL;

    PEN_TYPE pen = thePrinter->ActualPens();
    unsigned int numinks;

    if (pen == COLOR_PEN)
    {
        numinks = 3;
    }
    else
    {
        numinks = 4;     // even for MDL_BOTH, 4 is still enough
    }

    pColorMatcher = Create_ColorMatcher(pSS, cm,numinks, OutputWidth);

    err = pColorMatcher->MakeGrayMap(thePrinter->CMYMap, graymap);
    delete pColorMatcher;
    pColorMatcher = NULL;

    ERRCHECK;                       // possible return here

    if (pen == COLOR_PEN)
    {
        resPM = new CMYGrayMode(graymap);
    }
    else
    {
        resPM = new KCMYGrayMode(graymap);
    }

    if (resPM == NULL)
    {
        return ALLOCMEM_ERROR;
    }
//    resPM->myIndex = MAX_PRINTMODES;

    if (pen == COLOR_PEN)
    {
        resPM->bFontCapable = FALSE;
    }

    // make myIndex the last one in the list
    resPM->myIndex = thePrinter->GetModeCount();
/*
 *  [K]CMYGrayMode sets all resolution to 300 and quality to normal.
 *  Some printers, eg., DJ970, is 600 dpi in black and has bitdepth of 2.
 *  So, we cannot use normal mode for these printers and must set quality to
 *  draft.
 *  This is also true for 8x5 in one pen mode.
 *  Anyway, here, pen is both_pens or color_pen only.
 */

    PrintMode    *pPM = thePrinter->GetMode (DEFAULTMODE_INDEX);

//    if (pen != COLOR_PEN && pPM->ResolutionX[K] != 300)
    if (pPM->ResolutionX[K] != 300)
    {
        resPM->theQuality = qualityDraft;
        resPM->pmQuality  = QUALITY_DRAFT;
    }

/*
 *  Some printers do not use any data compression. Ex. DJ3320
 *  So, turn off the bCompress flag in such cases.
 */

    resPM->Config.bCompress = pPM->Config.bCompress;


#ifdef APDK_AUTODUPLEX
    resPM->bDuplexCapable = thePrinter->bDuplexCapable;
#endif

    return NO_ERROR;
} //SetCompGrayMode

// SetMediaSource
//! Select input media source bin
/*!
Used to set the bin number from which media will be loaded by the printer. This
is relevant for those printers that have multiple input bins. All other printers
will ignore the bin number. The typical bin numbers are
    1 - Upper Tray
    4 - Lower Tray
    5 - Duplexer Hagaki Feed
    7 - Auto Select
Any value between 1 and 50 is valid where there are more than 2 trays.
******************************************************************************/

DRIVER_ERROR PrintContext::SetMediaSource
(
    MediaSource num             //!< Bin Number
)
{
    if ((num > sourceTrayMax) || (num < sourceTrayMin))
         return WARN_INVALID_MEDIA_SOURCE;
    m_MediaSource = num;

    if (thePrinter != NULL)
    {
        BOOL bQueryHagakiTray = TRUE;
        if (num == sourceDuplexerNHagakiFeed && !thePrinter->HagakiFeedPresent(bQueryHagakiTray))
        {
            m_MediaSource = sourceTrayAuto;
        }
    }
    return NO_ERROR;
}

unsigned int PrintContext::GetCurrentDyeCount()
{ 
    return CurrentMode->dyeCount; 
}

PEN_TYPE PrintContext::GetDefaultPenSet()
{
    if (thePrinter==NULL)
        return NO_PEN;
    return thePrinter->DefaultPenSet();
}

PEN_TYPE PrintContext::GetInstalledPens()
{
    if(!thePrinter)
        return NO_PEN;
    return thePrinter->ePen;
}

void PrintContext::ResetIOMode (BOOL bDevID, BOOL bStatus)
{
    if (thePrinter)
    {
        thePrinter->IOMode.bDevID  = bDevID;
        thePrinter->IOMode.bStatus = bStatus;
        thePrinter->bCheckForCancelButton = bDevID;
    }
}

DRIVER_ERROR PrintContext::SetPrinterHint (PRINTER_HINT eHint, int iValue)
{
    if (thePrinter)
    {
        return thePrinter->SetHint (eHint, iValue);
    }
    return NO_PRINTER_SELECTED;
}

DRIVER_ERROR PrintContext::SetMediaType (MEDIATYPE eMediaType)
{
    if (CurrentMode == NULL)
        return NO_PRINTER_SELECTED;
    return CurrentMode->SetMediaType (eMediaType);
}

void PrintContext::SetJobAttributes (JobAttributes *pJA)
{
    if (m_job_attributes == NULL)
    {
        m_job_attributes = (JobAttributes *) new BYTE[sizeof (JobAttributes)];
    }
    if (m_job_attributes == NULL)
    {
        return;
    }

    memcpy(m_job_attributes, pJA, sizeof(JobAttributes));
    InputWidth = (int) (pJA->media_attributes.fPrintableWidth * EffectiveResolutionX ());
    OutputWidth = InputWidth;
//BUG("CurrentPrintMode=%d\n", CurrentPrintMode());
//BUG("fPrintableWidth=%0.5g EffectiveResolutionX=%d\n", pJA->media_attributes.fPrintableWidth, EffectiveResolutionX());
//BUG("InputWidth=%d OutputWidth=%d\n", InputWidth, OutputWidth);
}

int PrintContext::GetJobAttributes (int getWhat)
{
    if (m_job_attributes == NULL)
        return -1;

    if (getWhat == MEDIASIZE_PCL)
        return m_job_attributes->media_attributes.pcl_id;
    return -1;
}

APDK_END_NAMESPACE

