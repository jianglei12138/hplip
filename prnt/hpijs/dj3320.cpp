/*****************************************************************************\
  dj3320.cpp : Implimentation for the DJ3320 class

  Copyright (c) 2001 - 2015, HP Co.
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
\***************************************************************************/

#ifdef APDK_DJ3320

#include "header.h"
#include "io_defs.h"
#include "dj3320.h"
#include "resources.h"
#include "printerproxy.h"

APDK_BEGIN_NAMESPACE
extern BYTE* GetHT3x3_4();
extern BYTE* GetHT6x6_4_970();
APDK_END_NAMESPACE

APDK_BEGIN_NAMESPACE

extern uint32_t ulMapDJ3320_K_3x3x1[9 * 9 * 9];
extern uint32_t ulMapDJ3320_K_6x6x1[9 * 9 * 9];
extern uint32_t ulMapDJ3320_CMY_3x3x1[9 * 9 * 9];
extern uint32_t ulMapDJ3320_CMY_6x6x1[9 * 9 * 9];
extern uint32_t ulMapDJ3320_KCMY_3x3x1[9 * 9 * 9];
extern uint32_t ulMapDJ3320_KCMY_6x6x1[9 * 9 * 9];
extern uint32_t ulMapDJ970_Gossimer_Normal_KCMY[ 9 * 9 * 9 ];

extern uint32_t ulMapDJ3600_KCMY_6x6x1[9 * 9 * 9];
extern uint32_t ulMapDJ3600_ClMlxx_6x6x1[9 * 9 * 9];
extern uint32_t ulMapDJ3600_KCMY_6x6x2[9 * 9 * 9];
extern uint32_t ulMapDJ3600_ClMlxx_6x6x2[9 * 9 * 9];
extern uint32_t ulMapDJ3600_KCMY_3x3x1[9 * 9 * 9];
extern uint32_t ulMapDJ3600_ClMlxx_3x3x1[9 * 9 * 9];

extern void AsciiHexToBinary(BYTE* dest, char* src, int count);

#define NULL 0
//#define DBG1(str)
//#define DBG2(str, i) {}
//#define DBG3(str, i, j) {}

DJ3320::DJ3320 (SystemServices* pSS, BOOL proto)
: Printer(pSS,NUM_DJ6XX_FONTS,proto), m_dsCurrentStatus(DISPLAY_PRINTING)
{

    pLDLEncap = NULL;
    m_iBytesPerSwing = 2;
    m_iLdlVersion = 1;
    m_iColorPenResolution = 300;
    m_iBlackPenResolution = 1200;
    m_iNumBlackNozzles    = 400;

    if (IOMode.bDevID)
    {
        bCheckForCancelButton = TRUE;
        constructor_error = VerifyPenInfo ();
        CERRCHECK;
//        pSS->GetVertAlignFromDevice();
    }
    else
        ePen = BOTH_PENS;    // matches default mode
    CMYMap = ulMapDJ3320_CMY_3x3x1;
    InitPrintModes ();

    if (pSendBuffer)
    {
        pSS->FreeMem (pSendBuffer);
    }
    pSendBuffer = pSS->AllocMem (iBuffSize);
    CNEWCHECK (pSendBuffer);
}

void    DJ3320::InitPrintModes ()
{
    if (ePen == BLACK_PEN || ePen == MDL_PEN)
    {
        pMode[GRAYMODE_INDEX]    = new DJ3320KDraftMode ();
        pMode[DEFAULTMODE_INDEX] = new DJ3320GrayMode (ePen);
        ModeCount = 2;
    }
    else if (ePen == BOTH_PENS)
    {
        pMode[GRAYMODE_INDEX]       = new DJ3320GrayMode (ePen);
        pMode[DEFAULTMODE_INDEX]    = new DJ3320NormalMode (ePen);
        pMode[SPECIALMODE_INDEX]    = new DJ3320PhotoMode ();
        pMode[SPECIALMODE_INDEX+1]  = new DJ3320KDraftMode ();
        pMode[SPECIALMODE_INDEX+2]  = new DJ3320DraftMode (ePen);
        ModeCount = 5;
    }
	else if (ePen == MDL_BOTH)
	{
		pMode[GRAYMODE_INDEX]       = new DJ3320GrayMode (ePen);
        pMode[DEFAULTMODE_INDEX]    = new DJ3600MDLNormalMode ();
        pMode[SPECIALMODE_INDEX]    = new DJ3320KDraftMode ();
		pMode[SPECIALMODE_INDEX+1]  = new DJ3600MDLDraftMode ();
		pMode[SPECIALMODE_INDEX+2]  = new DJ3600MDLPhotoMode ();
        ModeCount = 5;
	}
    else
    {
        pMode[DEFAULTMODE_INDEX]    = new DJ3320NormalMode (ePen);
        pMode[SPECIALMODE_INDEX]    = new DJ3320PhotoMode ();
        pMode[GRAYMODE_INDEX]       = new DJ3320DraftMode (ePen);
        ModeCount = 3;
    }
}

DRIVER_ERROR DJ3320::SetPens (PEN_TYPE eNewPen)
{
    if (eNewPen == ePen)
    {
        return NO_ERROR;
    }
    ASSERT (eNewPen <= MAX_PEN_TYPE);
    if (eNewPen > MAX_PEN_TYPE)
    {
        return UNSUPPORTED_PEN;
    }

    for (int i = 0; i < (int) ModeCount; i++)
    {
        if (pMode[i])
        {
            delete pMode[i];
            pMode[i] = NULL;
        }
    }
    ePen = eNewPen;
    InitPrintModes ();
    AdjustResolution ();

    return NO_ERROR;
} //SetPens

DJ3320::~DJ3320 ()
{
   if (ePen == COLOR_PEN && pMode[GRAYMODE_INDEX])
   {
        delete pMode[GRAYMODE_INDEX];
        pMode[GRAYMODE_INDEX] = NULL;
    }
    if (pLDLEncap)
        delete pLDLEncap;
    if (pSendBuffer)
        pSS->FreeMem ((BYTE *) pSendBuffer);
    pSendBuffer = NULL;
}

DJ3320GrayMode::DJ3320GrayMode (PEN_TYPE ePen) : PrintMode (ulMapDJ3320_K_6x6x1)
{
	if (ePen == MDL_BOTH)
	{
		cmap.ulMap2 = ulMapDJ3600_ClMlxx_6x6x1;
	}

    ColorDepth[K] = 1;
    dyeCount = 1;
    pmColor = GREY_K;
    CompatiblePens[1] = BLACK_PEN;
	CompatiblePens[2] = MDL_BOTH;
	CompatiblePens[3] = MDL_PEN;

    ResolutionX[0] = 600;
    ResolutionY[0] = 600;

    BaseResX = 600;
    BaseResY = 600;

    MixedRes = FALSE;
    bFontCapable = FALSE;
    Config.bCompress = FALSE;
}

DJ3320KDraftMode::DJ3320KDraftMode () : GrayMode (ulMapDJ3320_K_3x3x1)
{
    bFontCapable = FALSE;
    Config.bCompress = FALSE;
    theQuality = qualityDraft;
    pmQuality = QUALITY_DRAFT;
	CompatiblePens[2] = MDL_BOTH;
	CompatiblePens[3] = MDL_PEN;
}

DJ3320DraftMode::DJ3320DraftMode (PEN_TYPE ePen)
: PrintMode (ulMapDJ3320_KCMY_3x3x1)
{

    if (ePen == COLOR_PEN)
    {
        CompatiblePens[1] = ePen;
        cmap.ulMap1 = ulMapDJ3320_CMY_3x3x1;
        dyeCount = 3;
    }

    for (int i = 0; i < 4; i++)
    {
        ColorDepth[i] = 1;

        ResolutionX[i] = 300;
        ResolutionY[i] = 300;
    }
    MixedRes = FALSE;
    BaseResX = 300;
    BaseResY = 300;
    bFontCapable = FALSE;
    pmQuality = QUALITY_DRAFT;
    Config.bCompress = FALSE;

//    strcpy(ModeName, "Draft");
}

DJ3320NormalMode::DJ3320NormalMode (PEN_TYPE ePen)
: PrintMode (ulMapDJ3320_KCMY_6x6x1)
{

    if (ePen == COLOR_PEN)
    {
        CompatiblePens[1] = ePen;
        cmap.ulMap1 = ulMapDJ3320_CMY_6x6x1;
        dyeCount = 3;
    }

    for (int i = 0; i < 4; i++)
    {
        ColorDepth[i] = 1;

        ResolutionX[i] = 600;
        ResolutionY[i] = 600;
    }

    BaseResX = 600;
    BaseResY = 600;
    MixedRes = FALSE;

    bFontCapable = FALSE;
    Config.bCompress = FALSE;

//    strcpy(ModeName, "Normal");
}

DJ3320PhotoMode::DJ3320PhotoMode ()
: PrintMode (ulMapDJ970_Gossimer_Normal_KCMY)
{

    for (int i = 0; i < 4; i++)
    {
        ColorDepth[i] = 2;

        ResolutionX[i] = 600;
        ResolutionY[i] = 600;
    }
    ColorDepth[0] = 1;
    CompatiblePens[1] = COLOR_PEN;

    BaseResX = 600;
    BaseResY = 600;
    MixedRes = FALSE;

    ColorFEDTable = GetHT6x6_4_970 ();

    bFontCapable = FALSE;

    pmQuality = QUALITY_BEST;
    pmMediaType = MEDIA_PHOTO;

    medium = mediaGlossy;
    theQuality = qualityPresentation;

    Config.bCompress = FALSE;
}


//
// Plain Normal Print Mode for Photo and Color Pen
//
DJ3600MDLNormalMode::DJ3600MDLNormalMode()
: PrintMode( ulMapDJ3600_KCMY_6x6x1, ulMapDJ3600_ClMlxx_6x6x1 )
{
	dyeCount=6;
	CompatiblePens[0] = MDL_BOTH;

    for (int i = 0; i < 6; i++)
    {
        ColorDepth[i] = 1;

        ResolutionX[i] = 600;
        ResolutionY[i] = 600;
    }

    BaseResX = 600;
    BaseResY = 600;
    MixedRes = FALSE;

    bFontCapable = FALSE;
    Config.bCompress = FALSE;
}

//
// Photo Best Print Mode for Photo and Color Pen
//
DJ3600MDLPhotoMode::DJ3600MDLPhotoMode()
: PrintMode( ulMapDJ3600_KCMY_6x6x2, ulMapDJ3600_ClMlxx_6x6x2 )
{
	dyeCount=6;
	CompatiblePens[0] = MDL_BOTH;

    for (int i = 0; i < 6; i++)
    {
        ColorDepth[i] = 2;

        ResolutionX[i] = 600;
        ResolutionY[i] = 600;
    }

    BaseResX = 600;
    BaseResY = 600;
    MixedRes = FALSE;


    ColorFEDTable = (BYTE*) HT600x6004level3600_open;

    bFontCapable = FALSE;

    pmQuality = QUALITY_BEST;
    pmMediaType = MEDIA_PHOTO;

    medium = mediaGlossy;
    theQuality = qualityPresentation;
    Config.bCompress = FALSE;
}

//
// Draft Mode for Photo and Color Pen
//
DJ3600MDLDraftMode::DJ3600MDLDraftMode()
: PrintMode( ulMapDJ3600_KCMY_3x3x1, ulMapDJ3600_ClMlxx_3x3x1 )
{
	dyeCount=6;
	CompatiblePens[0] = MDL_BOTH;

    for (int i = 0; i < 6; i++)
    {
        ColorDepth[i] = 1;

        ResolutionX[i] = 300;
        ResolutionY[i] = 300;
    }

    BaseResX = 300;
    BaseResY = 300;
    MixedRes = FALSE;

    bFontCapable = FALSE;
	pmQuality = QUALITY_DRAFT;
    Config.bCompress = FALSE;
}

DRIVER_ERROR DJ3320::Encapsulate (const RASTERDATA *pRasterData, BOOL bLastPlane)
{
    return pLDLEncap->Encapsulate (pRasterData->rasterdata[COLORTYPE_COLOR], pRasterData->rastersize[COLORTYPE_COLOR], bLastPlane);
}

Header* DJ3320::SelectHeader(PrintContext* pc)
{
    pLDLEncap = new LDLEncap (this, pSS, pc);
    if (pLDLEncap)
        pLDLEncap->AllocateSwathBuffer ((pc->OutputPixelsPerRow ()) / 8 + 2);
    if (pLDLEncap->constructor_error != NO_ERROR)
        return NULL;

    return new Header3320 (this,pc);
}

/*
 *    Author: Don Castrapel
 */

DISPLAY_STATUS DJ3320::ParseError (BYTE byStatusReg)
{
    DRIVER_ERROR err = NO_ERROR;
    BYTE byDevIDBuffer[DevIDBuffSize];
    char *pcStr = NULL;
    BYTE byStatus1, byStatus2;

    memset(byDevIDBuffer, 0, sizeof(byDevIDBuffer));
    byStatus1 = byStatus2 = 0;

    if (IOMode.bDevID)
    {
        // If a bi-di cable was plugged in and everything was OK, let's see if it's still
        // plugged in and everything is OK
        err = pSS->GetDeviceID (byDevIDBuffer, DevIDBuffSize, TRUE);
        if (err)
        {
            // job was bi-di but now something's messed up, probably cable unplugged
            m_dsCurrentStatus = DISPLAY_COMM_PROBLEM;
            return DISPLAY_COMM_PROBLEM;
        }
    }

    if (IOMode.bStatus)
    {
        if(pLDLEncap->bNewStatus)
        {
            pLDLEncap->bNewStatus = FALSE;

            // First 10 bytes of m_pbyReadBuff are packet header.  Status query from printer has $S:
            if ((pcStr = (char *) strstr((const char*)pLDLEncap->byStatusBuff + 10, "$S:")) == NULL)
            {
                m_dsCurrentStatus = DISPLAY_COMM_PROBLEM;
                return DISPLAY_COMM_PROBLEM;
            }

            // Point to first byte of Feature State.  Skip 3 bytes for "$S:", 2 for version
            pcStr += 5;
            byStatus1 = *pcStr;

            if (byStatus1 == '9')
            {
                iTotal_SLOW_POLL_Count = 0;
                m_dsCurrentStatus = DISPLAY_TOP_COVER_OPEN;
                return DISPLAY_TOP_COVER_OPEN;
            }

            // Point to Printer State.  Skip 14-byte Feature State
            pcStr += 14;
            byStatus1 = *pcStr++;
            byStatus2 = *pcStr++;

            // In any of the cases where we know what's wrong, reset the slow poll count, which we're
            // using as a "we know what's going on" variable, to 0 since we do know what's going on
            if ((byStatus1 == '0') && (byStatus2 == '5'))
            {
                // 05 = CNCL state
                iTotal_SLOW_POLL_Count = 0;
                m_dsCurrentStatus = DISPLAY_PRINTING_CANCELED;
                return DISPLAY_PRINTING_CANCELED;
            }
            if ((byStatus1 == '0') && (byStatus2 == '9'))
            {
                // 09 = OOP state
                iTotal_SLOW_POLL_Count = 0;
                m_dsCurrentStatus = DISPLAY_OUT_OF_PAPER_NEED_CONTINUE;
                return DISPLAY_OUT_OF_PAPER_NEED_CONTINUE;
            }
            if ((byStatus1 == '0') && (byStatus2 == 'E'))
            {
                // 0E = Paper jam
                iTotal_SLOW_POLL_Count = 0;
                m_dsCurrentStatus = DISPLAY_ERROR_TRAP;
                return DISPLAY_ERROR_TRAP;
            }
            if ((byStatus1 == '0') && (byStatus2 == 'F'))
            {
                // 0F = Carriage stall
                iTotal_SLOW_POLL_Count = 0;
                m_dsCurrentStatus = DISPLAY_ERROR_TRAP;
                return DISPLAY_ERROR_TRAP;
            }
            if ((byStatus1 == '1') && (byStatus2 == '0'))
            {
                // 10 = Paper stall
                iTotal_SLOW_POLL_Count = 0;
                m_dsCurrentStatus = DISPLAY_ERROR_TRAP;
                return DISPLAY_ERROR_TRAP;
            }

            // No problem detectable from status string.  Set default condition
            m_dsCurrentStatus = DISPLAY_PRINTING;
        }

        // VerifyPenInfo will handle prompting the user if this is a problem
        err = VerifyPenInfo();
        if(err)
            // VerifyPenInfo returned an error, which can only happen when ToDevice
            // or GetDeviceID returns an error. Either way, it's BAD_DEVICE_ID or
            // IO_ERROR, both unrecoverable.  This is probably due to the printer
            // being turned off during printing
            return DISPLAY_COMM_PROBLEM;
    }

    // Don't know what the problem is.  Increment wait count.  i_Total_SLOW_POLL_Count
    // really has no meaning for the DJ3320, but since it's a printer class variable
    // and it's not used for the DJ3320 we'll use it here rather than create another
    // printer class variable
    iTotal_SLOW_POLL_Count++;

    // If we've exceeded our wait time and we still don't know what's wrong, return
    // a communication problem
    if(iTotal_SLOW_POLL_Count >= ERROR_WAIT)
        return DISPLAY_BUSY;
    else
        return m_dsCurrentStatus;
}

