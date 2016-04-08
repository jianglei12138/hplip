/*****************************************************************************\
  ljcolor.cpp : Implimentation for the LJColor class

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


#ifdef APDK_LJCOLOR

#include "header.h"
#include "io_defs.h"
#include "ljcolor.h"
#include "printerproxy.h"
#include "resources.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];
extern uint32_t ulMapGRAY_K_6x6x1[9 * 9 * 9];

LJColor::LJColor (SystemServices* pSS, int numfonts, BOOL proto)
    : Printer(pSS, numfonts, proto)
{

    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else ePen = BOTH_PENS;    // matches default mode

    pMode[GRAYMODE_INDEX]      = new LJColorGrayMode ();
    pMode[DEFAULTMODE_INDEX]   = new LJColor300DPIMode ();
    pMode[SPECIALMODE_INDEX]   = new LJColor600DPIMode ();
    pMode[SPECIALMODE_INDEX+1] = new LJColor150DPIMode ();
    pMode[SPECIALMODE_INDEX+2] = new LJColorKDraftMode ();
    ModeCount = 5;


    CMYMap = NULL;
    m_bJobStarted = FALSE;
#ifdef  APDK_AUTODUPLEX
    m_bRotateBackPage = FALSE;  // Lasers don't require back side image to be rotated
#endif
#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[SPECIALMODE_INDEX+3] = new LJColorPlainBestMode ();
    ModeCount = 6;
#endif
    m_pCompressor = NULL;
    m_iYPos = 0;

    DBG1("LJColor created\n");
}

LJColor::~LJColor ()
{
    DISPLAY_STATUS  eDispStatus;
    if (IOMode.bStatus && m_bJobStarted)
    {
        for (int i = 0; i < 5; i++)
        {
            pSS->BusyWait (2000);
            eDispStatus = ParseError (0);
            if (eDispStatus == DISPLAY_PRINTING_COMPLETE)
            {
                pSS->DisplayPrinterStatus (eDispStatus);
                break;
            }
        }
    }
}

LJColorKDraftMode::LJColorKDraftMode ()
: GrayMode (ulMapDJ600_CCM_K)
{
    theQuality = qualityDraft;
    pmQuality  = QUALITY_DRAFT;
    pmColor    = GREY_K;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

LJColorGrayMode::LJColorGrayMode () : GrayMode (ulMapGRAY_K_6x6x1)
{
	ResolutionX[0] = 
	ResolutionY[0] = 
	BaseResX =
	BaseResY = 600;
    pmColor = GREY_K;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

LJColor150DPIMode::LJColor150DPIMode ()
: PrintMode (NULL)
{
    ResolutionX[0] = ResolutionY[0] = 150;
    BaseResX = BaseResY = 150;
	TextRes = 150;
	Config.bColorImage = FALSE;
	theQuality = qualityDraft;
	pmQuality  = QUALITY_DRAFT;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

LJColor300DPIMode::LJColor300DPIMode ()
: PrintMode (NULL)
{
    ResolutionX[0] = 300;
    ResolutionY[0] = 300;
    BaseResX = BaseResY = 300;

    Config.bColorImage = FALSE;
	theQuality = qualityNormal;

    bFontCapable = TRUE;
    pmQuality = QUALITY_NORMAL;
	Config.bColorImage = FALSE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

LJColor600DPIMode::LJColor600DPIMode ()
: PrintMode (NULL)
{
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    BaseResX = BaseResY = 600;
    Config.bColorImage = FALSE;
	theQuality = qualityPresentation;
    bFontCapable = TRUE;
    pmQuality   = QUALITY_BEST;
    pmMediaType = MEDIA_PHOTO;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

#ifdef  APDK_EXTENDED_MEDIASIZE
LJColorPlainBestMode::LJColorPlainBestMode ()
: PrintMode (NULL)
{
    ResolutionX[0] = 600;
    ResolutionY[0] = 600;
    BaseResX = BaseResY = 600;

    Config.bColorImage = FALSE;
	theQuality = qualityPresentation;
    bFontCapable = TRUE;
    pmQuality   = QUALITY_BEST;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}
#endif

HeaderLJColor::HeaderLJColor (Printer* p, PrintContext* pc)
    : Header(p,pc)
{ }

DRIVER_ERROR HeaderLJColor::Send ()
{
    DRIVER_ERROR err;
    char    uom[12];

    COLORMODE       eC = COLOR;
    MEDIATYPE       eM;
    QUALITY_MODE    eQ;
    BOOL            bD;

    ((LJColor *)thePrinter)->bGrey_K = FALSE;
    if ((thePrintContext->GetPrintModeSettings (eQ, eM, eC, bD)) == NO_ERROR &&
        eC == GREY_K)
    {
        ((LJColor *)thePrinter)->bGrey_K = TRUE;
    }

    StartSend ();

    if (eC != GREY_K)
    {

/*
 *      Configure image data - ESC*v#W - # = 6 bytes
 *      02 - RGB colorspace (00 - Device RGB)
 *      03 - Direct pixel
 *      08 - bits per index - ignored for direct pixel
 *      08, 08, 08 - bits per primary each
 */

        err = thePrinter->Send ((const BYTE *) "\033*v6W\00\03\010\010\010\010", 11);
        ERRCHECK;

