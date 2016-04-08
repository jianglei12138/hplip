/*****************************************************************************\
  ljmono.cpp : Implimentation for the LJMono class

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


#ifdef APDK_LJMONO

#include "header.h"
#include "io_defs.h"
#include "ljmono.h"
#include "printerproxy.h"
#include "resources.h"

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];

LJMono::LJMono (SystemServices* pSS, int numfonts, BOOL proto)
    : Printer(pSS, numfonts, proto)
{

    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo ();
        CERRCHECK;
    }
    else ePen = BLACK_PEN;    // matches default mode

    pMode[GRAYMODE_INDEX]    = new LJMonoDraftMode ();
    pMode[DEFAULTMODE_INDEX] = new LJMonoNormalMode ();
    pMode[SPECIALMODE_INDEX] = new LJMonoBestMode ();
    ModeCount = 3;

    CMYMap = NULL;
    m_bJobStarted = FALSE;
#ifdef  APDK_AUTODUPLEX
    m_bRotateBackPage = FALSE;  // Lasers don't require back side image to be rotated
#endif

    DBG1("LJMono created\n");
}

LJMono::~LJMono ()
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

LJMonoDraftMode::LJMonoDraftMode ()
: GrayMode(ulMapDJ600_CCM_K)
{

//  By default, this is 150 dpi because LJ_BASE_RES is defined to be 300
//  unless APDK_HIGH_RES_MODES is defined, then VIP_BASE_RES will be 600

    ResolutionX[0] =
    ResolutionY[0] = LJ_BASE_RES;
    BaseResX =
    BaseResY = LJ_BASE_RES;
    MixedRes = FALSE;
    bFontCapable = TRUE;
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

LJMonoNormalMode::LJMonoNormalMode ()
: GrayMode(ulMapDJ600_CCM_K)
{

//  300 or 600 dpi depending on LJ_BASE_RES value, which in turn is affected by APDK_HIGH_RES_MODES

    ResolutionX[0] =
    ResolutionY[0] = LJ_BASE_RES * 2;
    BaseResX =
    BaseResY = LJ_BASE_RES * 2;
	TextRes  = LJ_BASE_RES * 2;
    MixedRes = FALSE;
    bFontCapable = TRUE;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

LJMonoBestMode::LJMonoBestMode ()
: GrayMode(ulMapDJ600_CCM_K)
{
    ResolutionX[0] = ResolutionY[0] = 600;
    BaseResX = BaseResY = 600;
    TextRes = 600;
    MixedRes = FALSE;
    bFontCapable = TRUE;
	pmQuality = QUALITY_BEST;
#ifdef APDK_AUTODUPLEX
    bDuplexCapable = TRUE;
#endif
}

HeaderLJMono::HeaderLJMono (Printer* p,PrintContext* pc)
    : Header(p,pc)
{ }

DRIVER_ERROR HeaderLJMono::Send ()
{
    DRIVER_ERROR err;

    StartSend ();

    err = Graphics ();     // start raster graphics and set compression mode

    return err;
}

DRIVER_ERROR HeaderLJMono::StartSend ()
{
    DRIVER_ERROR err;
    char    res[96];
    int     iRes;

    iRes = thePrintContext->EffectiveResolutionY ();

    err = thePrinter->Send((const BYTE*)UEL,sizeof(UEL));
    ERRCHECK;

    sprintf (res, "@PJL SET PAGEPROTECT=AUTO\015\012@PJL SET RESOLUTION=%d\015\012", iRes);
    err = thePrinter->Send ((const BYTE *) res, strlen (res));
    ERRCHECK;

   err = thePrinter->Send ((const BYTE *) "@PJL SET DENSITY=5\015\012", 20); // for lj1100, des 8/7/02
   ERRCHECK;

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
        sprintf (res, "@PJL JOB NAME = \"%ld\"\015\012", (long) (thePrinter));
        err = thePrinter->Send ((const BYTE *) res, strlen (res));
        ERRCHECK;

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
    ERRCHECK;

    err = thePrinter->Send ((const BYTE*) EnterLanguage, sizeof (EnterLanguage));
    ERRCHECK;

    err = thePrinter->Send ((const BYTE*) "PCL\015\012", 5);
    ERRCHECK;

    err = thePrinter->Send ((const BYTE*)Reset,sizeof(Reset));
    ERRCHECK;

    sprintf (res, "\033&l%dH", thePrintContext->GetMediaSource());
    err = thePrinter->Send ((const BYTE *) res, strlen (res));      // Source
    ERRCHECK;

//  Media size, vertical spacing between lines and top margin

    memcpy (res, mediasize, mscount - 1);
    strcpy (res+mscount-1, "a8c0E");
    err = thePrinter->Send ((const BYTE *) res, strlen (res));
    ERRCHECK;

    sprintf (res, "\033*t%dR\033&u%dD", iRes, iRes);
    err = thePrinter->Send ((const BYTE *) res, 14);
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

DRIVER_ERROR HeaderLJMono::Graphics ()
{
    DRIVER_ERROR error = thePrinter->Send ((const BYTE*)grafStart, sizeof (grafStart) );
    if (error!=NO_ERROR)
        return error;

    error= thePrinter->Send((const BYTE*) grafMode2, sizeof (grafMode2) );

    if (error!=NO_ERROR)
        return error;

    return error;
}

DRIVER_ERROR HeaderLJMono::EndJob ()
{
    char    szBuff[128];
    DRIVER_ERROR    err = NO_ERROR;
    if (thePrinter->IOMode.bStatus)
    {
        sprintf (szBuff, "\033E\033%%-12345X@PJL EOJ NAME = \"%ld\"\015\012@PJL RESET\015\012",
                (long) (thePrinter));
        err = thePrinter->Send ((const BYTE *) szBuff, strlen (szBuff));
    }

    strcpy (szBuff, "\033%-12345X");
    err = thePrinter->Send ((const BYTE *) szBuff, strlen (szBuff));

    return err;
}

Header* LJMono::SelectHeader (PrintContext* pc)
{
    m_bJobStarted = TRUE;
    return new HeaderLJMono (this,pc);
}

DRIVER_ERROR LJMono::VerifyPenInfo()
{

    DRIVER_ERROR err = NO_ERROR;

    if(IOMode.bDevID == FALSE)
    {
        return err;
    }

    ePen = BLACK_PEN;
    return NO_ERROR;
}

DRIVER_ERROR LJMono::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{

    ePen = BLACK_PEN;

    return NO_ERROR;
}

Compressor* LJMono::CreateCompressor (unsigned int RasterSize)
{
    return new Mode2 (pSS,RasterSize);
}

DISPLAY_STATUS LJMono::ParseError (BYTE status_reg)
{
    DBG1("LJMono: parsing error info\n");

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

#endif  // defined APDK_LJMono