/*
 *    Author: Don Castrapel
 */

DRIVER_ERROR DJ3320::Send (const BYTE* pWriteBuff, DWORD dwWriteCount)
{
    DRIVER_ERROR    err = NO_ERROR;
    DISPLAY_STATUS  eDisplayStatus = DISPLAY_PRINTING;
    DWORD           dwResidual = 0;
    DWORD           dwPrevResidual = 0;
    const BYTE      *pWritePos = NULL;
    BYTE            byPacketType = 0;
    BYTE            byCommandNumber = 0;
    BYTE            byCommandNumberOriginal = 0;
    BYTE            byCreditWaitCount = 0;
    BYTE            byCreditWaitCountOriginal = 0;
    BYTE            byIOWaitCount = 0;
    BOOL            bUpdateState = FALSE;
    BOOL            bCreditForCommand = FALSE;
    BOOL            bFlush = FALSE;
    BOOL            bOriginalRequest = TRUE;

    // DJ3400 doesn't use a staus reg, but we need one for the call to ParseError
    BYTE            byStatusReg = 0;

    // Buffering variables
    DWORD           BytesToWrite = dwWriteCount;
    DWORD           BytesToWriteOriginal = dwWriteCount;
    const BYTE      *pWriteBuffOriginal = pWriteBuff;
    const BYTE      *pBuffer = pWriteBuff;
    DWORD           dwWriteCountOriginal = dwWriteCount;
    DWORD           dwSendSize = dwWriteCount;

    // Retry, query, and cancel variables
    BOOL            bPrinterCancelButton = FALSE;
    BOOL            bCanceling = FALSE;
    BOOL            bCanceled = FALSE;

////////////////////////////////////////////////////////////////
#ifdef NULL_IO
    // test imaging speed independent of printer I/O, will not
    // send any data to the device
    return NO_ERROR;
#endif
////////////////////////////////////////////////////////////////

    if (!IOMode.bDevID)
    {
        return pSS->ToDevice (pWriteBuff, &dwSendSize);
    }

    if (ErrorTerminationState)
    {
        // Don't try any more I/O if we previously terminated in an error state
        return JOB_CANCELED;
    }

    // If EndJob is TRUE we don't want to return.  The Job destructor is the only place that sets this
    // boolean and we have to flush the buffer if EndJob is TRUE.
    if (!EndJob)
    {
        if (dwWriteCount == 0)
            // Don't bother processing an empty Send call
            return NO_ERROR;

        // Get Packet Type.  If Packet Type is a command, buffer it if buffering is turned on.  If it's
        // a different Packet Type, send it directly to the printer
        byPacketType = pWriteBuff[PACKET_TYPE_BYTE];

        // Get Command Number if packet is a command.  If it is then we need to check for credit
        // before we send the command
        if(!byPacketType)
        {
            byCommandNumber = pWriteBuff[COMMAND_NUMBER_BYTE];
            byCommandNumberOriginal = pWriteBuff[COMMAND_NUMBER_BYTE];
        }
    }
    else
    {
        // Just flush whatever is in the buffer
        bFlush = TRUE;
    }

    do
    {
        // If it's a command, check to see if we have credit for it.  If it's a special packet type,
        // we'll just send it directly to the printer
        if(!bCreditForCommand)
        {
            // Could be first time through do loop or could have not had credit and had to check.  We'll
            // always get here since we don't change bCreditForCommand until here.
            if (!byPacketType)
            {
                // Check to see if we have credit for this command
                if (pLDLEncap->piCreditCount[byCommandNumber] > 0)
                {
                    pLDLEncap->piCreditCount[byCommandNumber]--;
                    bCreditForCommand = TRUE;
                    byCreditWaitCount = 0;
                }
                else
                {
                    byCreditWaitCount++;

                    bUpdateState = pLDLEncap->UpdateState (FALSE);
                    if(bUpdateState)
                    {
                        pSS->BusyWait(0);
                        // Rechecking here will save us a trip through the do loop
                        if (pLDLEncap->piCreditCount[byCommandNumber] > 0)
                        {
                            pLDLEncap->piCreditCount[byCommandNumber]--;
                            bCreditForCommand = TRUE;
                            byCreditWaitCount = 0;
                        }
                    }
                }
            }
            else
            {
                // Special packet types get a free pass
                bCreditForCommand = TRUE;
            }
        } // if(!bCreditForCommand)

        // If we don't have credit for the current command we don't want to put it in the
        // buffer.  If, however, we have exceeded our credit wait limit and we need to check
        // for an error, we have to flush what's in our buffer
        if (bCreditForCommand || bFlush)
        {
            if (bCreditForCommand)
            {
                // We should bypass the buffering for a large Send, but don't lose what may
                // already be buffered
                if ((BytesToWrite >= (DWORD) iBuffSize) && (iCurrBuffSize == 0))
                {
                    pBuffer = pWriteBuff + (dwWriteCount - BytesToWrite);
                    dwSendSize = BytesToWrite;
                    BytesToWrite = 0;  // This is checked for at the end of the outer loop
                }
                else // We will buffer this data
                {
                    // If it'll fit then just copy everything to the buffer
                    if (BytesToWrite <= (DWORD) iBuffSize - iCurrBuffSize)
                    {
                        memcpy ((void*) (pSendBuffer + iCurrBuffSize),
                                (void*) (pWriteBuff + (dwWriteCount - BytesToWrite)),
                                BytesToWrite);
                        iCurrBuffSize += BytesToWrite;
                        BytesToWrite = 0;
                    }
                    else // Copy what we can into the buffer, we'll get the rest later
                    {
                        memcpy ((void*) (pSendBuffer + iCurrBuffSize),
                                (void*) (pWriteBuff + (dwWriteCount - BytesToWrite)),
                                iBuffSize - iCurrBuffSize);
                        BytesToWrite -= (iBuffSize - iCurrBuffSize);
                        iCurrBuffSize = iBuffSize;
                    }
                }

                // If this wasn't the original request, like a query, continue, prepare to
                // cancel, or cancel command, flush the buffer immediately
                if(!bOriginalRequest)
                {
                    bFlush = TRUE;
                }
            } // if (bCreditForCommand)

            // If the buffer is now full (ready-to-send) or if we're at the end of the job, or
            // if the Packet Type is not a command then send what we have in the buffer.
            // otherwise just break (the buffer isn't ready to send)
            if ((EndJob == FALSE) && (iCurrBuffSize != iBuffSize)  && (!byPacketType) && (!bFlush) )
            {
                // We're not ready to send yet.  Break out of do loop
                break;
            }
            else // Send this buffered data
            {
                if (bFlush)
                {
                    bFlush = FALSE;
                }
                pBuffer = pSendBuffer;
                dwSendSize = iCurrBuffSize;
            }

            // Initialize our 'residual' to the full send size
            dwResidual = dwSendSize;

            // Code to check to see if user has pressed cancel button.  DJ3320 front panel button, if
            // pressed during normal printing, will initiate a cancel in the printer.  This will cause the
            // printer to just throw away data but not notify the host.  We have to do that here if we
            // detect a cancel
            if(bCheckForCancelButton && (ulBytesSentSinceCancelCheck >= CANCEL_BUTTON_CHECK_THRESHOLD))
            {
                ulBytesSentSinceCancelCheck = 0;

                if(ParseError(byStatusReg) == DISPLAY_PRINTING_CANCELED)
                    bPrinterCancelButton = TRUE;
            }

            // If we have nothing to send, we need to bail to avoid spurious dialogs
            // at the end of the ::send function.  I'd prefer a solution where we don't
            // bail from a while loop but in practice this shouldn't have any ill effects.
            if (dwResidual <= 0)
            {
                return NO_ERROR;
            }

            // While still data to send in this request
            while (dwResidual > 0)
            {
                // WritePort overwrites request count, need to save
                dwPrevResidual = dwResidual;

                pWritePos = (const BYTE *) &(pBuffer[dwSendSize - dwResidual]);
                err = pSS->ToDevice (pWritePos, &dwResidual);

                if(err)
                {
                    ErrorTerminationState = TRUE;
                    return err;
                }

                // No more data to send this time
                if (dwResidual == 0)
                {
                    // For USB printer class drivers that have buffering we need to flush the buffer.
                    // If we are sending a special packet type and can't wait for a full buffer we have
                    // to flush
                    err = pSS->FlushIO ();
                    if (err)
                    {
                        ErrorTerminationState = TRUE;
                        return err;
                    }
                    // We successfully sent the entire non-original request, so reset to original
                    // request
                    if(!bOriginalRequest && !bCanceled)
                    {
                        pWriteBuff = pWriteBuffOriginal;
                        dwWriteCount = dwWriteCountOriginal;
                        BytesToWrite = BytesToWriteOriginal;
                        byCommandNumber = byCommandNumberOriginal;
                        bOriginalRequest = TRUE;
                        bCreditForCommand = FALSE;
                        byCreditWaitCount = byCreditWaitCountOriginal;
                        // We have to give the printer some time to update its status and send us a
                        // new status packet
                        pSS->BusyWait(500);
                    }
                    else
                    {
                        if(bOriginalRequest)
                        {
                            byIOWaitCount = 0;
                        }

                        iTotal_SLOW_POLL_Count = 0;
                    }

                    break; // Out of while loop
                }
                else
                {
                    if (dwPrevResidual == dwResidual)
                    {
                        // The I/O didn't take any data, increment count
                        byIOWaitCount++;
                    }
                    else
                    {
                        // The I/O took some data, although not the full request
                        if(bOriginalRequest)
                            byIOWaitCount = 0;
                    }
                }

                // If I/O hasn't finished after our timeout limit, we have to bail.
                if (byIOWaitCount >= IO_WAIT)
                {
                    ErrorTerminationState = TRUE;
                    pSS->DisplayPrinterStatus (DISPLAY_COMM_PROBLEM);
                    return IO_ERROR;
                }

                // Check for user cancel each time through loop
                if (pSS->BusyWait ((DWORD)0) == JOB_CANCELED)
                {
                    pSS->DisplayPrinterStatus (DISPLAY_PRINTING_CANCELED);
                }

            } // while (residual > 0)

            iCurrBuffSize = 0;

        } // if(bCreditForCommand || bFlush)
        else
        {
            // If we can't get credit and we've exceeded our wait limit, check for an error
            if (byCreditWaitCount >= CREDIT_WAIT)
            {
                if(!bOriginalRequest)
                {
                    // Something is wrong but we don't know what it is and we can't get credit
                    // to Query, Continue, Prepare to Cancel, or Cancel, so we have to bail
                    ErrorTerminationState = TRUE;
                    return JOB_CANCELED;
                }
                // Flush our internal buffer so that we can send command such as query or continue
                if (iCurrBuffSize)
                {
                    bFlush = TRUE;
                }
                else
                {
                    // See if we can find out what's wrong
                    eDisplayStatus = ParseError(byStatusReg);

                    // For recoverable cases such as out of paper or top cover open, we just want to
                    // display the error and break.  For non-recoverable cases we'll wait for the
                    // user to cancel the job and return.

                    // If the user terminated in an error state we have to send the CancelJob,
                    // unless the error state is a condition that would prevent a paper eject, such
                    // as paper jam or error trap.  In those cases we'll just return, since the
                    // user has to power cycle the printer before he can continue
                    switch (eDisplayStatus)
                    {
                        case DISPLAY_PRINTING_CANCELED:
                            // User canceled in an error condition, break from here and let check
                            // for cancel at end of do loop send Cancel Job to printer
                            pSS->DisplayPrinterStatus (eDisplayStatus);
                            ErrorTerminationState = TRUE;

                            break;

                        case DISPLAY_ERROR_TRAP:
                        case DISPLAY_COMM_PROBLEM:
                            // These are unrecoverable cases.  Don't let any more of this job be sent
                            // to the printer.  We can't even eject the page at the end of the job

                            ErrorTerminationState = TRUE;
                            pSS->DisplayPrinterStatus (eDisplayStatus);

                            // Wait for user to cancel the job, otherwise they might miss the
                            // error message
                            while (pSS->BusyWait ((DWORD) 500) != JOB_CANCELED)
                            {
                                // nothing....
                                ;
                            }

                            return IO_ERROR;

                        case DISPLAY_TOP_COVER_OPEN:
                            pSS->DisplayPrinterStatus(DISPLAY_TOP_COVER_OPEN);

                            err = NO_ERROR;

                            // Wait for top cover to close or user to cancel
                            while(eDisplayStatus == DISPLAY_TOP_COVER_OPEN && !err)
                            {
                                err = pSS->BusyWait((DWORD)500);
                                if(err == JOB_CANCELED)
                                {
                                    ErrorTerminationState = TRUE;
                                }

                                if(!err)
                                {
                                    bUpdateState = pLDLEncap->UpdateState(FALSE);
                                    if(bUpdateState)
                                    {
                                        eDisplayStatus = ParseError(byStatusReg);

                                        // Need to check for cancel here, because we could miss it if the
                                        // user presses cancel button then lifts lid or something
                                        if(eDisplayStatus == DISPLAY_PRINTING_CANCELED)
                                        {
                                            ErrorTerminationState = TRUE;
                                            bPrinterCancelButton = TRUE;
                                        }
                                    }
                                }
                            } // while(eDisplayStatus == DISPLAY_TOP_COVER_OPEN && !err)

                            if(!err && !bPrinterCancelButton)
                            {
                                pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

                                // Give the printer some time to come back online
                                if(pSS->BusyWait((DWORD)1000) == JOB_CANCELED)
                                {
                                    ErrorTerminationState = TRUE;
                                }
                            }

                            break;

                        case DISPLAY_OUT_OF_PAPER_NEED_CONTINUE:
                            pSS->DisplayPrinterStatus (DISPLAY_OUT_OF_PAPER_NEED_CONTINUE);

                            err = NO_ERROR;

                            // Wait for user to add more paper and press resume button on printer or
                            // select CONTINUE button from host's error dialog
                            while(eDisplayStatus == DISPLAY_OUT_OF_PAPER_NEED_CONTINUE && !err)
                            {
                                err = pSS->BusyWait((DWORD)500);

                                if (err == JOB_CANCELED)
                                {
                                    ErrorTerminationState = TRUE;
                                }
                                else if(err == CONTINUE_FROM_BLOCK)
                                {
                                    // Setup CONTINUE command
                                    pWriteBuff = byContinue;
                                    dwWriteCount = sizeof(byContinue);
                                    BytesToWrite = sizeof(byContinue);
                                    byCommandNumber = COMMAND_CONTINUE;
                                    bOriginalRequest = FALSE;
                                    bCreditForCommand = FALSE;
                                    byCreditWaitCountOriginal = byCreditWaitCount;
                                    byCreditWaitCount = 0;
                                }
                                else
                                {
                                    bUpdateState = pLDLEncap->UpdateState(FALSE);
                                    if(bUpdateState)
                                    {
                                        eDisplayStatus = ParseError(byStatusReg);

                                        // Need to check for cancel here, because we could miss it if the
                                        // user presses cancel button then lifts lid or something
                                        if(eDisplayStatus == DISPLAY_PRINTING_CANCELED)
                                        {
                                            ErrorTerminationState = TRUE;
                                            bPrinterCancelButton = TRUE;
                                        }
                                    }
                                }
                            } // while(eDisplayStatus == DISPLAY_OUT_OF_PAPER_NEED_CONTINUE && !err)

                            if(!err && !bPrinterCancelButton)
                            {
                                pSS->DisplayPrinterStatus(DISPLAY_PRINTING);
                            }

                            break;

                        case DISPLAY_BUSY:
                            pSS->DisplayPrinterStatus(DISPLAY_BUSY);

                            if (pSS->BusyWait ((DWORD) 5000) == JOB_CANCELED)
                            {
                                ErrorTerminationState = TRUE;
                            }

                            break;

                        // Other cases need no special handling, display the error and try to continue
                        default:
                            pSS->DisplayPrinterStatus (eDisplayStatus);

                            if (pSS->BusyWait ((DWORD) 500) == JOB_CANCELED)
                            {
                                ErrorTerminationState = TRUE;
                            }

                            break;
                    } // switch(eDisplayStatus)
                } // else(iCurrBuffSize)
            } // if(byCreditWaitCount >= CREDIT_WAIT)
        } // else(bCreditForCommand || bFlush)

        if (pSS->BusyWait ((DWORD)0) == JOB_CANCELED || bPrinterCancelButton)
        {
            pSS->DisplayPrinterStatus(DISPLAY_PRINTING_CANCELED);

            // If there is anything waiting in the buffer, send it.  I/O may have had a full buffer and
            // may not have been able to buffer the full request for which we had credit, so if there
            // are still BytesToWrite and we have credit for the current command, we have to buffer
            // those and send them if we didn't terminate in an error condition.

            // Another possibility is that we don't have credit for the current command.  In that case
            // we don't want to wait for BytesToWrite to be 0 or we'll be in a deadlock.  We'll never
            // get credit for the command and thus BytesToWrite will never be 0.

            // We can only terminate in an error condition if we didn't have credit for the original
            // command, so in that case we only have to flush what's in the buffer and not worry about
            // the remaining BytesToWrite.

            // After we check for these conditions we can send the Prepare to Cancel and Cancel
            // Job commands
            if( (iCurrBuffSize || (BytesToWrite && bCreditForCommand && !ErrorTerminationState) ) &&
               !(bCanceling || bCanceled) )
            {
                bFlush = TRUE;
            }
            else if(!bCanceling)
            {
                pWriteBuff = byPrepareToCancel;
                dwWriteCount = sizeof(byPrepareToCancel);
                BytesToWrite = sizeof(byPrepareToCancel);
                byCommandNumber = COMMAND_PREPARE_TO_CANCEL;
                bOriginalRequest = FALSE;
                bCreditForCommand = FALSE;
                byCreditWaitCount = 0;
                bCanceling = TRUE;
            }
            else if(!bCanceled)
            {
                // pWriteBuff will only be equal to byPrepareToCancel when we've setup the Prepare to
                // Cancel command but haven't yet sent it to the printer
                if(pWriteBuff != byPrepareToCancel)
                {
                    pWriteBuff = pLDLEncap->pbyCancel;
                    dwWriteCount = sizeof(pLDLEncap->pbyCancel);
                    BytesToWrite = sizeof(pLDLEncap->pbyCancel);
                    byCommandNumber = COMMAND_CANCEL;
                    bOriginalRequest = FALSE;
                    bCreditForCommand = FALSE;
                    byCreditWaitCount = 0;
                    bCanceled = TRUE;
                }
            }
        }
    } while (BytesToWrite > 0);

    if (bCanceled)
    {
        // Ensure that display still says we're cancelling
        pSS->DisplayPrinterStatus(DISPLAY_PRINTING_CANCELED);
        ErrorTerminationState = TRUE;
        return JOB_CANCELED;
    }
    else
    {
        // Ensure any error message has been cleared
        pSS->DisplayPrinterStatus (DISPLAY_PRINTING);
        if (bCheckForCancelButton)
        {
            ulBytesSentSinceCancelCheck += dwWriteCount;
        }
        return NO_ERROR;
    }
}