//      Continues tone dither
//      Logical operation - 0

//      err = thePrinter->Send ((const BYTE *) "\033*t18J\033*l204O", 13);
        err = thePrinter->Send ((const BYTE *) "\033*t18J", 6);

        ERRCHECK;

/*
 *      Driver Configuration Command - ESC*#W - # = 3 bytes
 *      device id - 6 = color HP LaserJet Printer
 *      func index - 4 = Select Colormap
 *      argument - 2   = Vivid Graphics
 */

        err = thePrinter->Send ((const BYTE *) "\033*o3W\06\04\06", 8);
        ERRCHECK;

/*
 *      Program color palette entries
 */
        err = thePrinter->Send ((const BYTE *) "\033*v255A\033*v255B\033*v255C\033*v0I", 26);
        ERRCHECK;

        err = thePrinter->Send ((const BYTE *) "\033*v255A\033*v0B\033*v0C\033*v6I", 22);
        ERRCHECK;
        err = thePrinter->Send ((const BYTE *) "\033*v0A\033*v255B\033*v0C\033*v5I", 22);
        ERRCHECK;
        err = thePrinter->Send ((const BYTE *) "\033*v0A\033*v0B\033*v255C\033*v3I", 22);
        ERRCHECK;
        err = thePrinter->Send ((const BYTE *) "\033*v255A\033*v255B\033*v0C\033*v4I", 24);
        ERRCHECK;
        err = thePrinter->Send ((const BYTE *) "\033*v255A\033*v0B\033*v255C\033*v2I", 24);
        ERRCHECK;
        err = thePrinter->Send ((const BYTE *) "\033*v0A\033*v255B\033*v255C\033*v1I", 24);
        ERRCHECK;
        err = thePrinter->Send ((const BYTE *) "\033*v0A\033*v0B\033*v0C\033*v7I", 20);
        ERRCHECK;
    }

    sprintf (uom, "\033*r%dS", thePrintContext->OutputPixelsPerRow ());
    err = thePrinter->Send ((const BYTE*)uom, strlen (uom));
    ERRCHECK;

    err = Graphics ();     // start raster graphics and set compression mode

    return err;
}

