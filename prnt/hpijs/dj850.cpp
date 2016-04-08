/*****************************************************************************\
  dj850.cpp : Implimentation for the DJ850 class

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

#if defined(APDK_DJ850)

#include "header.h"
#include "io_defs.h"
#include "dj850.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE
extern BYTE* GetHT3x3_4();
extern BYTE* GetHT6x6_4();
APDK_END_NAMESPACE

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];
extern uint32_t ulMapDJ850_Normal_KCMY[9 * 9 * 9];
extern uint32_t ulMapDJ600_CCM_K[ 9 * 9 * 9 ];
extern uint32_t ulMapGRAY_K_6x6x1[9 * 9 * 9];

DJ850Mode1::DJ850Mode1()    // Normal Color
: PrintMode(ulMapDJ850_Normal_KCMY)
// 600x600x1 K
// 300x300x2 CMY
{
    ColorDepth[K]=1;  // 600x600x1 K

    for (int i=1; i < 4; i++)
        ColorDepth[i]=2;    // 300x300x2 CMY

    ResolutionX[K]=ResolutionY[K]=600;
    MixedRes = TRUE;

    ColorFEDTable = GetHT3x3_4();

}

DJ850Mode3::DJ850Mode3()    // Draft Color
: PrintMode(ulMapDJ850_Normal_KCMY)
// 300x300x1 KCMY
{
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
}

DJ850Mode4::DJ850Mode4()    // Draft Gray K
: GrayMode(ulMapDJ600_CCM_K)
// 300x300x1 K
{
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
}

DJ850Mode5::DJ850Mode5()    // Normal Gray K
: GrayMode(ulMapGRAY_K_6x6x1)
// 600x600x1 K
{
    BaseResX = BaseResY = 600;
    ResolutionX[K]=ResolutionY[K]=600;
}

DJ850::DJ850(SystemServices* pSS,
                       int numfonts, BOOL proto)
    : Printer(pSS, numfonts, proto)
{
    if ((!proto) && (IOMode.bDevID))
    {
        constructor_error = VerifyPenInfo();
        CERRCHECK;
    }
    else ePen=BOTH_PENS;    // matches default mode

    pMode[GRAYMODE_INDEX]      = new DJ850Mode5 ();   // Normal Gray K
    pMode[DEFAULTMODE_INDEX]   = new DJ850Mode1 ();   // Normal Color
    pMode[SPECIALMODE_INDEX] = new DJ850Mode3 ();   // Draft Color
    pMode[SPECIALMODE_INDEX+1] = new DJ850Mode4 ();   // Draft Gray K
    ModeCount = 4;

    CMYMap = ulMapDJ850_Normal_KCMY;

    DBG1("DJ850 created\n");
}

Header850::Header850(Printer* p,PrintContext* pc)
    : Header895(p,pc)
{ }

Header* DJ850::SelectHeader(PrintContext* pc)
{
    return new Header850(this,pc);
}

DRIVER_ERROR Header850::StartSend()
// common items gathered for code savings
{
    DRIVER_ERROR err;

    err = thePrinter->Send((const BYTE*)Reset,sizeof(Reset));
    ERRCHECK;
#if 0
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
#endif
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

DRIVER_ERROR DJ850::VerifyPenInfo()
{

    DRIVER_ERROR err = NO_ERROR;

    if(IOMode.bDevID == FALSE)
    {
        return err;
    }

    err = ParsePenInfo(ePen);

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

DRIVER_ERROR DJ850::ParsePenInfo(PEN_TYPE& ePen, BOOL QueryPrinter)
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

    // Check for (M)onet color pen, note (X)Undefined, (A)Missing.
    if(str[i] == 'M')
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


Compressor* DJ850::CreateCompressor(unsigned int RasterSize)
{
    DBG1("Compression = Mode2\n");
    return new Mode2(pSS,RasterSize);
}

BOOL DJ850::UseGUIMode(PrintMode* pPrintMode)
{
    return (!pPrintMode->bFontCapable);
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

DISPLAY_STATUS DJ850::ParseError(BYTE status_reg)
{
    DBG1("DJ850: parsing error info\n");

    DRIVER_ERROR err = NO_ERROR;
    BYTE DevIDBuffer[DevIDBuffSize];

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

#endif  // defined APDK_DJ850