DRIVER_ERROR DJ3320::ParsePenInfo (PEN_TYPE& ePen, BOOL QueryPrinter)
{

    char    *str;
    int     num_pens = 0;

    DRIVER_ERROR err = SetPenInfo (str, QueryPrinter);
    ERRCHECK;

    // the first byte indicates how many pens are supported
    if ((str[0] >= '0') && (str[0] <= '9'))
    {
        num_pens = str[0] - '0';
    }
    else if ((str[0] >= 'A') && (str[0] <= 'F'))
    {
        num_pens = 10 + (str[0] - 'A');
    }
    else
    {
        return BAD_DEVICE_ID;
    }

    if ((int) strlen (str) < (num_pens * 8))
    {
        return BAD_DEVICE_ID;
    }

    char    *p = str + 1;
    BYTE    penInfoBits[4];
/*
 *  Pen Type Info
 *
    Bit 31 (1 bit)

    1 if these fields describe a print head
    0 otherwise
    Bit 30 (1 bit)

    1 if these fields describe an ink supply
    0 otherwise

    Bits 29 .. 24 (6 bits) describes the pen/supply type:

    0 = none
    1 = black
    2 = CMY
    3 = KCM
    4 = Cyan
    5 = Meganta
    6 = Yellow
	7 = Cyan - low dye load 
    8 = Magenta - low dye load 
    9 = Yellow - low dye load (may never be used, but reserve space anyway) [def added Jun 3, 2002]
    10 = gGK - two shades of grey plus black; g=light grey, G=medium Grey, K=black  [added Sep 12, 02]
    11 .. 62 = reserved for future use
    63=Unknown
 */
    ePen = NO_PEN;

    for (int i = 0; i < num_pens; i++, p += 8)
    {
		AsciiHexToBinary (penInfoBits, p, 8);
        if ((penInfoBits[1] & 0xf8) == 0xf8)
        {
//          The high 5 bits in the 3rd and 4th nibble (second byte) identify the
//          installed pen. If all 5 bits are on, user has installed an incompatible pen.
            return UNSUPPORTED_PEN;
        }
        if ((penInfoBits[0] & 0x80) != 0x80)        // if Bit 31 is 0, this is not a pen
        {
            continue;
        }
        BYTE penColor = penInfoBits[0] & 0x3F;
        switch (penColor)
        {
            case 0:
            {
                break;
            }
            case 1:
                ePen = BLACK_PEN;
                break;
            case 2:
            {
                if (ePen == BLACK_PEN)
                {
                    ePen = BOTH_PENS;
                }
                else if (ePen == MDL_PEN)
                {
                    ePen = MDL_BOTH;
                }
                else
                {
                    ePen = COLOR_PEN;
                }
                break;
            }
            case 3:
                if (ePen == BLACK_PEN)
                {
                    ePen = MDL_AND_BLACK_PENS;
                }
                else if (ePen == COLOR_PEN)
                {
                    ePen = MDL_BOTH;
                }
                else if (ePen == BOTH_PENS)
                {
                    ePen = MDL_BLACK_AND_COLOR_PENS;
                }
                else
                {
                    ePen = MDL_PEN;
                }
                break;
            case 4:             // cyan pen
            case 5:             // magenta pen
            case 6:             // yellow pen
            case 7:             // low dye load cyan pen
            case 8:             // low dye load magenta pen
            case 9:             // low dye load yellow pen
                if (ePen == BLACK_PEN || ePen == BOTH_PENS)
                {
                    ePen = BOTH_PENS;
                }
                else
                {
                    ePen = COLOR_PEN;
                }
                break;
            default:
                ePen = UNKNOWN_PEN;
        }
    }

    return NO_ERROR;
}

DRIVER_ERROR DJ3320::VerifyPenInfo()
{

    DRIVER_ERROR err = NO_ERROR;

    if(IOMode.bDevID == FALSE)
        return err;

    ePen = NO_PEN;

    err = ParsePenInfo(ePen);
    ERRCHECK;

    while (ePen == NO_PEN)
    {
        err = ParsePenInfo (ePen);
        ERRCHECK;
        if (ePen == NO_PEN)
        {
            pSS->DisplayPrinterStatus (DISPLAY_NO_PENS);
            if (pSS->BusyWait (500) == JOB_CANCELED)
                return JOB_CANCELED;
        }

    }

    pSS->DisplayPrinterStatus (DISPLAY_PRINTING);

    return NO_ERROR;
}

DRIVER_ERROR DJ3320::CheckInkLevel()
{
    DRIVER_ERROR err;
    char* pStr;

    BYTE bDevIDBuff[DevIDBuffSize];

    if (!IOMode.bDevID)
    {
        return NO_ERROR;
    }

    err = pSS->GetDeviceID(bDevIDBuff, DevIDBuffSize, TRUE);
    if (err!=NO_ERROR)
    {
        return NO_ERROR;
    }

    if ( (pStr=(char *)strstr((const char*)bDevIDBuff+2,";S:")) == NULL )
    {
        return NO_ERROR;
    }

    pStr += 21;
    int     numPens = 0;
    if (*pStr > '0' && *pStr < '9')
    {
        numPens = *pStr - '0';
    }
    else if (*pStr > 'A' && *pStr < 'F')
    {
        numPens = *pStr - 'A';
    }
    else if (*pStr > 'a' && *pStr < 'f')
    {
        numPens = *pStr - 'a';
    }

    pStr++;

	BYTE    penInfoBits[4];
    BYTE    blackink = 0;
    BYTE    colorink = 0;
    BYTE    photoink = 0;
    BYTE    greyink = 0;
    for (int i = 0; i < numPens; i++, pStr += 8)
    {
        AsciiHexToBinary (penInfoBits, pStr, 8);

        if ((penInfoBits[0] & 0x80) != 0x80)        // if Bit 31 is 0, this is not a pen
        {
            continue;
        }
        int penColor = penInfoBits[0] & 0x3F;
        switch (penColor)
        {
            case 1:
                blackink =  penInfoBits[1] & 0x7;
                break;
            case 2:
                colorink =  penInfoBits[1] & 0x7;
                break;
            case 3:
                photoink =  penInfoBits[1] & 0x7;
                break;
            case 10:
                greyink =  penInfoBits[1] & 0x7;
                break;
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                colorink = penInfoBits[1] & 0x7;   // REVISIT: these are C, M, Y respectively
                break;
            default:
                break;
        }
    }
    if (blackink < 2 && colorink < 2 && photoink < 2 && greyink < 2)
    {
        return NO_ERROR;
    }
    else if (blackink > 1 && colorink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_COLOR_BLACK_PHOTO;
    }
    else if (greyink > 1 && colorink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_COLOR_GREY_PHOTO;
    }
    else if (blackink > 1 && colorink > 1)
    {
        return WARN_LOW_INK_BOTH_PENS;
    }
    else if (blackink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_BLACK_PHOTO;
    }
    else if (greyink > 1 && colorink > 1)
    {
        return WARN_LOW_INK_COLOR_GREY;
    }
    else if (greyink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_GREY_PHOTO;
    }
    else if (colorink > 1 && photoink > 1)
    {
        return WARN_LOW_INK_COLOR_PHOTO;
    }
    else if (blackink > 1)
    {
        return WARN_LOW_INK_BLACK;
    }
    else if (colorink > 1)
    {
        return WARN_LOW_INK_COLOR;
    }
    else if (photoink > 1)
    {
        return WARN_LOW_INK_PHOTO;
    }
    else if (greyink > 1)
    {
        return WARN_LOW_INK_GREY;
	}
    else if (colorink > 1)
    {
        return WARN_LOW_INK_COLOR;
    }
	else
	{
		return NO_ERROR;
	}
} //CheckInkLevel


DRIVER_ERROR DJ3320::SkipRasters (int nBlankRasters)
{
    return (pLDLEncap->SetVerticalSkip (nBlankRasters));
}

DRIVER_ERROR DJ3320::CleanPen()
{
    return pLDLEncap->CleanPen ();
}

DRIVER_ERROR DJ3320::Flush (int FlushSize)
{
    pLDLEncap->Flush ();
    return NO_ERROR;
}

Header3320::Header3320 (Printer* p,PrintContext* pc)
    : Header(p,pc)
{

}

DRIVER_ERROR Header3320::FormFeed ()
{
    return (((DJ3320 *) thePrinter)->pLDLEncap->EndPage ());
}

DRIVER_ERROR Header3320::EndJob()
{
    ((DJ3320 *) thePrinter)->pLDLEncap->EndJob ();
    return NO_ERROR;
}

DRIVER_ERROR Header3320::Send ()
{

    DJ3320  *pXBow = (DJ3320 *) thePrinter;

    return (pXBow->pLDLEncap->StartJob ());

}

DRIVER_ERROR Header3320::SendCAPy (unsigned int iAbsY)
{
    return NO_ERROR;
}