DRIVER_ERROR HeaderLJColor::StartSend ()
{
    DRIVER_ERROR err;
    char    res[72];
    int     iRes;

    iRes = thePrintContext->EffectiveResolutionY ();

    err = thePrinter->Send ((const BYTE*)UEL,sizeof(UEL));
    ERRCHECK;

    sprintf (res, "@PJL SET PAGEPROTECT=AUTO@PJL SET RESOLUTION=%d\015\012", iRes);
    err = thePrinter->Send ((const BYTE *) res, strlen (res));
    ERRCHECK;

    if (thePrinter->IOMode.bStatus)
    {
        sprintf (res, "@PJL JOB NAME = \"%ld\"\015\012", (long) (thePrinter));
        err = thePrinter->Send ((const BYTE *) res, strlen (res));
        ERRCHECK;
    }

    QUALITY_MODE    eQ = QUALITY_NORMAL;
    COLORMODE       eC;
    MEDIATYPE       eM;
    BOOL            bD;

    thePrintContext->GetPrintModeSettings (eQ, eM, eC, bD);

    if (eQ == QUALITY_DRAFT)
    {
        strcpy (res, "@PJL SET RET=OFF\015\012@PJL SET ECONOMODE=ON\015\012");
        err = thePrinter->Send ((const BYTE *) res, strlen (res));
        ERRCHECK;
    }

    if (thePrinter->IOMode.bStatus)
    {
        strcpy (res, "@PJL USTATUSOFF\015\012@PJL USTATUS DEVICE = ON\015\012@PJL USTATUS JOB = ON\015\012");
        err = thePrinter->Send ((const BYTE *) res, strlen (res));
        ERRCHECK;
    }

//  Duplexing directive

    strcpy (res, "@PJL SET DUPLEX=OFF\015\012");

#ifdef APDK_AUTODUPLEX
    DUPLEXMODE  dupmode = thePrintContext->QueryDuplexMode ();
    if (dupmode != DUPLEXMODE_NONE)
    {
        strcpy (res, "@PJL SET DUPLEX=ON\015\012@PJL SET BINDING=");
        if (dupmode == DUPLEXMODE_BOOK)
            strcat (res, "LONGEDGE\015\012");
        else
            strcat (res, "SHORTEDGE\015\012");
    }
#endif

    err = thePrinter->Send ((const BYTE *) res, strlen (res));


    err = thePrinter->Send ((const BYTE*) EnterLanguage, sizeof (EnterLanguage));
    ERRCHECK;

    err = thePrinter->Send ((const BYTE*) "PCL\015\012", 5);
    ERRCHECK;

    err = thePrinter->Send ((const BYTE*) Reset,sizeof (Reset));
    ERRCHECK;

    sprintf (res, "\033&l%dH", thePrintContext->GetMediaSource ());
    err = thePrinter->Send ((const BYTE *) res, strlen (res));		// Source
    ERRCHECK;

//  Media size, vertical spacing between lines and top margin

    memcpy (res, mediasize, mscount - 1);
    strcpy (res+mscount-1, "a8c0E");
    err = thePrinter->Send ((const BYTE *) res, strlen (res));
    ERRCHECK;

	sprintf (res, "\033*t%dR\033&u%dD", iRes, iRes);
    err=thePrinter->Send ((const BYTE*) res, strlen (res));
    ERRCHECK;

    err = Margins ();
    ERRCHECK;
    CAPy = 0;

//  Default is single sided printing

    strcpy (res, "\033&l0S");

#ifdef APDK_AUTODUPLEX
    DUPLEXMODE  eDupMode = thePrintContext->QueryDuplexMode ();
    if (eDupMode != DUPLEXMODE_NONE)
    {
        sprintf (res, "\033&l%dS", (eDupMode == DUPLEXMODE_BOOK) ? 1 : 2);
    }
#endif
    err = thePrinter->Send ((const BYTE *) res, strlen (res));
    ERRCHECK;

/*
 *  Set orientation to Portrait. APDK supports printing in Portrait mode only.
 *  If users desire Landscape printing, application/gluecode will have to
 *  rearrange the rasters appropriately.
 */

    err = thePrinter->Send ((const BYTE *) "\033&l0O", 5);

    // Number of copies
    sprintf (res, "\033&l%dX", thePrintContext->GetCopyCount ());
    err = thePrinter->Send ((const BYTE *) res, strlen (res));
    ERRCHECK;

    return err;
}

DRIVER_ERROR HeaderLJColor::Graphics ()
{

    DRIVER_ERROR err;
    err = thePrinter->Send ((const BYTE*) grafStart, sizeof (grafStart));
    ERRCHECK;

    if (((LJColor *) thePrinter)->bGrey_K)
    {
        err = thePrinter->Send ((const BYTE*) "\033*b2M", 5);
        ERRCHECK;
    }
    else
    {
        err = thePrinter->Send ((const BYTE*) "\033*b3M", 5);
    }

    return err;
}

