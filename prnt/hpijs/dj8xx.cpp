/*****************************************************************************\
  dj8xx.cpp : Implimentation for the DJ8xx class

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


#if defined(APDK_DJ8xx)|| defined(APDK_DJ9xx)

#include "header.h"
#include "io_defs.h"
#include "dj8xx.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE
extern BYTE* GetHT3x3_4();
extern BYTE* GetHT6x6_4();
APDK_END_NAMESPACE

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ895_KCMY[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ895_HB_KCMY[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ895_Binary_KCMY[ 9 * 9 * 9 ];
extern uint32_t ulMapGRAY_K_6x6x1[9 * 9 * 9];

DJ895Mode1::DJ895Mode1()    // Normal Color
: PrintMode(ulMapDJ895_KCMY)
// 600x600x1 K
// 300x300x2 CMY
{

    ColorDepth[K]=1;  // 600x600x1 K

    for (int i=1; i < 4; i++)
        ColorDepth[i]=2;    // 300x300x2 CMY

    ResolutionX[K]=ResolutionY[K]=600;

    MixedRes = TRUE;

    ColorFEDTable = GetHT3x3_4();

    bFontCapable = FALSE;       // DJ895 can't do fonts and hifipe at same time
}

DJ895Mode2::DJ895Mode2()    // Photo
: PrintMode(ulMapDJ895_HB_KCMY)
// 600x600x1 K
// 600x600x2 CMY
{
    int i;
    ColorDepth[K]=1;  // 600x600x1 K

    for (i=1; i < 4; i++)
        ColorDepth[i]=2;    // 600x600x2 CMY

    for (i=0; i < 4; i++)
        ResolutionX[i]=ResolutionY[i]=600;

    BaseResX = BaseResY = 600;
    MixedRes = FALSE;

    medium     = mediaGlossy;
    theQuality = qualityPresentation;

    ColorFEDTable = GetHT6x6_4();

     bFontCapable = FALSE;       // DJ895 can't do fonts and hifipe at same time

//     strcpy(ModeName, "Photo");

     pmQuality   = QUALITY_BEST;
     pmMediaType = MEDIA_PHOTO;
}

DJ895Mode3::DJ895Mode3()    // Draft Color
: PrintMode(ulMapDJ895_Binary_KCMY)
// 300x300x1 KCMY
{

    theQuality = qualityDraft;
//    strcpy(ModeName,"Draft");
    pmQuality = QUALITY_DRAFT;
}

DJ895Mode4::DJ895Mode4()    // Draft Gray K
: GrayMode(ulMapDJ600_CCM_K)
// 300x300x1 K
{
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
}

#ifdef APDK_EXTENDED_MEDIASIZE
DJ895Mode5::DJ895Mode5()    // Normal Gray K
: GrayMode(ulMapGRAY_K_6x6x1)
// 600x600x1 K
{
    BaseResX = BaseResY = 600;
    ResolutionX[K]=ResolutionY[K]=600;
    bFontCapable = FALSE;
}
#endif

DJ8xx::DJ8xx(SystemServices* pSS,
                       int numfonts, BOOL proto)
    : Printer(pSS, numfonts, proto)
{

    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else ePen=BOTH_PENS;    // matches default mode

#ifdef APDK_EXTENDED_MEDIASIZE
    pMode[GRAYMODE_INDEX]      = new DJ895Mode5 ();   // Normal Gray K
#else
    pMode[GRAYMODE_INDEX]      = new GrayMode (ulMapDJ600_CCM_K);   // Normal Gray K
#endif
    pMode[DEFAULTMODE_INDEX]   = new DJ895Mode1 ();   // Normal Color
    pMode[SPECIALMODE_INDEX]   = new DJ895Mode2 ();   // Photo
    pMode[SPECIALMODE_INDEX+1] = new DJ895Mode3 ();   // Draft Color
    pMode[SPECIALMODE_INDEX+2] = new DJ895Mode4 ();   // Draft Gray K
    ModeCount = 5;

    CMYMap = ulMapDJ895_Binary_KCMY;

    DBG1("DJ8xx created\n");
}


Header895::Header895(Printer* p,PrintContext* pc)
    : Header(p,pc)
{ }

DRIVER_ERROR Header895::Send()
{   DRIVER_ERROR err;

    StartSend();

    err = ConfigureRasterData();
    ERRCHECK;

    err=Graphics();     // start raster graphics and set compression mode

    // this is the pre-pick command but it doesn't work.  hmm...
    //    err = thePrinter->Send((const BYTE*)DJ895_Pre_Pick,sizeof(DJ895_Pre_Pick));

return err;
}

DRIVER_ERROR Header895::StartSend()
// common items gathered for code savings
{
    DRIVER_ERROR err;

    err = thePrinter->Send((const BYTE*)Reset,sizeof(Reset));
    ERRCHECK;

    err = thePrinter->Send((const BYTE*)UEL,sizeof(UEL));
    ERRCHECK;

    err = thePrinter->Send((const BYTE*)EnterLanguage,sizeof(EnterLanguage));
    ERRCHECK;

    if (!thePrinter->UseGUIMode(thePrintContext->CurrentMode))
        err = thePrinter->Send((const BYTE*)PCL3,sizeof(PCL3));
    else
        err = thePrinter->Send((const BYTE*)PCLGUI,sizeof(PCLGUI));
    ERRCHECK;

    err = thePrinter->Send((const BYTE*)&LF,1);
    ERRCHECK;

    err=Modes();            // Set media source, type, size and quality modes.
    ERRCHECK;

//  Now send media pre-load command
    err = thePrinter->Send ((const BYTE *) "\033&l-2H", 6);   
    ERRCHECK;

    char uom[10];
    sprintf(uom,"%c%c%c%d%c",ESC,'&','u',thePrintContext->EffectiveResolutionY(),'D');
    err=thePrinter->Send((const BYTE*)uom, strlen (uom));
    ERRCHECK;

    if (!thePrinter->UseGUIMode(thePrintContext->CurrentMode))
        err=Margins();          // set margins

    else    // special GUI mode top margin set
    {
        CAPy = thePrintContext->GUITopMargin();
    }

    return err;
}

extern BYTE EscAmplCopy(BYTE *dest, int num, char end);


DRIVER_ERROR Header895::Graphics()
// variant due to compression problems
{
    DRIVER_ERROR error= thePrinter->Send((const BYTE*)grafStart, sizeof(grafStart) );
    if (error!=NO_ERROR)
        return error;

    if (thePrintMode->Config.bCompress)
        {
            error= thePrinter->Send((const BYTE*)grafMode2, sizeof(grafMode2) );
            if (error!=NO_ERROR)
                return error;
        }
    else

        error= thePrinter->Send((const BYTE*)grafMode0, sizeof(grafMode0) );

return error;
#define GRAPHICSIZE (sizeof(grafStart)+sizeof(grafMode9)+sizeof(SeedSame))
}


Header* DJ8xx::SelectHeader(PrintContext* pc)
{
    return new Header895(this,pc);
}

DRIVER_ERROR DJ8xx::VerifyPenInfo()
{

    DRIVER_ERROR err = NO_ERROR;

    if(IOMode.bDevID == FALSE)
    {
        return err;
    }

    err = ParsePenInfo(ePen);

    if(err == UNSUPPORTED_PEN) // probably Power Off - pens couldn't be read
    {
        DBG1("DJ8xx::Need to do a POWER ON to get penIDs\n");

        // have to delay the POWER ON will be ignored
        if (pSS->BusyWait((DWORD)2000) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }

        DWORD length = sizeof(DJ895_Power_On);
        err = pSS->ToDevice(DJ895_Power_On,&length);
        ERRCHECK;

        err = pSS->FlushIO();
        ERRCHECK;

        // give the printer some time to power up
        if (pSS->BusyWait ((DWORD) 2500) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }

        err = ParsePenInfo(ePen);
    }

    ERRCHECK;

    // check for the normal case
    if (ePen == BOTH_PENS)
    {
        return NO_ERROR;
    }

    while ( ePen != BOTH_PENS   )
    {

        switch (ePen)
        {
            case BLACK_PEN:
                // black pen installed, need to install color pen
                pSS->DisplayPrinterStatus(DISPLAY_NO_COLOR_PEN);
                break;
            case COLOR_PEN:
                // color pen installed, need to install black pen
                pSS->DisplayPrinterStatus(DISPLAY_NO_BLACK_PEN);
                break;
            case NO_PEN:
                // neither pen installed
            default:
                pSS->DisplayPrinterStatus(DISPLAY_NO_PENS);
                break;
        }

        if (pSS->BusyWait(500) == JOB_CANCELED)
        {
            return JOB_CANCELED;
        }

        err =  ParsePenInfo(ePen);
        ERRCHECK;
    }

    pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

    return NO_ERROR;
}

DRIVER_ERROR DJ8xx::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
{
    char* str;
    DRIVER_ERROR err = SetPenInfo (str, QueryPrinter);
    ERRCHECK;

    if (*str != '$')
    {
        return BAD_DEVICE_ID;
    }

    // parse penID
    PEN_TYPE temp_pen1;
    // check pen1, assume it is black, pen2 is color
    switch (str[1])
    {
        case 'H': // (H)obbes
        case 'L': // (L)inus
        case 'Z': // (Z)orro
        {
            temp_pen1 = BLACK_PEN;
            break;
        }
        case 'X': return UNSUPPORTED_PEN;
        default:  temp_pen1 = NO_PEN; break;
    }

    // now check pen2

    int i = 2;
    while ((i < DevIDBuffSize) && str[i] != '$') i++; // handles variable length penIDs
    if (i == DevIDBuffSize)
    {
        return BAD_DEVICE_ID;
    }

    i++;

    // need to be more forgiving of the color pen type because of
    // the unknown chinookID for DJ970
    // we can't guarantee the (F)lash color pen, but we can make sure
    // the pen is not (X)Undefined, (A)Missing or (M)onet
    if(str[i] != 'X' && str[i] != 'A' && str[i] != 'M')
    // check what pen1 was
    {
        if (temp_pen1 == BLACK_PEN)
        {
            ePen = BOTH_PENS;
        }
        else
        {
            ePen = COLOR_PEN;
        }
    }
    else // no color pen, just set what pen1 was
    {
        ePen = temp_pen1;
    }

    return NO_ERROR;
}


Compressor* DJ8xx::CreateCompressor(unsigned int RasterSize)
{
    DBG1("Compression = Mode2\n");
    return new Mode2(pSS,RasterSize);
}

BOOL DJ8xx::UseGUIMode(PrintMode* pPrintMode)
{
    return (!pPrintMode->bFontCapable);
}

DRIVER_ERROR DJ8xx::CleanPen()
{
    const BYTE DJ895_Diag_Page[] = {ESC, '%','P','u','i','f','p','.',
        'm','u','l','t','i','_','b','u','t','t','o','n','_','p','u','s','h',' ','4',';',
        'u','d','w','.','q','u','i','t',';',ESC,'%','-','1','2','3','4','5','X' };

    const BYTE PEN_CLEAN_PML[]={0x1B,0x45,0x1B,0x26,0x62,0x31,0x36,0x57,
                0x50,0x4D,0x4C,0x20,  // EscE Esc&b16WPML{space}
                0x04,0x00,0x06,0x01,0x04,0x01,0x05,0x01,
                0x01,0x04,0x01,0x64}; // PML Marking-Agent-Maintenance=100

    DWORD length = sizeof(DJ895_Power_On);
    DRIVER_ERROR Error = pSS->ToDevice(DJ895_Power_On,&length);

    pSS->BusyWait((DWORD)1000);

    length = sizeof(PEN_CLEAN_PML);
    Error = pSS->ToDevice(PEN_CLEAN_PML,&length);

    length = sizeof(DJ895_Diag_Page);
    return pSS->ToDevice(DJ895_Diag_Page,&length);
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

DISPLAY_STATUS DJ8xx::ParseError(BYTE status_reg)
{
    DBG1("DJ8xx: parsing error info\n");

    DRIVER_ERROR err = NO_ERROR;
    BYTE DevIDBuffer[DevIDBuffSize];
 
    char *pStr;
    if(IOMode.bDevID)
    {
        // If a bi-di cable was plugged in and everything was OK, let's see if it's still
        // plugged in and everything is OK
        err = pSS->GetDeviceID(DevIDBuffer, DevIDBuffSize, TRUE);
        if(err != NO_ERROR)
            // job was bi-di but now something's messed up, probably cable unplugged
            return DISPLAY_COMM_PROBLEM;

        if ( TopCoverOpen(status_reg) )
        {
            DBG1("Top Cover Open\n");
            return DISPLAY_TOP_COVER_OPEN;
        }


        if ( (pStr=(char *)strstr((const char*)DevIDBuffer+2,"VSTATUS:")) == NULL )
        {
            return DISPLAY_COMM_PROBLEM;
        }

        pStr+=8;   // skip "VSTATUS:"
		// Paper Jam or Paper Stall
        if (strstr((char*)pStr,"PAJM") || strstr((char*)pStr,"PAPS"))
        {
            return DISPLAY_PAPER_JAMMED;
        }

		// Power down or printing turning off
        if (strstr((char*)pStr,"PWDN") || strstr((char*)pStr,"OFFF"))
        {
            return DISPLAY_OFFLINE;
        }


        // VerifyPenInfo will handle prompting the user
        // if this is a problem
        err = VerifyPenInfo();

        if(err != NO_ERROR)
            // VerifyPenInfo returned an error, which can only happen when ToDevice
            // or GetDeviceID returns an error. Either way, it's BAD_DEVICE_ID or
            // IO_ERROR, both unrecoverable.  This is probably due to the printer
            // being turned off during printing, resulting in us not being able to
            // power it back on in VerifyPenInfo, since the buffer still has a
            // partial raster in it and we can't send the power-on command.
            return DISPLAY_COMM_PROBLEM;
    }

    // check for errors we can detect from the status reg
    if (IOMode.bStatus)
    {
        if ( DEVICE_IS_OOP(status_reg) )
        {
            DBG1("Out Of Paper\n");
            return DISPLAY_OUT_OF_PAPER;
        }

        if (DEVICE_PAPER_JAMMED(status_reg))
        {
            DBG1("Paper Jammed\n");
            return DISPLAY_PAPER_JAMMED;
        }

        if (DEVICE_IO_TRAP(status_reg))
        {
            DBG1("IO trap\n");
            return DISPLAY_ERROR_TRAP;
        }
    }

    // don't know what the problem is-
    //  Is the PrinterAlive?
    if (pSS->PrinterIsAlive())
    {
        iTotal_SLOW_POLL_Count += iMax_SLOW_POLL_Count;
#if defined(DEBUG) && (DBG_MASK & DBG_LVL1)
        printf("iTotal_SLOW_POLL_Count = %d\n",iTotal_SLOW_POLL_Count);
#endif
        // -Note that iTotal_SLOW_POLL_Count is a multiple of
        //  iMax_SLOW_POLL_Count allowing us to check this
        //  on an absolute time limit - not relative to the number
        //  of times we happen to have entered ParseError.
        // -Also note that we have different thresholds for uni-di & bi-di.
        if(
            ((IOMode.bDevID == FALSE) && (iTotal_SLOW_POLL_Count >= 60)) ||
            ((IOMode.bDevID == TRUE)  && (iTotal_SLOW_POLL_Count >= 120))
          )
        {
            return DISPLAY_BUSY;
        }
        else
        {
            return DISPLAY_PRINTING;
        }
    }
    else
    {
        return DISPLAY_COMM_PROBLEM;
    }
}

APDK_END_NAMESPACE

#endif  // defined APDK_DJ8xx or APDK_9xx