LDLEncap::LDLEncap (DJ3320 *pPrinter, SystemServices *pSys, PrintContext *pc)
{

    constructor_error = NO_ERROR;
    piCreditCount = NULL;
    m_pSys = pSys;
    pPrinterXBow = pPrinter;
    m_iXResolution = pc->EffectiveResolutionX ();
    m_iYResolution = pc->EffectiveResolutionY ();
    m_pthisPC = pc;
    m_cPrintDirection = PRNDRN_LEFTTORIGHT;
    m_SwathData = NULL;
    m_sRefCount = 6;
    m_iBlankRasters = 0;
    m_iRasterCount = 0;
    m_iVertPosn = (int) (m_pthisPC->PrintableStartY () * DEVUNITS_XBOW);
    m_iNumColors = 4;
    m_iLeftMargin = (int) (m_pthisPC->PrintableStartX () * DEVUNITS_XBOW);

    ////////////////////////////////////////////////////////////////////////////////////
    float   fXOverSpray = 0.0;
    float   fYOverSpray = 0.0;
    float   fLeftOverSpray = 0.0;
    float   fTopOverSpray  = 0.0;
	FullbleedType   fbType;
    if (m_pthisPC->bDoFullBleed &&
        pPrinterXBow->FullBleedCapable (m_pthisPC->thePaperSize,
									  &fbType,
                                      &fXOverSpray, &fYOverSpray,
                                      &fLeftOverSpray, &fTopOverSpray))
    {

		/*
		 *      To get the printer to do fullbleed printing, move the vertical postion 
		 *      to cover the overspary. Overspray is needed to take care of
		 *      skew during paper pick. These values may be mech dependent.
		 *      Currently, supported only on PhotoSmart 100, Malibu. DJ3600 supports
		 *      fullbleed printing also. The current values for overspray are
		 *      0.059 inch for top, bottom and left edges and 0.079 for right edge.
		 */
		m_iVertPosn = (int) (-fTopOverSpray * DEVUNITS_XBOW);
		m_iLeftMargin = (int) (-fLeftOverSpray * DEVUNITS_XBOW);
    }

    m_bStartPageNotSent = TRUE;

    m_iNextRaster = 0;
    m_iNextColor  = 0;

    m_iBitDepth = 1;
    m_cPlaneNumber = 0;
    m_cPrintQuality = (BYTE) QUALITY_NORMAL;
    m_cMediaType    = (BYTE) MEDIA_PLAIN;

    BYTE    cm = COLOR;
    QUALITY_MODE cqm;
    MEDIATYPE cmt;
    COLORMODE ccm;
    BOOL cdt;
    DRIVER_ERROR err = m_pthisPC->GetPrintModeSettings (cqm, cmt, ccm, cdt);
    if (err == NO_ERROR)
    {
        if (cqm == QUALITY_BEST && cmt == MEDIA_PHOTO)
            m_iBitDepth = 2;
        m_cPrintQuality = (BYTE) cqm;
        m_cMediaType    = (BYTE) cmt;
        cm = (BYTE) ccm;
    }

    if (pPrinterXBow->ePen == BLACK_PEN || pPrinterXBow->ePen == MDL_PEN || cm == GREY_K)
        m_iNumColors = 1;
    else if (pPrinterXBow->ePen == COLOR_PEN || cm == GREY_CMY)
        m_iNumColors = 3;
	else if (pPrinterXBow->ePen == BOTH_PENS && m_iBitDepth == 2)
		m_iNumColors = 3;
	else if (pPrinterXBow->ePen == MDL_BOTH)
		m_iNumColors = 6;

    m_bBidirectionalPrintingOn = TRUE; //FALSE;

    UInt16 mem_needed =   SIZEOF_LDLHDR
                        + SIZEOF_LDL_PRTSWP_CMDOPT
                        + SIZEOF_LDL_PRTSWP_OPTFLDS
                        + 6 * SIZEOF_LDL_PRTSWP_COLROPT
                        + SIZEOF_LDL_COLROPT_ACTIVECOLR
                        + SIZEOF_LDLTERM;
    m_szCmdBuf = new BYTE[mem_needed];
    CNEWCHECK (m_szCmdBuf);

    m_bLittleEndian = TRUE;
    {
        union
        {
            short   a;
            char    b[2];
        }c;
        c.a = 0x1234;
        if (c.b[0] == 0x12)
            m_bLittleEndian = FALSE;
    }

    m_szCmdBuf[0] = FRAME_SYN;
    m_szCmdBuf[1] = 0;
    m_szCmdBuf[3] = 0;
    m_szCmdBuf[4] = 0;
    m_szCmdBuf[5] = 0;
    m_szCmdBuf[6] = 0;
    m_szCmdBuf[8] = 0;

    // Pacing and status handling data
    bNewStatus = FALSE;
    memset(byStatusBuff, 0, sizeof(byStatusBuff));

    m_pbyPacketBuff = NULL;
    m_pbyPacketBuff = pSys->AllocMem (MAX_PACKET_READ_SIZE);
    CNEWCHECK (m_pbyPacketBuff);

    m_dwPacketBuffSize = MAX_PACKET_READ_SIZE;

    // Setup Sync command
    pbySync = m_pSys->AllocMem(SYNCSIZE);
    CNEWCHECK(pbySync);

    memset(pbySync, 0, SYNCSIZE);
    memcpy(pbySync, bySync, sizeof(bySync));
    memset((pbySync + sizeof(bySync)), 0, SYNC_CMD_OPT_SIZE);
    pbySync[sizeof(bySync) + SYNC_CMD_OPT_SIZE] = FRAME_SYN;
    memset((pbySync + sizeof(bySync) + SYNC_CMD_OPT_SIZE + sizeof(FRAME_SYN)), 0, LDL_MAX_IMAGE_SIZE);

/*
 *  Alignment Values.
 *  Currently, only Black to Color Vertical Alignment value is used.
 *  This value should really be obtained by running the pen alignment test.
 *  A value of 12 device units seems to be a good default.
 */
	BYTE cVertAlign = 0;
	if (pPrinterXBow->ePen == BOTH_PENS)
	{
		if (pSys->GetVerticalAlignmentValue(&cVertAlign))
		{
			m_cKtoCVertAlign = cVertAlign;
		}
		else
		{
			m_cKtoCVertAlign = 12;
		}
	}
	else if (pPrinterXBow->ePen == MDL_BOTH)
	{
		if (pSys->GetVerticalAlignmentValue(&cVertAlign))
		{
			m_cPtoCVertAlign = cVertAlign;
		}
		else
		{
			m_cPtoCVertAlign = 6;
		}
	}
	else
	{
		m_cKtoCVertAlign = 12;
		m_cPtoCVertAlign = 6;
	}
}

void LDLEncap::AllocateSwathBuffer (unsigned int RasterSize)
{
    int size = RasterSize;
    size = (size / 8 + 1) * 8;
    m_iImageWidth = size;

    constructor_error = NO_ERROR;
    m_ldlCompressData = NULL;

#ifdef  APDK_LDL_COMPRESS
    if (pPrinterXBow->m_iLdlVersion == 1)
    {
        m_ldlCompressData = new comp_ptrs_t;
    }
#endif

    if (m_iBitDepth == 2)
    {
        size *= 2;
    }

    int     iSwings = pPrinterXBow->m_iBytesPerSwing / 2;
    int     iCompressBufSize = iSwings * LDL_MAX_IMAGE_SIZE+20;    // additional space for load sweep command
    m_szCompressBuf = new BYTE[iCompressBufSize];
    CNEWCHECK (iCompressBufSize);
    memset (m_szCompressBuf, 0, iCompressBufSize);

    BYTE    *p = NULL;
    int     iSwathBuffSize;

    m_sSwathHeight = SWATH_HEIGHT;

	/*
	 *  This swath buffer cannot be greater than the number of nozzles - 400 for black
	 *  and 100 for color - we can use.
	 */

    int    iAdjHeight = (pPrinterXBow->m_iNumBlackNozzles / 32) * 8;
    if (pPrinterXBow->ePen == BLACK_PEN)
    {
        m_sSwathHeight = m_sSwathHeight * 4;
        if (m_sSwathHeight * 1200 / m_iYResolution > pPrinterXBow->m_iNumBlackNozzles)
            m_sSwathHeight = m_iYResolution / 3;
    }
    else if (m_cPrintQuality != QUALITY_DRAFT && m_iYResolution > 300 && m_iNumColors > 1 && m_iBitDepth == 1) // Collie change
    {
        m_sSwathHeight = (m_sSwathHeight / 4) * 4 * 2;
        if (m_sSwathHeight > 200)
            m_sSwathHeight = 200;
    }

    else if (m_iBitDepth == 2)
        m_sSwathHeight = iAdjHeight * 4;

    if (m_cPrintQuality == QUALITY_NORMAL)
        m_sSwathHeight = iAdjHeight * 2;

    if (m_cPrintQuality == QUALITY_DRAFT && pPrinterXBow->ePen != BLACK_PEN)
    {
        m_sSwathHeight *= iSwings;
    }

    while (m_sSwathHeight > 16)
    {
        iSwathBuffSize = m_iNumColors * sizeof (BYTE *) +
                         m_iNumColors * m_sSwathHeight * sizeof (BYTE *) +
                         size * m_iNumColors * m_sSwathHeight;
        if ((p = m_pSys->AllocMem(iSwathBuffSize)) == NULL)
        {
            m_sSwathHeight = (m_sSwathHeight / 16) * 8;
            continue;
        }
        break;
    }
    if (m_sSwathHeight < 16)
    {
        m_sSwathHeight = 16;
        iSwathBuffSize = m_iNumColors * sizeof (BYTE *) +
                         m_iNumColors * m_sSwathHeight * sizeof (BYTE *) +
                         size * m_iNumColors * m_sSwathHeight;
        p = m_pSys->AllocMem(iSwathBuffSize);
        CNEWCHECK (p);
    }

    int     i;
    m_SwathData = (BYTE ***) p;
    for (i = 0; i < m_iNumColors; i++)
        m_SwathData[i] = (BYTE **) (p + sizeof (BYTE *) * m_iNumColors + i * m_sSwathHeight * sizeof (BYTE *));

    for (i = 0; i < m_iNumColors; i++)
    {
        p = (BYTE *) m_SwathData + sizeof (BYTE *) * m_iNumColors +
                m_iNumColors * m_sSwathHeight * sizeof (BYTE *) +
                size * m_sSwathHeight * i;
        for (int j = 0; j < m_sSwathHeight; j++)
        {
            memset (p, 0, size);
            m_SwathData[i][j] = p;
            p = p + size;
        }
    }

    if (m_cPrintQuality != QUALITY_DRAFT && m_iYResolution != 300)
    {
        m_iRasterCount = (m_sSwathHeight - m_sSwathHeight / (4*m_iBitDepth)) * m_iNumColors;
        m_iVertPosn -= (((m_sSwathHeight - m_sSwathHeight / (4*m_iBitDepth)) * 600 / m_iYResolution) * DEVUNITS_XBOW / 600);
    }
    m_cPassNumber = 0;

    float   fXOverSpray = 0.0;
    float   fYOverSpray = 0.0;
    float   fLeftOverSpray = 0.0;
    float   fTopOverSpray  = 0.0;
	FullbleedType   fbType;
    if (m_pthisPC->bDoFullBleed &&
        pPrinterXBow->FullBleedCapable (m_pthisPC->thePaperSize,
									  &fbType,
                                      &fXOverSpray, &fYOverSpray,
                                      &fLeftOverSpray, &fTopOverSpray))
	{
		if (m_iVertPosn < -850) m_iVertPosn = -850;
	}
	else
	{
		if (m_iVertPosn < -600) m_iVertPosn = -600;
	}
    if (m_iBitDepth == 2)
        m_iVertPosn += 6;
}

unsigned int LDLEncap::GetSwathWidth (int iStart, int iLast, int iWidth)
{
    int k;
    int i, j;
    for (i = iWidth - 1 /*sizeof (long)*/; i > -1; i--)
    {
        for (j = iStart; j < iLast; j++)
        {
            for (k = m_iRasterCount / m_iNumColors-1; k >= 0; k--)
            {

                if (m_SwathData[j][k][i])
                {
                    return (i+1);
                }
            }
        }
    }

    return 0;
}

void LDLEncap::Flush ()
{
//  if (m_iRasterCount)
//      Process (NULL, 0);
}

DRIVER_ERROR LDLEncap::SetVerticalSkip (int nBlankRasters)
{
    DRIVER_ERROR    err = NO_ERROR;
#if 0
    if (m_iRasterCount == 0)
    {
        m_iBlankRasters += nBlankRasters;
        return err;
    }
#endif
    int     iCount = m_iNumColors * m_iBitDepth;
    if (m_iBitDepth == 2 && m_iNumColors != 6)
        iCount++;

    while (nBlankRasters > 0)
    {
        for (int i = 0; i < iCount; i++)
        {
            err = Encapsulate (NULL, m_iImageWidth, 0);
            ERRCHECK;
        }
        nBlankRasters--;
    }
    return err;
}

BOOL LDLEncap::IsBlankRaster (BYTE *raster, int width)
{
    while (width > 0)
    {
        if (*raster)
            return FALSE;
        width--;
    }
    return FALSE;
}

DRIVER_ERROR LDLEncap::Encapsulate (const BYTE *input, DWORD size, BOOL bLastPlane)
{
    DRIVER_ERROR    err = NO_ERROR;
    int iPlaneNum = 0;
    if (size > (DWORD) m_iImageWidth)
        size = m_iImageWidth;
    if (m_iBitDepth == 2)
    {
		if (m_iNumColors != 6)
		{
			if (m_cPlaneNumber == 0)
			{
				m_cPlaneNumber++;
				return NO_ERROR;
			}
		}
        int iCPlane;
		if (m_iNumColors == 6)
		{
			iPlaneNum = m_cPlaneNumber % 2;
		}
		else
		{
			iPlaneNum = (m_cPlaneNumber + 1) % 2;
		}
        int iRowNum = (m_iRasterCount / 6) * 2 + iPlaneNum;
        iRowNum = m_iNextRaster;
		if (m_iNumColors == 6)
		{
			iCPlane = m_cPlaneNumber / 2;
		}
		else
		{
			iCPlane = (m_cPlaneNumber - 1) / 2;
		}
        if (iPlaneNum == 0)
        {
            if (!input)
                memset (m_SwathData[iCPlane][iRowNum], 0, m_iImageWidth * 2);
            else
                memcpy (m_SwathData[iCPlane][iRowNum], input, size);
        }
		if (m_iNumColors == 6)
		{
			m_cPlaneNumber = (m_cPlaneNumber + 1) % 12;
		}
		else
		{
			m_cPlaneNumber = (m_cPlaneNumber + 1) % 8;
		}

        if (iPlaneNum == 1)
        {
            // do the dotmapping here
            BYTE    cbyte1, cbyte2;
            BYTE    c1, c2;
            int     j = 0;
            BYTE    r1b1 = 0;
            BYTE    r1b2 = 0;
            BYTE    r2b1 = 0;
            BYTE    r2b2 = 0;
            BYTE    bitmask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

            // Collie changes
            int     iNextBitPos = m_iImageWidth;
            int     iJIncrement = 1;
            if (pPrinterXBow->m_iLdlVersion == 2)
            {
                iNextBitPos = 1;
                iJIncrement = 2;
            }

            memcpy (m_szCompressBuf, m_SwathData[iCPlane][m_iNextRaster], size);

            static  BYTE    rand_table[4][4] = {{0, 3, 1, 2},
                                                {3, 1, 2, 0},
                                                {1, 2, 0, 3},
                                                {2, 0, 3, 1}};

            BYTE    rt1, rt2;
            rt1 = iRowNum % 4;

            for (unsigned int i = 0; i < size; i++)
            {
                cbyte2 = m_szCompressBuf[i];
                cbyte1 = (input == NULL) ? 0 : input[i];

//              1200 dpi raster split into 2 600 dpi rasters

                r1b1 = 0;
                r1b2 = 0;
                r2b1 = 0;
                r2b2 = 0;

                for (int ibit = 0; ibit < 8; ibit++)
                {
                    c1 = (cbyte1 & bitmask[ibit]) ? 1 : 0;
                    c2 = (cbyte2 & bitmask[ibit]) ? 1 : 0;
                    c1 = 2 * c1 + c2;

                    rt2 = (i + ibit) % 4;
                    rt2 = rand_table[rt1][rt2];

                    if (c1 == 2)
                    {
                        if (rt2 == 0)
                        {
                            r1b1 = r1b1 | (0xff & (cbyte1 & bitmask[ibit]));
                            r2b2 = r2b2 | (0xff & bitmask[ibit]);
                        }
                        else if (rt2 == 1)
                        {
                            r1b2 = r1b2 | (0xff & bitmask[ibit]);
                            r2b1 = r2b1 | (0xff & bitmask[ibit]);
                        }

                        else if (rt2 == 2)
                        {
                            r1b1 = r1b1 | (0xff & bitmask[ibit]);
                            r2b1 = r2b1 | (0xff & bitmask[ibit]);
                        }
                        else if (rt2 == 3)
                        {
                            r1b2 = r1b2 | (0xff & bitmask[ibit]);
                            r2b2 = r2b2 | (0xff & bitmask[ibit]);
                        }

                    }
                    else if (c1 == 1)
                    {
                        if (rt2 == 0)
                            r1b1 = r1b1 | (0xff & bitmask[ibit]);
                        else if (rt2 == 1)
                            r1b2 = r1b2 | (0xff & bitmask[ibit]);
                        else if (rt2 == 2)
                            r2b1 = r2b1 | (0xff & bitmask[ibit]);
                        else
                            r2b2 = r2b2 | (0xff & bitmask[ibit]);

                    }
                    else if (c1 == 3)
                    {
                        r1b1 = r1b1 | (0xff & bitmask[ibit]);
                        r1b2 = r1b2 | (0xff & bitmask[ibit]);
                        r2b1 = r2b1 | (0xff & bitmask[ibit]);
                        r2b2 = r2b2 | (0xff & bitmask[ibit]);
                    }
                }
                m_SwathData[iCPlane][m_iNextRaster][j] = r1b1;
                m_SwathData[iCPlane][m_iNextRaster][j+iNextBitPos] = r1b2;
                m_SwathData[iCPlane][m_iNextRaster+1][j]   = r2b1;
                m_SwathData[iCPlane][m_iNextRaster+1][j+iNextBitPos] = r2b2;

                j += iJIncrement;
            }

			if (m_iNumColors == 6)
			{
				m_cPlaneNumber = m_cPlaneNumber % 12;
			}
			else
			{
				m_cPlaneNumber = m_cPlaneNumber % 7;
			}
        }
    }
    else
    {
        if (!input || size == 0)
            memset (m_SwathData[m_iNextColor][m_iNextRaster], 0, m_iImageWidth);
        else
            memcpy (m_SwathData[m_iNextColor][m_iNextRaster], input, size);
    }
    m_iRasterCount++;
    if (m_iBitDepth == 1 || (m_iBitDepth == 2 && iPlaneNum == 1))
        m_iNextColor++;
    if (m_iNextColor == m_iNumColors)
    {
        m_iNextColor = 0;
        if (m_iBitDepth == 2)
            m_iNextRaster += 2;
        else
            m_iNextRaster++;
    }
    if (m_iRasterCount < m_sSwathHeight * m_iNumColors)
        return NO_ERROR;

    if (m_bStartPageNotSent)
    {
        err = StartPage ();
        if (err != NO_ERROR)
            return err;
    }

    err = ProcessSwath (size);

    if (m_iNextRaster >= m_sSwathHeight)
    {
        m_iNextRaster = 0;
    }
    return err;
}