DRIVER_ERROR HeaderLJColor::EndJob ()
{
    char    szBuff[128];
    DRIVER_ERROR    err = NO_ERROR;
    if (thePrinter->IOMode.bStatus)
    {
        sprintf (szBuff, "\033E\033%%-12345X@PJL EOJ NAME = \"%ld\"\015\012@PJL RESET\015\012",
                 (long) (thePrinter));
        err = thePrinter->Send ((const BYTE *) szBuff, strlen (szBuff));
        ERRCHECK;
    }
    strcpy (szBuff, "\033%-12345X");
    err = thePrinter->Send ((const BYTE *) szBuff, strlen (szBuff));

    return err;
}

DRIVER_ERROR HeaderLJColor::FormFeed ()
{
    ((LJColor *)thePrinter)->bFGColorSet = FALSE;
    ((LJColor *)thePrinter)->m_iYPos = 0;

    return (thePrinter->Send ((const BYTE *) "\014", 1));
}

DRIVER_ERROR HeaderLJColor::SendCAPy (unsigned int iAbsY)
{
    if (iAbsY == 0)
    {
        return thePrinter->Send ((const BYTE *) "\033*p0Y", 5);
    }
    return NO_ERROR;
}

DRIVER_ERROR LJColor::Encapsulate (const RASTERDATA* InputRaster, BOOL bLastPlane)
{
    char    szScratchStr[16];
    DRIVER_ERROR    err;
    m_iYPos++;
    if (bFGColorSet == FALSE)
    {
        Send ((const BYTE *) "\033*v7S", 5);
        bFGColorSet = TRUE;
    }
    sprintf (szScratchStr, "\033*b%uW", InputRaster->rastersize[COLORTYPE_COLOR]);
    err = Send ((const BYTE *) szScratchStr, strlen (szScratchStr));
    if (err == NO_ERROR)
    {
        err = Send (InputRaster->rasterdata[COLORTYPE_COLOR], InputRaster->rastersize[COLORTYPE_COLOR]);
    }

/*
 *  Printers with low memory (64 MB or less) can run out of memory during decompressing
 *  the image data and will abort the job. To prevent this, restart raster command.
 *  Raghu
 */

    if (!bGrey_K &&
        m_iYResolution == 600 &&
        m_iYPos % 1200 == 0)
    {
        SkipRasters (0);
        err = Send ((const BYTE *) "\033*rC\033*r1A\033*b3M", 14);
    }

    return err;
}

DRIVER_ERROR LJColor::SkipRasters (int nBlankRasters)
{
    char    szScratchStr[16];
    DRIVER_ERROR    err = NO_ERROR;
    if (m_pCompressor)
    {
        m_pCompressor->Flush ();
    }
    if (nBlankRasters > 0)
    {
        m_iYPos += nBlankRasters;
        sprintf (szScratchStr, "\033*p%dY", m_iYPos);
        err = Send ((const BYTE *) szScratchStr, strlen (szScratchStr));
    }
    return err;
}


Header* LJColor::SelectHeader (PrintContext *pc)
{
    m_iYResolution = pc->EffectiveResolutionY ();
    return new HeaderLJColor (this, pc);
}

DRIVER_ERROR LJColor::VerifyPenInfo()
{

    DRIVER_ERROR err = NO_ERROR;

    if(IOMode.bDevID == FALSE)
    {
        return err;
    }

    ePen = BOTH_PENS;
    return NO_ERROR;
}

DRIVER_ERROR LJColor::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{
//    char* str;
//    DRIVER_ERROR err = SetPenInfo(str, QueryPrinter);
//    ERRCHECK;

    ePen = BOTH_PENS;

    return NO_ERROR;
}

Compressor* LJColor::CreateCompressor (unsigned int RasterSize)
{
    bFGColorSet = FALSE;
    if (bGrey_K)
        return new Mode2 (pSS, RasterSize);

    m_pCompressor = new Mode3 (pSS, this, RasterSize);
    return m_pCompressor;
}

/*
 *  Function name: ParseError
 *
 *  Owner: Darrell Walker
 *
 *  Purpose:  To determine what error state the printer is in.
 *
 *  Called by: Send()
 *
 *  Parameters on entry: status_reg is the contents of the centronics
 *                      status register (at the time the error was
 *                      detected)
 *
 *  Parameters on exit: unchanged
 *
 *  Return Values: The proper DISPLAY_STATUS to reflect the printer
 *              error state.
 *
 */

/*  We have to override the base class's (Printer) ParseError function due
    to the fact that the 8XX series returns a status byte of F8 when it's out of
    paper.  Unfortunately, the 600 series returns F8 when they're turned off.
    The way things are structured in Printer::ParseError, we used to check only
    for DEVICE_IS_OOP.  This would return true even if we were connected to a
    600 series printer that was turned off, causing the Out of paper status
    message to be displayed.  This change also reflects a corresponding change
    in IO_defs.h, where I split DEVICE_IS_OOP into DEVICE_IS_OOP, DJ400_IS_OOP, and
    DJ8XX_IS_OOP and we now check for DJ8XX_IS_OOP in the DJ8xx class's
    ParseError function below.  05/11/99 DGC.
*/

DISPLAY_STATUS LJColor::ParseError(BYTE status_reg)
{
    DBG1("LJColor: parsing error info\n");

    DRIVER_ERROR err = NO_ERROR;
    BYTE    szReadBuff[256];
    DWORD   iReadCount = 256;
    DISPLAY_STATUS  eStatus = (DISPLAY_STATUS) status_reg;
    char    *tmpStr;
    int     iErrorCode;

    if (!IOMode.bDevID)
        return eStatus;

    memset (szReadBuff, 0, 256);
    err = pSS->FromDevice (szReadBuff, &iReadCount);
    if (err == NO_ERROR && iReadCount == 0)
        return eStatus;

    if (strstr ((char *) szReadBuff, "JOB"))
    {
        if (!(tmpStr = strstr ((char *) szReadBuff, "NAME")))
            return DISPLAY_PRINTING;
        tmpStr += 6;
        while (*tmpStr < '0' || *tmpStr > '9')
            tmpStr++;
        sscanf (tmpStr, "%d", &iErrorCode);
        if (iErrorCode != (long) (this))
            return DISPLAY_PRINTING;
    }

    if (strstr ((char *) szReadBuff, "END"))
    {
        return DISPLAY_PRINTING_COMPLETE;
    }


    if (strstr ((char *) szReadBuff, "CANCEL"))
        return DISPLAY_PRINTING_CANCELED;

    if (!(tmpStr = strstr ((char *) szReadBuff, "CODE")))
        return eStatus;

    tmpStr += 4;
    while (*tmpStr < '0' || *tmpStr > '9')
        tmpStr++;
    sscanf (tmpStr, "%d", &iErrorCode);

    if (iErrorCode < 32000)
        return DISPLAY_PRINTING;

    if (iErrorCode == 40010 || iErrorCode == 40020)
        return DISPLAY_NO_PENS;     // Actually, out of toner

    if (iErrorCode == 40021)
        return DISPLAY_TOP_COVER_OPEN;

    if ((iErrorCode / 100) == 419)
        return DISPLAY_OUT_OF_PAPER;

    if ((iErrorCode / 1000) == 42 || iErrorCode == 40022)
    {
        DBG1("Paper Jammed\n");
        return DISPLAY_PAPER_JAMMED;
    }

    if (iErrorCode > 40049 && iErrorCode < 41000)
    {
        DBG1("IO trap\n");
        return DISPLAY_ERROR_TRAP;
    }

    if (iErrorCode == 40079)
        return DISPLAY_OFFLINE;

    return DISPLAY_ERROR_TRAP;
}

APDK_END_NAMESPACE

#endif  // defined APDK_LJCOLOR