DRIVER_ERROR LDLEncap::ProcessSwath (int iCurRasterSize)
{

    DRIVER_ERROR    err = NO_ERROR;
    unsigned int    start = 0;
    int             size = 0;
    Int32           iVertPosn;
    Int16           sCurSwathHeight = m_iRasterCount / m_iNumColors;
    Int32           LeftEdge  = 0;
    int i;
    m_iVertPosn += ((m_iBlankRasters) * 600 / m_iYResolution) * DEVUNITS_XBOW / 600;
    m_iBlankRasters = 0;

    iVertPosn = m_iVertPosn;

    BOOL    bColorPresent = TRUE;
    BOOL    bBlackPresent = TRUE;
	BOOL    bPhotoPresent = TRUE;
    short   sColorSize = 0;

    int     StartColor = 0;
    int     LastColor  = 1;
    Int32   RightEdge;
    int     delta = 2;
    int     iColors = 0;
    UInt32  uiSwathSize = 0;

    int     iSwings = pPrinterXBow->m_iBytesPerSwing;

    if (m_iNumColors == 1)
	{
        bColorPresent = 0;
        bPhotoPresent = 0;
/* 
		if (pPrinterXBow->ePen == BLACK_PEN)
			bPhotoPresent = 0;
		else
			bBlackPresent = 0;
 */
	}
    if (m_iNumColors == 3)
	{
        bBlackPresent = 0;
		bPhotoPresent = 0;
	}
    if (m_iNumColors == 6)
	{
        bBlackPresent = 0;
	}
    if (m_iNumColors == 4)
	{
        bPhotoPresent = 0;
	}

    if (!m_bBidirectionalPrintingOn)
        m_cPrintDirection = PRNDRN_LEFTTORIGHT;

    Int16   j;
    int     n;
    int     count;
    int     iStartRaster = m_cPassNumber % (2 * m_iBitDepth);
    BYTE    mask = 0xFF;

    if (pPrinterXBow->m_iLdlVersion == 2)
    {
        iStartRaster = 0;   // Version 2 - REVISIT
    }

    if (m_cPrintQuality != QUALITY_DRAFT && m_iYResolution != 300)
    {
        if ((m_cPassNumber % (4 * m_iBitDepth)) < (2 * m_iBitDepth))
            mask = 0xAA;
        else
            mask = 0x55;
    }

/*
 *  Photo Swath
 */

    BYTE    csavMask = mask;

	Int16       iOffset = 0;

    if (m_sRefCount > 64000)
        m_sRefCount = 6;

    if (bPhotoPresent)
    {
		if (bColorPresent)
		{
			iColors = 6;
			LastColor = 6;
			StartColor = 0;
            if (pPrinterXBow->m_iLdlVersion == 1)
            {
			    // 1200 dpi split into two
			    size = GetSwathWidth (StartColor, LastColor, iCurRasterSize/* * m_iBitDepth*/);
            }
            else
            {
                size = GetSwathWidth (StartColor, LastColor, iCurRasterSize * m_iBitDepth);
            }
		}
		else
		{
			iColors = 1;
			LastColor = 1;
			StartColor = 0;
			// 1200 dpi split into two
			size = GetSwathWidth (StartColor, LastColor, iCurRasterSize/* * m_iBitDepth*/);
		}
    }

    if (bPhotoPresent && size)
    {
        if (size % iSwings)
            size = ((size/iSwings) + 1) * iSwings;
        if (pPrinterXBow->m_iLdlVersion == 1)
        {
            RightEdge = LeftEdge + (size * 8 * 600 / m_iXResolution - 1 * (600 / m_iYResolution)) *
                                    (DEVUNITS_XBOW / 600);
        }
        else
        {
            RightEdge = LeftEdge + (size * 8 * 600 / m_iXResolution - 1 * (600 / m_iYResolution)) *
                                    (DEVUNITS_XBOW / (600 * m_iBitDepth));
        }
        Int16   sLastNozzle;
        Int16   sFirstNozzle = 1;
        unsigned int    uSweepSize;
        int     jDelta = m_iYResolution / pPrinterXBow->m_iColorPenResolution;
        jDelta *= m_iBitDepth;

        uiSwathSize = size * iColors * sCurSwathHeight / jDelta;

        uSweepSize = sCurSwathHeight * iSwings / jDelta;
        n = LDL_MAX_IMAGE_SIZE / (uSweepSize);
        count = 0;

        if (m_iBitDepth == 2)
            iStartRaster = (4 - (iStartRaster+1)) % 4;

        if (pPrinterXBow->m_iLdlVersion == 2)
        {
            iStartRaster = 0;   // Collie - REVISIT
        }

        sLastNozzle = sFirstNozzle - 1 + sCurSwathHeight / jDelta;

        BYTE *cb = m_szCompressBuf + 16;    // load sweep command
        memset (m_szCompressBuf, 0x0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));

        
		// 1200 dpi split into two
        int    ib = 0;

        if (m_iYResolution > 300 && m_cPrintQuality != QUALITY_DRAFT)
        {
            iOffset = (sCurSwathHeight / (4 * m_iBitDepth));
            iOffset = iOffset + iOffset * ((m_cPassNumber) % (4 * m_iBitDepth));
        }

        BYTE    cVertAlign = 0;

        if (bColorPresent)
        {
            cVertAlign = m_cPtoCVertAlign;
        }

        for (ib = 0; ib < (int) m_iBitDepth; ib++)
        {
            if (m_cPrintDirection == PRNDRN_RIGHTTOLEFT)
            {
                start = size - iSwings;
                delta = -iSwings;
            }
            else
            {
                start = 0;
                delta = iSwings;
            }

            err = PrintSweep (uiSwathSize, bColorPresent, FALSE, bPhotoPresent,
                              iVertPosn+cVertAlign, LeftEdge, RightEdge, m_cPrintDirection,
                              sFirstNozzle, sLastNozzle);
            ERRCHECK;

            i = start + ib * m_iImageWidth;     // 1200 dpi split into two
            for (int l = 0; l < size; l += iSwings)   // Collie
            {
				for (int k = StartColor+1; k < LastColor; k++)
				{
					mask = csavMask;
					for (j = iOffset + iStartRaster; j < sCurSwathHeight; j += jDelta)
					{
                        for (int is = 0; is < iSwings; is++)
                        {
						    *cb++ = m_SwathData[k][j][i+is]   & mask;
                        }
						mask = ~mask;
					}
					for (j = iStartRaster; j < iOffset; j += jDelta)
					{
                        for (int is = 0; is < iSwings; is++)
                        {
						    *cb++ = m_SwathData[k][j][i+is]   & mask;
                        }
						mask = ~mask;
					}

					count++;
					if (count == n)
					{
						err = LoadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
						memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));
						cb = m_szCompressBuf+16;
						count = 0;
						ERRCHECK;
					}
				}
				mask = csavMask;
				for (j = iOffset + iStartRaster; j < sCurSwathHeight; j += jDelta)
				{
                    for (int is = 0; is < iSwings; is++)
                    {
					    *cb++ = m_SwathData[0][j][i + is]   & mask;
                    }
					mask = ~mask;
				}
				for (j = iStartRaster; j < iOffset; j += jDelta)
				{
                    for (int is = 0; is < iSwings; is++)
                    {
					    *cb++ = m_SwathData[0][j][i + is]   & mask;
                    }
					mask = ~mask;
				}

				count++;
				if (count == n)
				{
					err = LoadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
				    memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));
					cb = m_szCompressBuf+16;
					count = 0;
					ERRCHECK;
				}
                i = i + delta;
            }
            if (count != 0)
            {
                err = LoadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
                memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));
                cb = m_szCompressBuf+16;
                count = 0;
                ERRCHECK;
            }

            if (m_bBidirectionalPrintingOn)
                m_cPrintDirection = (m_cPrintDirection + 1) % 2;

            if (pPrinterXBow->m_iLdlVersion == 2) // Collie
            {
                break;
            }
            LeftEdge += 2;
            RightEdge += 2;

        }   // 1200 dpi split into two - end of for ib = 0 loop

    }

/*
 *  Color Swath
 */
    if (!bPhotoPresent && bColorPresent)
    {
        iColors = 3;
        LastColor = 4;
        StartColor = 1;
        if (!bBlackPresent)
        {
            StartColor = 0;
            LastColor  = 3;
        }
        if (pPrinterXBow->m_iLdlVersion == 1)
        {
            // 1200 dpi split into two
            size = GetSwathWidth (StartColor, LastColor, iCurRasterSize/* * m_iBitDepth*/);
        }
        else
        {
            size = GetSwathWidth (StartColor, LastColor, iCurRasterSize * m_iBitDepth);
        }
        sColorSize = size;
    }
/*
 *  Check if RefCount is close to overflow of 65k.
 */

    if (!bPhotoPresent && bColorPresent && size)
    {
        if (size % iSwings)
            size = ((size / iSwings) + 1) * iSwings;

        if (pPrinterXBow->m_iLdlVersion == 1)
        {
            RightEdge = LeftEdge + (size * 8 * 600 / m_iXResolution - 1 * (600 / m_iYResolution)) *
                                    (DEVUNITS_XBOW / 600);
        }
        else
        {
            RightEdge = LeftEdge + (size * 8 * 600 / m_iXResolution - 1 * (600 / m_iYResolution)) *
                                    (DEVUNITS_XBOW / (600 * m_iBitDepth));
        }
        Int16   sLastNozzle;
        Int16   sFirstNozzle = 1;
        unsigned int    uSweepSize;
        int     jDelta = m_iYResolution / pPrinterXBow->m_iColorPenResolution;
        jDelta *= m_iBitDepth;

        uiSwathSize = size * iColors * sCurSwathHeight / jDelta;

        uSweepSize = sCurSwathHeight * iSwings / jDelta;
        n = LDL_MAX_IMAGE_SIZE / (uSweepSize);
        count = 0;

        if (m_iBitDepth == 2)
        {
            iStartRaster = (4 - (iStartRaster+1)) % 4;
            if (pPrinterXBow->m_iLdlVersion == 2)
            {
                iStartRaster = m_cPassNumber % (m_iBitDepth);
            }
        }

        sLastNozzle = sFirstNozzle - 1 + sCurSwathHeight / jDelta;

        BYTE *cb = m_szCompressBuf + 16;    // load sweep command
        memset (m_szCompressBuf, 0x0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));


		// 1200 dpi split into two
        int    ib = 0;

        if (m_iYResolution > 300 && m_cPrintQuality != QUALITY_DRAFT)
        {
            iOffset = (sCurSwathHeight / (4 * m_iBitDepth));
            iOffset = iOffset + iOffset * ((m_cPassNumber) % (4 * m_iBitDepth));
        }

        for (ib = 0; ib < (int) m_iBitDepth; ib++)
        {
            if (m_cPrintDirection == PRNDRN_RIGHTTOLEFT)
            {
                start = size - iSwings;
                delta = -iSwings;
            }
            else
            {
                start = 0;
                delta = iSwings;
            }
            err = PrintSweep (uiSwathSize, bColorPresent, FALSE, FALSE,
                              iVertPosn, LeftEdge, RightEdge, m_cPrintDirection,
                              sFirstNozzle, sLastNozzle);
            ERRCHECK;

            i = start + ib * m_iImageWidth;     // 1200 dpi split into two
            for (int l = 0; l < size; l += iSwings)   // Collie
            {
                for (int k = StartColor; k < LastColor; k++)
                {
                    mask = csavMask;
                    for (j = iOffset + iStartRaster; j < sCurSwathHeight; j += jDelta)
                    {
                        for (int is = 0; is < iSwings; is++)
                        {
                            *cb++ = m_SwathData[k][j][i + is]   & mask;
                        }
                        mask = ~mask;
                    }
                    for (j = iStartRaster; j < iOffset; j += jDelta)
                    {
                        for (int is = 0; is < iSwings; is++)
                        {
                            *cb++ = m_SwathData[k][j][i + is]   & mask;
                        }
                        mask = ~mask;
                    }

                    count++;
                    if (count == n)
                    {
                        err = LoadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
                        memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));

                        cb = m_szCompressBuf+16;
                        count = 0;
                        ERRCHECK;
                    }

                }
                i = i + delta;

            }
            if (count != 0)
            {
                err = LoadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
                memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));

                cb = m_szCompressBuf+16;
                count = 0;
                ERRCHECK;
            }

            LeftEdge += 2;
            RightEdge += 2;

            if (m_bBidirectionalPrintingOn)
                m_cPrintDirection = (m_cPrintDirection + 1) % 2;
            if (pPrinterXBow->m_iLdlVersion == 2) // Collie
            {
                break;
            }

        }   // 1200 dpi split into two - end of for ib = 0 loop
    }

/*
 *  Black Swath
 */

    size = 0;
    if (bBlackPresent)
        size = GetSwathWidth (0, 1, iCurRasterSize);

    if (size % iSwings)
	    size = ((size/iSwings) + 1) * iSwings;

    RightEdge = LeftEdge + (size * 8 * 600 / m_iXResolution - 1 * (600 / m_iYResolution)) * DEVUNITS_XBOW/600;
	
    if (bBlackPresent && size && m_iBitDepth != 2 &&
        ((m_cPassNumber % 2) == 0 || m_cPrintQuality == QUALITY_DRAFT))
    {
        Int16   sLastNozzle = 0;
        Int16   sFirstNozzle = 1;

        int     xDelta = 0;
        BYTE    cVertAlign = 0;

        if (bColorPresent)
        {
            cVertAlign = m_cKtoCVertAlign;
        }

        if (bColorPresent && sColorSize && m_bBidirectionalPrintingOn)
            m_cPrintDirection = PRNDRN_RIGHTTOLEFT;
        if (m_cPrintDirection == PRNDRN_RIGHTTOLEFT)
        {
            start = size - iSwings;
            delta = -iSwings;
        }
        else
        {
            start = 0;
            delta = iSwings;
        }
        if (m_iYResolution == 300)
            xDelta = iSwings;
        uiSwathSize = ((size/iSwings) * sCurSwathHeight * iSwings * (600 * m_iBitDepth)/ m_iYResolution);

        if (pPrinterXBow->m_iLdlVersion == 2 && m_iNumColors != 1)
        {
            sFirstNozzle = 9;
        }

		err = PrintSweep (uiSwathSize, FALSE, bBlackPresent, FALSE,
						  (iVertPosn + cVertAlign), LeftEdge, RightEdge, m_cPrintDirection, sFirstNozzle, sLastNozzle);

        ERRCHECK;

        i = start;
        BYTE *cb = m_szCompressBuf+16;
        memset (m_szCompressBuf, 0x0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));

        n = LDL_MAX_IMAGE_SIZE / (sCurSwathHeight * iSwings * 600 / m_iYResolution);
        count = 0;

        iOffset = 0;
        if (m_iYResolution > 300 && m_cPrintQuality != QUALITY_DRAFT)
        {
            iOffset = sCurSwathHeight / 4;
            iOffset = iOffset + iOffset * (m_cPassNumber % 4);
        }

        for (int l = 0; l < size; l += iSwings) // Collie
        {
            for (j = iOffset; j < sCurSwathHeight; j++)
            {
                for (int is = 0; is < iSwings; is++)
                {
                    *cb++ = m_SwathData[0][j][i + is]   & mask;
                }
                cb += xDelta;
            }
            for (j = 0; j < iOffset; j++)
            {
                for (int is = 0; is < iSwings; is++)
                {
                    *cb++ = m_SwathData[0][j][i + is]   & mask;
                }
                cb += xDelta;
            }

            count++;
            if (count == n)
            {
                err = LoadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
                memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));

                cb = m_szCompressBuf+16;
                count = 0;
                ERRCHECK;
            }
            i = i + delta;
        }
        if (count != 0)
        {
            err = LoadSweepData (m_szCompressBuf, (unsigned int) (cb - m_szCompressBuf-16));
            memset (m_szCompressBuf, 0, LDL_MAX_IMAGE_SIZE * (iSwings / 2));

            cb = m_szCompressBuf+16;
            count = 0;
            ERRCHECK;
        }

		if (m_bBidirectionalPrintingOn)
			m_cPrintDirection = (m_cPrintDirection + 1) % 2;
    }

    m_iRasterCount = 0;

    if (m_cPrintQuality != QUALITY_DRAFT && m_iYResolution != 300)
    {
        m_cPassNumber = (m_cPassNumber + 1) % (4 * m_iBitDepth);
        m_iVertPosn += ((((sCurSwathHeight/(4 * m_iBitDepth))) * 600 / m_iYResolution) * DEVUNITS_XBOW / 600) / m_iBitDepth;
		if (m_iBitDepth == 1)
        {
            if (m_cPassNumber % 2)
                m_iVertPosn += 4;
            else
                m_iVertPosn -= 4;
        }
        else
        {
            m_iVertPosn -= 2;
            if ((m_cPassNumber % 4) == 0)
                  m_iVertPosn += (DEVUNITS_XBOW / pPrinterXBow->m_iColorPenResolution);
        }
        m_iRasterCount = (sCurSwathHeight - sCurSwathHeight / (4 * m_iBitDepth)) * m_iNumColors;
    }
    else
    {
        m_iVertPosn += ((sCurSwathHeight * 4 * 600) / m_iYResolution);

    }

    return err;
}

void    LDLEncap::FillLidilHeader (void *pLidilHdr, int Command,
                                   UInt16 CmdLen, UInt16 DataLen = 0)
{

    int index = 1;
    m_szCmdBuf[0] = FRAME_SYN;
    WRITE16 (CmdLen);
    m_szCmdBuf[5] = (BYTE) Command;
    index = 6;
    WRITE16(m_sRefCount++);
    index = 8;
    WRITE16 (DataLen);

}

DRIVER_ERROR LDLEncap::PrintSweep (UInt32 SweepSize,
                           BOOL ColorPresent,
                           BOOL BlackPresent,
						   BOOL PhotoPresent,
                           Int32 VerticalPosition,
                           Int32 LeftEdge,
                           Int32 RightEdge,
                           char PrintDirection,
                           Int16 sFirstNozzle,
                           Int16 sLastNozzle)
{
    // determine how many colors will be generated
    UInt16 colorcount = 0;
    UInt32  uiAffectedColors = 0;
    if (ColorPresent == TRUE) colorcount += 3;
    if (BlackPresent == TRUE) colorcount++;
	if (PhotoPresent == TRUE) 
	{
		if (ColorPresent == FALSE)
			colorcount++;
		else
			colorcount+=3;
	}

    UInt16  mem_needed;
    if (pPrinterXBow->m_iLdlVersion == 1)
    {
        mem_needed =   SIZEOF_LDLHDR
                         + SIZEOF_LDL_PRTSWP_CMDOPT
                         + SIZEOF_LDL_PRTSWP_OPTFLDS
                         + SIZEOF_LDL_PRTSWP_COLROPT * colorcount
                         + SIZEOF_LDLTERM;
    
        if (colorcount != 0)
            mem_needed += SIZEOF_LDL_COLROPT_ACTIVECOLR;
    }
    else
    {
        mem_needed =   SIZEOF_LDLHDR
                         + SIZEOF_LDL_PRTSWP_CMDOPT + 7
                         + SIZEOF_LDL_PRTSWP_OPTFLDS
                         + SIZEOF_LDL_PRTSWP_COLROPT + 4
                         + SIZEOF_LDLTERM;
    }

    memset (m_szCmdBuf, 0, mem_needed);

    FillLidilHeader (NULL, eLDLPrintSweep, mem_needed);

    int     index = SIZEOF_LDLHDR;
    if (pPrinterXBow->m_iLdlVersion == 2)
    {
        m_szCmdBuf[index++] = 1;    // Version number
    }
    WRITE32 (SweepSize);
    WRITE32 (VerticalPosition);
    WRITE32 (m_iLeftMargin);
    if (pPrinterXBow->m_iLdlVersion == 1)
    {
        // LIDIL First Version
        m_szCmdBuf[index++] = SWINGFMT_UNCOMPRSS;
    }
    else
    {
        // LIDIL Second Version
        m_szCmdBuf[index++] = 1;
    }
    m_szCmdBuf[index++] = PrintDirection;
    if (pPrinterXBow->m_iLdlVersion == 2)
    {
        WRITE32 (0); // Shingle mask
    }
    WRITE32 (IPS_CARRSPEED|IPS_INIPRNSPEED|ACCURATEPOSN_NEEDED);
 // Carriage Speed - 25 for plain, 12 for photo
    if (m_cPrintQuality == QUALITY_BEST && m_cMediaType == MEDIA_PHOTO)
        m_szCmdBuf[index++] = 12;
    else
        m_szCmdBuf[index++] = 25;
    m_szCmdBuf[index++] = 4; // Initial Print Speed
    m_szCmdBuf[index++] = 1; // Need Accurate Position
    if (pPrinterXBow->m_iLdlVersion == 2)
    {
        m_szCmdBuf[index++] = 1; // Number of entries in the sweep
    }

    // fill in the color information
    if(colorcount == 0)
    {
        m_szCmdBuf[index++] = NO_ACTIVE_COLORS;
        m_szCmdBuf[index++] = FRAME_SYN;
        mem_needed = index;
    }
    else
    {
        // figure out what are the active colors and fill in the optional color fields.

        UInt16 colrpresent = 0;
        UInt16 colr_found=0;
        UInt16 colormask = 0x01;
        UInt16 offset = eLDLBlack;
        UInt16 iDataRes;
        UInt16 iPrintRes;
        uiAffectedColors = offset;
        if (BlackPresent == TRUE)
        {
            uiAffectedColors = 0x1;
        }
        if(BlackPresent == FALSE && PhotoPresent == FALSE)
        {
            offset = eLDLCyan;
            colormask=0x02;
            uiAffectedColors |= 0x0000000e;
        }
        else if (BlackPresent == FALSE && PhotoPresent == TRUE)
        {
			if (ColorPresent == TRUE)
			{
				offset = eLDLCyan;
				colormask=0x02;
                uiAffectedColors |= 0x0000007e;
			}
			else
			{
				offset = eLDLLoBlack;
				colormask=0x40;
                uiAffectedColors |= 0x00000070;
			}
        }

        int actv_colr_index = index;
        int iColorRes = 300;
        if (pPrinterXBow->m_iLdlVersion == 1)
        {
            index += 2;
        }
        else
        {
            iColorRes = 600;
        }
        for(UInt16 i = offset; colr_found < colorcount && i < eLDLMaxColor; i++)
        {
            colr_found++;
            colrpresent = colrpresent | colormask;

            if (pPrinterXBow->m_iLdlVersion == 2)
            {
                WRITE32 (uiAffectedColors);
            }
            WRITE32 (LeftEdge);
            WRITE32 (RightEdge);
            WRITE32 (LeftEdge);
            WRITE32 (RightEdge);

            if ((i == 0 && pPrinterXBow->m_iLdlVersion == 1) || (BlackPresent && pPrinterXBow->m_iLdlVersion == 2))
            {
                iDataRes = 600;
                iPrintRes = pPrinterXBow->m_iBlackPenResolution;
            }
            else
            {
                iDataRes  = iColorRes; // 300;
                iPrintRes = iColorRes; // 300;
            }
            WRITE16 (iDataRes);         // Vertical Data Resolution
            WRITE16 (iPrintRes);        // Vertical Print Resolution

            if (pPrinterXBow->m_iLdlVersion == 2)
            {
                WRITE16 (m_iXResolution * m_iBitDepth);   // Horizontal Data Resolution // Collie
            }
            else
            {
                WRITE16 (m_iXResolution);
            }

			if (m_iXResolution == 300)
			{
				WRITE16 (600);   // Force 2 drop for draft mode.
			}
            else
			{
                if (pPrinterXBow->m_iLdlVersion == 2)
                {
				    WRITE16 (m_iXResolution * m_iBitDepth);   // Horizontal Print Resolution // Collie
                }
                else
                {
                    WRITE16 (m_iXResolution);
                }
			}
            WRITE16 (sFirstNozzle);
            if (sLastNozzle == 0)
            {
                int     iTmp = m_iRasterCount / m_iNumColors;
                if (pPrinterXBow->m_iLdlVersion == 2)
                {
                    WRITE16 (sFirstNozzle - 1 + ((iTmp * iPrintRes) / (m_iYResolution * m_iBitDepth))); // Collie
                }
                else
                {
                    WRITE16 (sFirstNozzle - 1 + ((iTmp * iPrintRes) / (m_iYResolution)))
                }
            }
            else
            {
                WRITE16 (sLastNozzle);
            }

            m_szCmdBuf[index++] = 0;    // Vertical Alignment
            colormask = colormask << 1;
            if (pPrinterXBow->m_iLdlVersion == 2)
            {
                break;
            }

        }
        // write the active color field
        mem_needed = index;
        if (pPrinterXBow->m_iLdlVersion == 1)
        {
            index = actv_colr_index;
            WRITE16 (colrpresent);
            index = mem_needed;
        }

        if (pPrinterXBow->m_iLdlVersion == 2)
        {
            m_szCmdBuf[index++] = 0; // # of entries in the shingle array
        }
        m_szCmdBuf[index++] = FRAME_SYN;
        mem_needed = index;
    }

    // write out the data
    return (pPrinterXBow->Send (m_szCmdBuf, (DWORD) mem_needed));
}


DRIVER_ERROR LDLEncap::LoadSweepData (BYTE *imagedata, int imagesize)
{
    UInt16 mem_needed = SIZEOF_LDLHDR + SIZEOF_LDL_LDSWPDATA_CMDOPT
                                      + SIZEOF_LDLTERM;
    UInt16 diff=0;
    if(mem_needed < LDLPACKET_MINSIZE)
    {
        diff = LDLPACKET_MINSIZE - mem_needed;
        mem_needed = LDLPACKET_MINSIZE;
    }
    memset (m_szCmdBuf, 0, mem_needed);

    BYTE    *compressed_dataptr = imagedata;
    UInt16  compressed_size = imagesize;

#ifdef  APDK_LDL_COMPRESS
    if (m_ldlCompressData)
    {
  	    m_ldlCompressData->Init ((UInt16 *) (imagedata+16), imagesize);
	    CompressData ();
	    GetFrameInfo (&compressed_dataptr, &compressed_size);
    }
#endif

    FillLidilHeader (NULL, eLDLLoadSweepData, mem_needed, compressed_size);

    int     index = SIZEOF_LDLHDR;
    WRITE16 (imagesize);

    if(diff)
    {
        memset (m_szCmdBuf+index, 0xFF, diff);
        index += diff;
    }
    m_szCmdBuf[index++] = FRAME_SYN;

    memcpy (compressed_dataptr, m_szCmdBuf, 16);
    return (pPrinterXBow->Send (compressed_dataptr, (DWORD) compressed_size+16));
}

LDLEncap::~LDLEncap ()
{
    if (m_SwathData)
	// Camera change - allocation is now AllocMem instead of new
	//        delete [] (BYTE *) m_SwathData;
        m_pSys->FreeMem ((BYTE *) m_SwathData);
    if (m_szCmdBuf)
        delete [] m_szCmdBuf;
    if (m_szCompressBuf)
        delete [] m_szCompressBuf;
    if (piCreditCount)
        m_pSys->FreeMem ((BYTE *) piCreditCount);
    if (m_pbyPacketBuff)
        m_pSys->FreeMem ((BYTE *) m_pbyPacketBuff);
    if (pbySync)
        m_pSys->FreeMem ((BYTE *) pbySync);
#ifdef  APDK_LDL_COMPRESS
    if (m_ldlCompressData)
    {
        delete m_ldlCompressData;
        m_ldlCompressData = NULL;
    }
#endif
}

DRIVER_ERROR LDLEncap::StartJob ()
{
    DRIVER_ERROR err = NO_ERROR;
    BYTE *pby = NULL;
    BYTE by = 0;
    BOOL bCreditInitialized = FALSE;
    WORD wCreditWaitCount = 0;

    // Send Sync packet
    err = pPrinterXBow->Send (pbySync, (DWORD) SYNCSIZE);
    if(err)
    {
        return err;
    }

    // Send Sync Complete packet
    err = pPrinterXBow->Send (bySyncComplete, (DWORD) sizeof (bySyncComplete));
    if(err)
    {
        return err;
    }

    // Send Reset LIDIL packet
    err = pPrinterXBow->Send (byResetLIDIL, (DWORD) sizeof (byResetLIDIL));
    if(err)
    {
        return err;
    }

    if (pPrinterXBow->IOMode.bDevID)
    {
        // Enable pacing, get credit packet and update credit count
        err = pPrinterXBow->Send (byEnablePacing, (DWORD) sizeof (byEnablePacing));
        if (err)
        {
            return err;
        }

        while (!bCreditInitialized && wCreditWaitCount++ < CREDIT_WAIT)
        {
            bCreditInitialized = UpdateState (TRUE);
        }

        if (!bCreditInitialized)
        {
            return SYSTEM_ERROR;
        }
        else
        {
            // Send Enable On Change status packet.  We don't have to worry about flushing the buffer,
            // because the ::Send logic will do that for us.  If we run out of credit for a command, the
            // first thing that we do is flush the buffer.  If we haven't already sent the EOCQuery, it
            // will get sent then.
            err = pPrinterXBow->Send (byEOCStatusQuery, (DWORD) sizeof (byEOCStatusQuery));
            if (err)
            {
                return err;
            }
        }

        // Setup Cancel command.  It would be nice to do this in the LDLEncap constructor, but we have
        // to know the JobID before we can setup the cancel command.
        memcpy (pbyCancel, byPrepareToCancel, sizeof (byPrepareToCancel));
        pbyCancel[COMMAND_NUMBER_BYTE] = 0x00;
        pbyCancel[COMMAND_OPT_BYTE] = 0x02;

        pby = (BYTE*) &pPrinterXBow->pLDLEncap;
        if(m_bLittleEndian)
        {
            // Go to last byte of pLDLEncap in memory, since system is little endian
            pby += sizeof (pPrinterXBow->pLDLEncap) - 1;
        }

        for(by = 0; by < sizeof(pPrinterXBow->pLDLEncap); by++)
        {
            if(m_bLittleEndian)
                pbyCancel[by + COMMAND_OPT_BYTE + 1] = *pby--;
            else
                pbyCancel[by + COMMAND_OPT_BYTE + 1] = *pby++;
        }

        // Setup Query command
//        memcpy(byQuery, byEnableResponses, sizeof(byEnableResponses));
//        memcpy(byQuery + sizeof(byEnableResponses), byStatusQuery, sizeof(byStatusQuery));
//        memcpy(byQuery + (sizeof(byEnableResponses) + sizeof(byStatusQuery)),
//               byDisableResponses, sizeof(byDisableResponses));
    }

    UInt16 mem_needed = SIZEOF_LDLHDR + SIZEOF_LDL_JOB_CMDOPT + SIZEOF_LDLTERM;
    if (pPrinterXBow->m_iLdlVersion == 2)
    {
        mem_needed += 4;
    }

    FillLidilHeader (NULL, eLDLStartJob, mem_needed);

    // write in the job id
    int     index = SIZEOF_LDLHDR;
    m_szCmdBuf[index++] = OPERATION_STJOB;
    if (pPrinterXBow->IOMode.bDevID)
    {
        WRITE32 ((UInt32) this);
    }
    else
    {
        WRITE32 ((UInt32) 0xbadfad);    // for deterministic testing, des
    }

    if (pPrinterXBow->m_iLdlVersion == 2)
    {
        WRITE32 (0);    // Shingle Mask option
    }

    // add in sync frame.
    m_szCmdBuf[index++] = FRAME_SYN;

    // fill in the job header and write out the generated data
    err = pPrinterXBow->Send (m_szCmdBuf, (DWORD) mem_needed);
    ERRCHECK;

    err = StartPage ();
    ERRCHECK;

#if 0
	/*
	 *  Query printer if pens are aligned when both pens are present.
	 *  If so, set Bi-Directional printing on.
	 *  If m_bBierectionalPrintingOn is already set or current mode is
	 *  PhotoBest, don't query for pen alignment.
	 */
    if (m_bBidirectionalPrintingOn || m_cPrintQuality == QUALITY_NORMAL)
        return NO_ERROR;

    mem_needed = SIZEOF_LDLHDR + 2 + SIZEOF_LDLTERM;
    memset (m_szCmdBuf, 0, LDLPACKET_MINSIZE);
    for (index = mem_needed; index < LDLPACKET_MINSIZE; index++)
        m_szCmdBuf[index] = 0xFF;
    if (mem_needed < LDLPACKET_MINSIZE)
        mem_needed = LDLPACKET_MINSIZE;
    FillLidilHeader (NULL, eLDLQueryPrinter, mem_needed);
    index = SIZEOF_LDLHDR;
    m_szCmdBuf[index++] = 3;    // Pen Alignment   - is this the right command?
    m_szCmdBuf[index++] = 0;    // Query - Immediate response
    m_szCmdBuf[mem_needed-1] = FRAME_SYN;
    pPrinterXBow->Send (m_szCmdBuf, (DWORD) mem_needed);

    index = 0;
    if ((pSS->FromDevice ((char *) m_szCmdBuf, (WORD *) &index)) == NO_ERROR)
    {
//      what am I looking for here?
        index = SIZEOF_LDLHDR + 1;
//      2 bytes for color alignment, however, bits 7-15 are not used at present
        BYTE    bColor = m_szCmdBuf[index] & 0x7F;
        if ((bColor & 0x0F) == 0x0F ||      // bits 0 - 3 represent KCMY
            (bColor & 0x7E) == 0x7E)        // bits 4 - 6 represent cmk
            m_bBidirectionalPrintingOn = TRUE;
    }
#endif

    return err;
}


DRIVER_ERROR    LDLEncap::EndJob ()
{
    DRIVER_ERROR err = NO_ERROR;

    UInt16 mem_needed = SIZEOF_LDLHDR + SIZEOF_LDL_JOB_CMDOPT + SIZEOF_LDLTERM;

    FillLidilHeader (NULL, eLDLEndJob, mem_needed);

    int     index = SIZEOF_LDLHDR;
    m_szCmdBuf[index++] = OPERATION_ENDJOB;
    if (pPrinterXBow->IOMode.bDevID)
    {
        WRITE32 ((UInt32) this);
    }
    else
    {
        WRITE32 ((UInt32) 0xbadfad);    // for deterministic testing, des
    }

    m_szCmdBuf[index++] = FRAME_SYN;

    err = pPrinterXBow->Send (m_szCmdBuf, (DWORD) mem_needed);
    if(err)
    {
        return err;
    }

    // Send Sync packet
    err = pPrinterXBow->Send (pbySync, SYNCSIZE);
    if(err)
    {
        return err;
    }

    // Send Sync Complete packet
    err = pPrinterXBow->Send (bySyncComplete, (DWORD) sizeof (bySyncComplete));
    if(err)
    {
        return err;
    }

    // Send Reset LIDIL packet
    err = pPrinterXBow->Send (byResetLIDIL, (DWORD) sizeof (byResetLIDIL));
    if(err)
    {
        return err;
    }

    return NO_ERROR;
}

DRIVER_ERROR LDLEncap::StartPage ()
{

    //figure out how much memory we needed
    UInt16 colorcount = 0; //m_iNumColors;
    UInt32 mem_needed = SIZEOF_LDLHDR  + SIZEOF_LDL_LDPAGE_CMDOPT
                                       + SIZEOF_LDL_LDPAGE_OPTFLDS
                                       + SIZEOF_LDLTERM;

    memset (m_szCmdBuf, 0, mem_needed);

    FillLidilHeader (NULL, eLDLLoadPage, (UInt16) mem_needed);

    char    mediatype = MEDIATYPE_PLAIN;
    BYTE    quality   = (BYTE) QUALITYLEVEL_NORMAL;
    if (m_cPrintQuality == QUALITY_BEST && m_cMediaType == MEDIA_PHOTO)
    {
        mediatype = MEDIATYPE_PHOTO;
        quality   = (BYTE) QUALITYLEVEL_BEST;
    }
    else if (m_cPrintQuality == QUALITY_DRAFT)
        quality = (BYTE) QUALITYLEVEL_DRAFT;

    int     index = SIZEOF_LDLHDR;
    m_szCmdBuf[index++] = mediatype;
    m_szCmdBuf[index++] = MEDIASRC_MAINTRAY;
    m_szCmdBuf[index++] = MEDIADEST_MAINBIN;
    m_szCmdBuf[index++] = quality; //PrintQuality;
    m_szCmdBuf[index++] = SPECLOAD_NONE;

    Int32   iVal;
    iVal = (Int32) (m_pthisPC->PhysicalPageSizeX () * 1000);
    WRITE32 ((iVal * DEVUNITS_XBOW) / 1000);
    iVal = (Int32) (m_pthisPC->PhysicalPageSizeY () * 1000);
    WRITE32 ((iVal * DEVUNITS_XBOW) / 1000);

    WRITE32 (MEDIALD_SPEED|NEED_TO_SERVICE_PERIOD|MINTIME_BTW_SWEEP);

    // set up the option fields
    m_szCmdBuf[index++] = 4; // MediaLoadSpeed;
    m_szCmdBuf[index++] = 0; // NeedToServicePeriod;
    WRITE16 (200);           // MinTimeBetweenSweeps

    if (colorcount == 0)
    {
        m_szCmdBuf[index++] = FRAME_SYN;
    }

    m_bStartPageNotSent = FALSE;
    // write out the data
    return (pPrinterXBow->Send (m_szCmdBuf, (DWORD) mem_needed));
}

DRIVER_ERROR LDLEncap::Continue ()
{

    UInt16 mem_needed = SIZEOF_LDLHDR + 1
                        + SIZEOF_LDLTERM;
    int     index;
    memset (m_szCmdBuf, 0, LDLPACKET_MINSIZE);
    for (index = mem_needed; index < LDLPACKET_MINSIZE; index++)
        m_szCmdBuf[index] = 0xFF;
    if (mem_needed < LDLPACKET_MINSIZE)
        mem_needed = LDLPACKET_MINSIZE;
    FillLidilHeader (NULL, eLDLControl, mem_needed);
    index = SIZEOF_LDLHDR;
    m_szCmdBuf[index] = OPERATION_CONTINUE;
    m_szCmdBuf[mem_needed-1] = FRAME_SYN;
    return (pPrinterXBow->Send (m_szCmdBuf, (DWORD) mem_needed));
}

void LDLEncap::Cancel ()
{

    UInt16 mem_needed = SIZEOF_LDLHDR + SIZEOF_LDL_JOB_CMDOPT
                        + SIZEOF_LDLTERM;
    int     index;
    memset (m_szCmdBuf, 0, LDLPACKET_MINSIZE);
    for (index = mem_needed; index < LDLPACKET_MINSIZE; index++)
        m_szCmdBuf[index] = 0xFF;
    if (mem_needed < LDLPACKET_MINSIZE)
        mem_needed = LDLPACKET_MINSIZE;
    FillLidilHeader (NULL, eLDLControl, mem_needed);
    index = SIZEOF_LDLHDR;
    WRITE32 ((UInt32) this);
    m_szCmdBuf[index] = OPERATION_CANCJOB;
    m_szCmdBuf[mem_needed-1] = FRAME_SYN;
    pPrinterXBow->Send (m_szCmdBuf, (DWORD) mem_needed);
}

DRIVER_ERROR LDLEncap::CleanPen ()
{

    UInt16 mem_needed = SIZEOF_LDLHDR + 1 + 2
                        + SIZEOF_LDLTERM;
    int     index;
    short   sNumSpits = 32;
    memset (m_szCmdBuf, 0, LDLPACKET_MINSIZE);
    for (index = mem_needed; index < LDLPACKET_MINSIZE; index++)
        m_szCmdBuf[index] = 0xFF;
    if (mem_needed < LDLPACKET_MINSIZE)
        mem_needed = LDLPACKET_MINSIZE;
    FillLidilHeader (NULL, eLDLHandlePen, mem_needed);
    index = SIZEOF_LDLHDR;
    m_szCmdBuf[index++] = OPERATION_SPIT_PEN;
    WRITE16 (sNumSpits);
    m_szCmdBuf[mem_needed-1] = FRAME_SYN;
    pPrinterXBow->Send (m_szCmdBuf, (DWORD) mem_needed);

    return NO_ERROR;
}

DRIVER_ERROR LDLEncap::EndPage ()
{

    DRIVER_ERROR    err = NO_ERROR;
    int icount = 0;
    int iCurNumRasters = m_iRasterCount;
    if ((m_cPrintQuality == QUALITY_DRAFT || m_iYResolution == 300) && m_iRasterCount)
        icount = 1;
    else if (m_cPrintQuality != QUALITY_DRAFT)
    {
        icount = 4 * m_iBitDepth;
        iCurNumRasters = m_sSwathHeight * m_iNumColors;
    }

    int i, j, n;
    n = m_sSwathHeight / (4 * m_iBitDepth);

    n = n * (m_cPassNumber + 1) - m_iNextRaster;
    for (i = 0; i < m_iNumColors; i++)
    {
        for (j = 0; j < n; j++)
            memset (m_SwathData[i][m_iNextRaster+j], 0, m_iImageWidth * m_iBitDepth);
    }
    m_iNextRaster += n;
    n = m_sSwathHeight / (4 * m_iBitDepth);

    while (icount)
    {
        m_iRasterCount = iCurNumRasters;
        err = ProcessSwath (m_iImageWidth);
        if (err != NO_ERROR)
            break;
        icount--;
        if (m_iNextRaster >= m_sSwathHeight)
            m_iNextRaster = 0;
        for (i = 0; i < m_iNumColors; i++)
        {
            for (j = 0; j < n; j++)
                memset (m_SwathData[i][m_iNextRaster+j], 0, m_iImageWidth * m_iBitDepth);
        }
        m_iNextRaster += n;
    }

    UInt16 mem_needed = SIZEOF_LDLHDR
                        + SIZEOF_LDL_EJPAGE_CMDOPT
                        + SIZEOF_LDL_EJPAGE_OPTFLDS
                        + SIZEOF_LDLTERM;

    memset (m_szCmdBuf, 0, mem_needed);
    FillLidilHeader(NULL, eLDLEjectPage, mem_needed);

    int     index = SIZEOF_LDLHDR;
    WRITE32 (MEDIA_EJSPEED);

    m_szCmdBuf[index++] = 15;
    m_szCmdBuf[index++] = FRAME_SYN;

    if(err == NO_ERROR)
    {
        err = pPrinterXBow->Send (m_szCmdBuf, (DWORD) mem_needed);
    }

    m_sRefCount = 6;
    m_iBlankRasters = 0;
    m_iVertPosn = (int) (m_pthisPC->PrintableStartY () * DEVUNITS_XBOW);
    m_iRasterCount = 0;
    m_iNextRaster = 0;
    m_iNextColor  = 0;

	float   fXOverSpray = 0.0;
    float   fYOverSpray = 0.0;
    float   fLeftOverSpray = 0.0;
    float   fTopOverSpray  = 0.0;
	FullbleedType   fbType;
    if (m_pthisPC->bDoFullBleed &&
        pPrinterXBow->FullBleedCapable (m_pthisPC->thePaperSize,
									  &fbType,
                                      &fXOverSpray, &fYOverSpray,
                                      &fLeftOverSpray, &fTopOverSpray))
    {

		/*
		 *      To get the printer to do fullbleed printing, move the vertical postion 
		 *      to cover the overspary. Overspray is needed to take care of
		 *      skew during paper pick. These values may be mech dependent.
		 *      Currently, supported only on PhotoSmart 100, Malibu. DJ3600 supports
		 *      fullbleed printing also. The current values for overspray are
		 *      0.059 inch for top, bottom and left edges and 0.079 for right edge.
		 */
		m_iVertPosn = (int) (-fTopOverSpray * DEVUNITS_XBOW);
    }

    if (/*m_iYResolution != 300*/m_cPrintQuality != QUALITY_DRAFT)
        m_iRasterCount = (m_sSwathHeight - m_sSwathHeight / 4) * m_iNumColors;

    m_bStartPageNotSent = TRUE;

    if (m_cPrintQuality != QUALITY_DRAFT && m_iYResolution != 300)
    {
        m_iRasterCount = (m_sSwathHeight - m_sSwathHeight / (4*m_iBitDepth)) * m_iNumColors;
        m_iVertPosn -= (((m_sSwathHeight - m_sSwathHeight / (4*m_iBitDepth)) * 600 / m_iYResolution) * DEVUNITS_XBOW / 600);
    }
    m_cPassNumber = 0;

    if (m_pthisPC->bDoFullBleed &&
        pPrinterXBow->FullBleedCapable (m_pthisPC->thePaperSize,
									  &fbType,
                                      &fXOverSpray, &fYOverSpray,
                                      &fLeftOverSpray, &fTopOverSpray))
	{
		if (m_iVertPosn < -850) m_iVertPosn = -850;
	}
	else
	{
		if (m_iVertPosn < -600) m_iVertPosn = -600;
	}
    if (m_iBitDepth == 2)
        m_iVertPosn += 6;

    for (i = 0; i < m_iNumColors; i++)
    {
        for (int j = 0; j < m_sSwathHeight; j++)
        {
            memset (m_SwathData[i][j], 0, m_iImageWidth);
        }
    }

    return (err);
}

// This routine dynamically allocates memory in which to read bytes from the port.  We allocate
// memory in MAX_PACKET_READ_SIZE chunks, which we set in ldlencap.h to 256 bytes.  The minimum
// read size is the size of a printer packet, which is 64 bytes.  We shouldn't have to read more
// than 256 bytes from the port since we only are getting credit and status.  Developers may want
// to increase or decrease the MAX_PACKET_READ_SIZE for their particular system based upon whether
// memory is easier to get statically or dynamically.  For instance, a system with a lot of static
// memory may want to increase MAX_PACKET_READ_SIZE to 4096 to minimize reads.  A system with
// limited static memory may want to allocate memory in smaller chunks, so 256 may be better.  The
// developer may reduce the MAX_PACKET_READ_SIZE to as little as 64 if desired, but it must always
// be a multiple of 64.

/*
 *    Author: Don Castrapel
 */

BOOL LDLEncap::GetPackets(DWORD &dwBytesRead)
{
    DRIVER_ERROR err = NO_ERROR;
    DWORD dwReadSize = 0;
    WORD wPacketWaitCount = 0;
    BYTE *pbyPacketBuff = NULL;

    dwBytesRead = 0;

    if(m_pbyPacketBuff)
    {
        // If we've had to reallocate the packet read buffer because it was too small to hold a read,
        // let's deallocate it and start with the original size.  This will prevent us hanging onto
        // what could be a large chunk of memory while also not performing multiple allocs and deallocs
        // in the normal case where the buffer holds the entire read
        if(m_dwPacketBuffSize != MAX_PACKET_READ_SIZE)
        {
            m_pSys->FreeMem ((BYTE *) m_pbyPacketBuff);
            m_pbyPacketBuff = NULL;
            m_dwPacketBuffSize = 0;
        }
        else
            memset(m_pbyPacketBuff, 0, MAX_PACKET_READ_SIZE);
    }

    // Wait for packets
    while(!dwBytesRead && wPacketWaitCount++ < PACKET_WAIT)
    {
        if(m_pSys->BusyWait((DWORD)100) == JOB_CANCELED)
        {
            return FALSE;
        }

        dwReadSize = MAX_PACKET_READ_SIZE;

        do
        {
            // If we've done a read but it was not a multiple of MAX_PACKET_READ_SIZE, then we
            // didn't read the full request size last time.  That means that we read some bytes
            // but that the printer didn't have any more to send
            if(dwBytesRead % MAX_PACKET_READ_SIZE)
            {
                break; // Out of do loop
            }

            // FromDevice resets dwReadSize to the number of bytes read from the port
            dwReadSize = MAX_PACKET_READ_SIZE;

            if(!m_pbyPacketBuff)
            {
                // First read, allocate buffer to hold data
                m_pbyPacketBuff = m_pSys->AllocMem(MAX_PACKET_READ_SIZE);
                if(!m_pbyPacketBuff)
                {
                    return FALSE;
                }
                memset(m_pbyPacketBuff, 0, MAX_PACKET_READ_SIZE);
                m_dwPacketBuffSize = MAX_PACKET_READ_SIZE;
            }
            else if(dwBytesRead)
            {
                // We've already read some bytes, so allocate a temporary buffer to store
                // what we've read so far.  We'll copy what we've read into the temporary buffer,
                // delete the original buffer, reallocate a new buffer MAX_PACKET_READ_SIZE
                // bytes larger, read the temporary buffer back into the newly reallocated
                // buffer, then delete the temporary buffer
                pbyPacketBuff = m_pSys->AllocMem(dwBytesRead + MAX_PACKET_READ_SIZE);
                if(!pbyPacketBuff)
                {
                    return FALSE;
                }
                m_dwPacketBuffSize += MAX_PACKET_READ_SIZE;
                memset(pbyPacketBuff, 0, m_dwPacketBuffSize);

                memcpy(pbyPacketBuff, m_pbyPacketBuff, dwBytesRead);
                if(m_pbyPacketBuff)
                {
                    m_pSys->FreeMem((BYTE *)m_pbyPacketBuff);
                    m_pbyPacketBuff = NULL;
                }

                m_pbyPacketBuff = pbyPacketBuff;
            }

            err = m_pSys->FromDevice((m_pbyPacketBuff + dwBytesRead), &dwReadSize);
            if(err)
            {
                return FALSE;
            }
            dwBytesRead += dwReadSize;
        } while(!err && dwReadSize);
    } // while(!dwBytesRead && wPacketWaitCount++ < PACKET_WAIT)

    if(!dwBytesRead)
    {
        // No data to read from port
        return FALSE;
    }

    return TRUE;
}

/*
 *    Author: Don Castrapel
 */

BOOL LDLEncap::UpdateState(BOOL bInitialize)
{
//    DRIVER_ERROR err = NO_ERROR;
    BOOL bPacketsReceived = FALSE;
    BOOL bUpdatedState = FALSE;
    BYTE byPacketType = 0;
    BYTE byCommandNumber = 0;
    WORD wCommandLength = 0;
    WORD wDataLength = 0;
    WORD wReferenceNumber = 0;
    DWORD dwBytesRead = 0;
    DWORD dwBytesProcessed = 0;
    BYTE by = 0;

    // Read packets from port
    bPacketsReceived = GetPackets(dwBytesRead);
    if(!bPacketsReceived)
    {
        return FALSE;
    }

    while(dwBytesProcessed < dwBytesRead)
    {
        if(m_pbyPacketBuff[dwBytesProcessed] != '$')
        {
            return FALSE;
        }

        // Get packet type and command number, command length, data length
        byPacketType = m_pbyPacketBuff[PACKET_TYPE_BYTE + dwBytesProcessed];
        byCommandNumber = m_pbyPacketBuff[COMMAND_NUMBER_BYTE + dwBytesProcessed];
        wCommandLength = (m_pbyPacketBuff[COMMAND_LENGTH_BYTE + dwBytesProcessed] << 8) |
                          m_pbyPacketBuff[COMMAND_LENGTH_BYTE + 1 + dwBytesProcessed];
        wDataLength = (m_pbyPacketBuff[DATA_LENGTH_BYTE + dwBytesProcessed] << 8) |
                       m_pbyPacketBuff[DATA_LENGTH_BYTE + 1 + dwBytesProcessed];
        wReferenceNumber = (m_pbyPacketBuff[REFERENCE_NUMBER_BYTE + dwBytesProcessed] << 8) |
                            m_pbyPacketBuff[REFERENCE_NUMBER_BYTE + 1 + dwBytesProcessed];

        // We should only get packet type 16 (Response, Command Executed), packet type
        // 24 (Response, Auto), packet type 32 (Absolute Credit) or packet type 33
        // (Incremental Credit).

        // For credit packets, we'll update the credit.  Credit for each command number starts at
        // byte 12, or m_pbyPacketBuff[11].  Credit is a 2-byte signed value, so we have to multiply
        // the loop counter by 2 to get the right array index.  A byPacketType of 32 indicates
        // absolute credit, while a byPacketType value of 33 indicates incremental credit.

        // For a command executed packet, we'll switch again and do the appropriate thing based upon
        // the command number for which the printer generated the response packet
        switch(byPacketType)
        {
            case ABSOLUTE_CREDIT:
                if(bInitialize)
                {
                    // Get number of commands to allocate memory for CreditCount buffer.
                    // Byte 11(byReadBuff[10])
                    byNumberOfCommands = m_pbyPacketBuff[NUMBER_OF_COMMANDS_BYTE];

                    if(piCreditCount)
                    {
                        m_pSys->FreeMem((BYTE*)piCreditCount);
                    }
                    piCreditCount =
                        (short int *)(m_pSys->AllocMem(sizeof(short int) * byNumberOfCommands));
                    if(!piCreditCount)
                    {
                        return FALSE;
                    }
                    memset(piCreditCount, 0, (sizeof(short int) * byNumberOfCommands));
                }

                for(by = 0; by < byNumberOfCommands; by++)
                {
                    piCreditCount[by] = (m_pbyPacketBuff[by * 2 + CREDIT_BYTE + dwBytesProcessed] << 8) |
                                        (m_pbyPacketBuff[by * 2 + CREDIT_BYTE + 1 + dwBytesProcessed]);
                }

                bUpdatedState = TRUE;

                break;

            case INCREMENTAL_CREDIT:
                if(bInitialize)
                {
                    // If we're initializing we must wait for an absolute credit packet
                    break;
                }

                for(by = 0; by < byNumberOfCommands; by++)
                {
                    piCreditCount[by] += (m_pbyPacketBuff[by * 2 + CREDIT_BYTE + dwBytesProcessed] << 8) |
                                         (m_pbyPacketBuff[by * 2 + CREDIT_BYTE + 1 + dwBytesProcessed]);
                }

                bUpdatedState = TRUE;

                break;

/*          case RESPONSE_COMMAND_EXECUTED:
                if(bInitialize)
                {
                    // If we're initializing we must wait for an absolute credit packet
                    return FALSE;
                }

                // We should only get command number 5, which is the Query command
                switch(byCommandNumber)
                {
                    case COMMAND_QUERY:
                        // Copy status into LDLEncap's status buffer
                        memcpy(byStatusBuff, (m_pbyPacketBuff + dwBytesProcessed),
                              (wCommandLength + wDataLength));
                        bNewStatus = TRUE;

                        break;

                    default:
                        break;
                } // switch(byCommandNumber)

                break;*/

            case RESPONSE_AUTO:
                if(bInitialize)
                {
                    // If we're initializing we must wait for an absolute credit packet
                    break;
                }

                // We should only get reference number 1, which is what I set up for the
                // EOCStatusQuery command
                switch(wReferenceNumber)
                {
                    case AUTO_RESPONSE_STATUS:

                        // Copy status into LDLEncap's status buffer
                        memcpy(byStatusBuff, (m_pbyPacketBuff + dwBytesProcessed),
                              (wCommandLength + wDataLength));
                        bNewStatus = TRUE;
                        bUpdatedState = TRUE;

                        break;

                    default:

                        break;
                } // switch(wReferenceNumber)

                break;

            default:
                if(bInitialize)
                {
                    // If we're initializing we must wait for an absolute credit packet
                    break;
                }

                break;

        } // switch(byPacketType)

        dwBytesProcessed += (DWORD)(wCommandLength + wDataLength);
    } // while(dwBytesProcessed < dwBytesRead)

    return bUpdatedState;
}

#ifdef  APDK_LDL_COMPRESS

/*
 *  Compression Related
 *  Mark Lund
 */


///////////////////////////////////////////////////////////////////////
// Flush_Image
///////////////////////////////////////////////////////////////////////

UInt16 LDLEncap::FlushImage ()
{
    UInt16    command;
    UInt16    wsize;
    UInt16    bsize;
    UInt16    *from_ptr;
    int       index;

    wsize = m_ldlCompressData->image_cnt;
    bsize = 0;

    if (wsize)
    {
        from_ptr = m_ldlCompressData->image_ptr;

        command = FILL_IMAGE_CMD | (wsize-1);

        index = 0;
        WRITE16(command);
        if (m_bLittleEndian)
        {
            *m_ldlCompressData->out_ptr++ = (((UInt16) m_szCmdBuf[1]) << 8) | m_szCmdBuf[0];
        }
        else
        {
            *m_ldlCompressData->out_ptr++ = command;
        }
/*
        memcpy (m_ldlCompressData->out_ptr, from_ptr, sizeof (UInt16) * wsize);
        m_ldlCompressData->out_ptr += wsize;
*/
        for (UInt16 i = 0; i < wsize; i++)
        {
            *m_ldlCompressData->out_ptr++ = *from_ptr++;
        }
        bsize = ((m_ldlCompressData->image_cnt+1) * 2);
        m_ldlCompressData->out_cnt += bsize;

        m_ldlCompressData->image_cnt = 0;
    }

    return bsize;
}

///////////////////////////////////////////////////////////////////////
// Flush_Copy
///////////////////////////////////////////////////////////////////////
UInt16 LDLEncap::FlushCopy (UInt16 value)
{
    UInt16    command;
    UInt16    size;

    int       index;

    size = m_ldlCompressData->copy_cnt;

    if (size)
    {
        UInt16  *uP = m_ldlCompressData->out_ptr++;
        size = 2;
        if (value == 0)
        {
            command = FILL_0000_CMD | (m_ldlCompressData->copy_cnt-1);
        }
        else if (value == 0xFFFF)
        {
            command = FILL_FFFF_CMD | (m_ldlCompressData->copy_cnt-1);
        }
        else
        {
            command = FILL_NEXT_CMD | (m_ldlCompressData->copy_cnt-1);
//            index = 0;
//            WRITE16 (value);
            *m_ldlCompressData->out_ptr++ = value;
//            *m_ldlCompressData->out_ptr++ = (((UInt16) m_szCmdBuf[1]) << 8) | m_szCmdBuf[0];
            size = 4;
        }

        index = 0;
        WRITE16(command);
        if (m_bLittleEndian)
        {
            *uP = (((UInt16) m_szCmdBuf[1]) << 8) | m_szCmdBuf[0];
        }
        else
        {
            *uP = command;
        }

        m_ldlCompressData->out_cnt += size;
        m_ldlCompressData->copy_cnt = 0;
    }
    return size;
}

///////////////////////////////////////////////////////////////////////
// Compress_Data
///////////////////////////////////////////////////////////////////////
void LDLEncap::CompressData (Int16 compressionmode)
{
    Int16   i;
    UInt16  *in_ptr;
    UInt16  in;
    UInt16  last=0;
    UInt16  copy_item;
    UInt16  data_length;

    LDLCOMPMODE mode = IN_NOT;


    m_ldlCompressData->out_cnt = 0;
    m_ldlCompressData->image_cnt = 0;
    m_ldlCompressData->copy_cnt = 0;

    m_ldlCompressData->out_ptr = &m_ldlCompressData->out_array[8];
    data_length = m_ldlCompressData->data_length;

    if ((data_length & 1) != 0)
    {
    //    ErrorTrap((char *)"Data length is odd.");
    }

    copy_item = 0;
    in_ptr = &m_ldlCompressData->raw_data[0];

    for (i=0; i<data_length; i+=2)
    {
        in = *in_ptr;

        switch(mode)
        {
            case IN_NOT:
            {
                /* default the first entry to 'image' */
                last = in;
                m_ldlCompressData->image_ptr = in_ptr;
                m_ldlCompressData->image_cnt = 1;
                mode = IN_FIRST;
                break;
            }

            case IN_FIRST:
            {
#if ALLOW_FILL_NEXT_CMD
                if (last == in)
#else
                if ((last == in) && ((in==0xFFFF) || (in == 0)) )
#endif
                {
                    mode = IN_COPY;
                    m_ldlCompressData->copy_cnt = 2;
                    m_ldlCompressData->image_cnt = 0;
                    copy_item = in;
                }
                else
                {
                    mode = IN_IMAGE;
                    m_ldlCompressData->image_cnt++;
                    last = in;
                }
                break;
            }

            case IN_COPY:
            {
                if (last == in)
                {
                    m_ldlCompressData->copy_cnt++;
                }
                else
                {
                    /* revisit - could allow 2 words of copy if the data is
                    0000 or FFFF */

                    /* convert a copy cnt of 2 to an image */
                    UInt16 copy_count = m_ldlCompressData->copy_cnt;

                    if (copy_count <= m_ldlCompressData->run_length)
                    {
                        if (m_ldlCompressData->image_cnt == 0)
                        {
                            /* point the pointer to the first element */
                            m_ldlCompressData->image_ptr = in_ptr - copy_count;
                        }
                        m_ldlCompressData->image_cnt += (1+copy_count);
                     m_ldlCompressData->copy_cnt = 0;
                    }
                    else
                    {
                        /* have enough to be a legal copy */

                        (void) FlushImage ();

                        (void) FlushCopy (copy_item);

                        m_ldlCompressData->image_ptr = in_ptr;
                        m_ldlCompressData->image_cnt = 1;
                    }
                    mode = IN_IMAGE;
                    last = in;
                }
                break;
            }

            case IN_IMAGE:
            {
#if ALLOW_FILL_NEXT_CMD
                if (last == in)
#else
                if ((last == in) && ((in==0xFFFF) || (in == 0)) )
#endif
                {
                    m_ldlCompressData->image_cnt--;

                    mode = IN_COPY;
                    copy_item = in;
                    m_ldlCompressData->copy_cnt = 2;
                }
                else /* different */
                {
                    last = in;
                    m_ldlCompressData->image_cnt++;
                }
                break;
            }

            default:
            {
                break;
            }
        }
        in_ptr++;
    } /* next data - end of processing */

    /* flush out the remainder */

    switch(mode)
    {
        case IN_COPY:
        {
            /* have enough to be a legal copy */
            (void) FlushImage ();

            (void) FlushCopy (copy_item);
            break;
        }
        case IN_IMAGE:
        case IN_FIRST:
        {
            (void) FlushImage ();
            break;
        }
        default:
            break;
    }

    if (m_ldlCompressData->out_cnt > 2048+16)
    {
    //    ErrorTrap("out cnt too big");
    //    exit (-7);
    }
}

/////////////////////////////////////////////////////////////////////////////////
//GetFrameInfo 
/////////////////////////////////////////////////////////////////////////////////
BOOL LDLEncap::GetFrameInfo (BYTE **outdata, UInt16 *data_size)
{
    *outdata = (unsigned char *) &m_ldlCompressData->out_array[0];
    *data_size = m_ldlCompressData->out_cnt;
    return(TRUE);
}

/////////////////////////////////////////////////////////////////////////////////
//Init: to init/reinit the data structure. 
/////////////////////////////////////////////////////////////////////////////////
BOOL comp_ptrs_t::Init (UInt16 *data, UInt16 datasize)
{
	image_ptr   = data;
	raw_data    = data;
	data_length = datasize;

	run_length = MAX_RUNLENGTH;
	display = 0;

	return(TRUE);
}
#endif  // APDK_LDL_COMPRESS

APDK_END_NAMESPACE

#endif  // APDK_DJ3320
