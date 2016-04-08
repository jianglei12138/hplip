/*****************************************************************************\
  printer.cpp : Implimentation for the Printer class

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


#include "header.h"
#include "io_defs.h"
#include "resources.h"


//
// ** Printer CLASS **

APDK_BEGIN_NAMESPACE
extern BYTE* GetHTBinary();
APDK_END_NAMESPACE

APDK_BEGIN_NAMESPACE

Printer::Printer
(
    SystemServices* pSys,
    int numfonts,
    BOOL proto
) :
    constructor_error(NO_ERROR),
    IOMode(pSys->IOMode),
#if defined(APDK_FONTS_NEEDED)
    iNumFonts(numfonts),
#else
    iNumFonts(0),
#endif
    bCheckForCancelButton(FALSE),
    ulBytesSentSinceCancelCheck(0),
    ePen(NO_PEN),
    CMYMap(NULL),
    pSS(pSys),
    InSlowPollMode(0),
    iTotal_SLOW_POLL_Count(0),
    iMax_SLOW_POLL_Count(DEFAULT_SLOW_POLL_COUNT),
    ErrorTerminationState(FALSE),
    iBuffSize(pSys->GetSendBufferSize()),
    iCurrBuffSize(0),
    EndJob(FALSE),
    ModeCount(2),
    m_bVIPPrinter(FALSE)
{

    int i = 0; // counter

#ifdef  APDK_LINUX
    m_iNumPages = 0;
#endif

    for (i = 0; i < MAX_PRINTMODES; i++)
    {
        pMode[i]=NULL;
    }

    //CompatiblePens[0] = BOTH_PENS;
    //for (i = 1; i < MAX_COMPATIBLE_PENS; i++)
    //{
    //    CompatiblePens[i] = DUMMY_PEN;
    //}

    if (IOMode.bDevID)
    {
       iMax_SLOW_POLL_Count = DEFAULT_SLOW_POLL_BIDI;
    }

    // Allocate memory for my send buffer
    pSendBuffer = NULL;
#ifdef APDK_BUFFER_SEND
    pSendBuffer = pSS->AllocMem(iBuffSize+2);
    CNEWCHECK(pSendBuffer);
#endif

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
    m_bRotateBackPage = TRUE;
#endif

/*
 *  LaserJet printers do not send status via device id string. PJL is used to
 *  get status.
 *  REVISIT:    Do the same for Business Inkjets as well.
 */

    m_bStatusByPJL = FALSE;
    char    *tmpStr;
    if ((strstr ((char *) pSS->strDevID, "LaserJet")) &&
        (tmpStr = strstr ((char *) pSS->strDevID, "CMD:")) &&
        (tmpStr = strstr (tmpStr+4, "PJL")))
    {
        m_bStatusByPJL = TRUE;
    }

#if defined(APDK_FONTS_NEEDED)
    // create dummy font objects to be queried via EnumFont
    // these fonts used by all except DJ400

    for (i = 0; i<=MAX_PRINTER_FONTS; i++)
        fontarray[i] = NULL;


#ifdef APDK_COURIER
    fontarray[COURIER_INDEX] = new Courier();
    CNEWCHECK(fontarray[COURIER_INDEX]);
#endif
#ifdef APDK_CGTIMES
    fontarray[CGTIMES_INDEX] = new CGTimes();
    CNEWCHECK(fontarray[CGTIMES_INDEX]);
#endif
#ifdef APDK_LTRGOTHIC
    fontarray[LETTERGOTHIC_INDEX] = new LetterGothic();
    CNEWCHECK(fontarray[LETTERGOTHIC_INDEX]);
#endif
#ifdef APDK_UNIVERS
    fontarray[UNIVERS_INDEX] = new Univers();
    CNEWCHECK(fontarray[UNIVERS_INDEX]);
#endif

#endif  //
} //Printer


Printer::~Printer()
{
    if (pMode[GRAYMODE_INDEX])
    {
        if (pMode[GRAYMODE_INDEX]->dyeCount==3) // only happens when compgray map used
        {
            pSS->FreeMem((BYTE*) pMode[GRAYMODE_INDEX]->cmap.ulMap1 );
        }
    }

    for (int i = 0; i < MAX_PRINTMODES; i++)
    {
        if (pMode[i])
        {
            delete pMode[i];
        }
    }

#ifdef APDK_BUFFER_SEND
    if (pSendBuffer != NULL)
    {
        pSS->FreeMem(pSendBuffer);
    }
#endif

#ifdef APDK_COURIER
    delete fontarray[COURIER_INDEX];
#endif
#ifdef APDK_CGTIMES
    delete fontarray[CGTIMES_INDEX];
#endif
#ifdef APDK_LTRGOTHIC
    delete fontarray[LETTERGOTHIC_INDEX];
#endif
#ifdef APDK_UNIVERS
    delete fontarray[UNIVERS_INDEX];
#endif

} //~Printer


////////////////////////////////////////////////////////////////////////////
Compressor* Printer::CreateCompressor (unsigned int RasterSize)
{
    return new Mode9 (pSS, RasterSize);   // most printers use mode 9
}

////////////////////////////////////////////////////////////////////////////
Compressor* Printer::CreateBlackPlaneCompressor (unsigned int RasterSize,
                                                 BOOL bVIPPrinter)
{
    return new Mode9 (pSS, RasterSize, bVIPPrinter);   // most printers use mode 9
}

////////////////////////////////////////////////////////////////////////////
// ** API functions
//


DRIVER_ERROR Printer::Flush
(
    int FlushSize       // = MAX_RASTERSIZE
)
// flush possible leftover garbage --
// default call will send one (maximal) raster's worth of zeroes
{
    ASSERT(FlushSize > 0);
    ASSERT(FlushSize <= MAX_RASTERSIZE);

    DRIVER_ERROR err = NO_ERROR;
    int iChunkSize = 1000;
    BYTE *zero = NULL;

    // Try to allocate iChunkSize bytes of memory.  If we fail then cut size
    // in half and try again.  If we can't allocate even 10 bytes then bail
    // with a memory allocation error.  Flush is called at the beginning of
    // the print job - if there is no memory allocate now then we won't be
    // printing in any case.
    while (iChunkSize > 10 && zero == NULL)
    {
        zero = pSS->AllocMem(iChunkSize);
        if (zero == NULL)
        {
            iChunkSize /= 2;
        }
    }

    if (zero == NULL)
    {
        return ALLOCMEM_ERROR;
    }

    memset(zero, 0, iChunkSize);
    int iChunks = (FlushSize / iChunkSize) + 1;

    for (int i = 0; i < iChunks; i++)
    {
        if ((err = Send( zero, iChunkSize)) != NO_ERROR)
        {
            break;              // there was an error
        }
    }
    pSS->FreeMem(zero);         //break to here
    return err;
} //Flush


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

DISPLAY_STATUS Printer::ParseError
(
    BYTE status_reg
)
{
    DBG1("Printer: parsing error info\n");

    DRIVER_ERROR err = NO_ERROR;
    BYTE DevIDBuffer[DevIDBuffSize];

    if(IOMode.bDevID)
    {
        // If a bi-di cable was plugged in and everything was OK, let's see if it's still
        // plugged in and everything is OK
        err = pSS->GetDeviceID(DevIDBuffer, DevIDBuffSize, TRUE);
        if(err != NO_ERROR)
        {
            // job was bi-di but now something's messed up, probably cable unplugged
            // or printer turned off during print job
            return DISPLAY_COMM_PROBLEM;
        }
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
            DBG1("IO Trap\n");
            return DISPLAY_ERROR_TRAP;
        }
    }

    if (IOMode.bDevID)
    {
        if ( TopCoverOpen(status_reg) )
        {
            DBG1("Top Cover Open\n");
            return DISPLAY_TOP_COVER_OPEN;
        }

        // VerifyPenInfo will handle prompting the user
        //  if this is a problem
        VerifyPenInfo();
    }

    // don't know what the problem is-
    //  Is the PrinterAlive?
    if (pSS->PrinterIsAlive())  // <- This is only viable if bStatus is TRUE
    {
        iTotal_SLOW_POLL_Count += iMax_SLOW_POLL_Count;

        // -Note that iTotal_SLOW_POLL_Count is a multiple of
        //  iMax_SLOW_POLL_Count allowing us to check this
        //  on an absolute time limit - not relative to the number
        //  of times we happen to have entered ParseError.
        // -Also note that we have different thresholds for uni-di & bi-di.

        // REVISIT these counts - they are relative to the speed through
        // the send loop aren't they?  They may be too long!
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
} //ParseError

DRIVER_ERROR Printer::Encapsulate
(
    const RASTERDATA* InputRaster,
    BOOL bLastPlane
)
{
    DRIVER_ERROR    err;
    char            scratch[20];
    int             scratchLen;

    if (bLastPlane)
    {
		if (VIPPrinter())
		{
			if (InputRaster->rastersize[COLORTYPE_BLACK] != 0)
			{
				if (InputRaster->rastersize[COLORTYPE_COLOR] != 0)
				{
					scratchLen = sprintf (scratch, "\033*b%uV", InputRaster->rastersize[COLORTYPE_BLACK]);
					err = Send ((const BYTE*) scratch, scratchLen);
					if (err == NO_ERROR)
					{
						err = Send (InputRaster->rasterdata[COLORTYPE_BLACK], InputRaster->rastersize[COLORTYPE_BLACK]);
					}
					if (err == NO_ERROR)
					{
						scratchLen = sprintf (scratch, "\033*b%uW", InputRaster->rastersize[COLORTYPE_COLOR]);
						err = Send ((const BYTE*) scratch, scratchLen);
						if (err == NO_ERROR)
						{
							err = Send (InputRaster->rasterdata[COLORTYPE_COLOR], InputRaster->rastersize[COLORTYPE_COLOR]);
						}
					}
				}
				else
				{
					scratchLen = sprintf (scratch, "\033*b%uV", InputRaster->rastersize[COLORTYPE_BLACK]);
					err = Send ((const BYTE*) scratch, scratchLen);
					if (err == NO_ERROR)
					{
						err = Send (InputRaster->rasterdata[COLORTYPE_BLACK], InputRaster->rastersize[COLORTYPE_BLACK]);
						if (err == NO_ERROR)
						{
							scratchLen = sprintf (scratch, "\033*b%uW", 0);
							err = Send ((const BYTE*) scratch, scratchLen);
						}
					}
				}
			}
			else
			{
				scratchLen = sprintf (scratch, "\033*b%uV", 0);
				err = Send ((const BYTE*) scratch, scratchLen);
				if (err == NO_ERROR)
				{
					scratchLen = sprintf (scratch, "\033*b%uW", InputRaster->rastersize[COLORTYPE_COLOR]);
					err = Send ((const BYTE*) scratch, scratchLen);
					if (err == NO_ERROR && InputRaster->rastersize[COLORTYPE_COLOR] != 0)
					{
						err = Send (InputRaster->rasterdata[COLORTYPE_COLOR], InputRaster->rastersize[COLORTYPE_COLOR]);
					}
				}
			}
		}
		else
		{
			scratchLen = sprintf (scratch, "\033*b%uW", InputRaster->rastersize[COLORTYPE_COLOR]);
			err = Send ((const BYTE*) scratch, scratchLen);
			if (err == NO_ERROR)
			{
				err = Send (InputRaster->rasterdata[COLORTYPE_COLOR], InputRaster->rastersize[COLORTYPE_COLOR]);
			}
		}
    }
    else
    {
        scratchLen = sprintf (scratch, "\033*b%uV", InputRaster->rastersize[COLORTYPE_COLOR]);
		err = Send ((const BYTE*) scratch, scratchLen);
		if (err == NO_ERROR)
		{
			err = Send (InputRaster->rasterdata[COLORTYPE_COLOR], InputRaster->rastersize[COLORTYPE_COLOR]);
		}
    }
    return err;
}


DRIVER_ERROR Printer::Send
(
    const BYTE* pWriteBuff
)
{
    ASSERT(pWriteBuff);
    int len = strlen((const char*)pWriteBuff);
    return Send(pWriteBuff,len);
} //Send


/*
 *  Function name: Printer::Send
 *
 *  Owner: Darrell Walker
 *
 *  Purpose:  Encapsulate error handling generated by performing I/O
 *
 *  Called by:
 *
 *  Calls made: WritePort(), GetStatus(), BusyWait(), ParseError(),
 *              DisplayPrinterStatus(), YieldToSystem()
 *
 *  Parameters on entry: pJob is a pointer to the current JOBSTRUCT,
 *                      pWriteBuff is a pointer to the data the
 *                      caller wishes to send to the pritner,
 *                      wWriteCount is the number of bytes of
 *                      pWriteBuff to send
 *
 *  Parameters on exit: Unchanged
 *
 *  Side effects: Sends data to the printer, may update print dialog,
 *              may change pJob->InSlowPollMode,
 *              pJob->ErrorTerminationState
 *
 *  Return Values: NO_ERROR or JOB_CANCELED or IO_ERROR
 *
 *  Comments: (TJL) This routine now has functionality to attempt iterating
 *      through wWriteCount bytes of data (until we hit slow poll mode)
 *      before sending PCL cleanup code.  This still leaves the possibility of
 *      prematurely exiting with an incomplete raster, but gives I/O a fighting chance
 *      of completing the raster while also protecting from a bogged down slow poll phase
 *      which would prevent a timely exit from CANCEL.  A JobCanceled flag is used
 *      because JOB_CANCELED cannot be maintained by write_error through the while
 *      loop since it will be overwritten by next ToDevice.
 *
 */

DRIVER_ERROR Printer::Send
(
    const BYTE* pWriteBuff,
    DWORD dwWriteCount
)
{
    ASSERT(pWriteBuff);

    DRIVER_ERROR    write_error = NO_ERROR;
    DWORD           residual = 0;
    const BYTE *    pWritePos = NULL;
    BOOL            error_displayed = FALSE;
    BYTE            status_reg = 0;
    DISPLAY_STATUS  eDisplayStatus = DISPLAY_PRINTING;
    BOOL            JobCanceled = FALSE;  // see comments in function header

    // these are just an abstraction layer - buffered vs. non-buffered
    const BYTE *    pBuffer = pWriteBuff;
    DWORD           dwSendSize = dwWriteCount;

////////////////////////////////////////////////////////////////
#ifdef NULL_IO
    // test imaging speed independent of printer I/O, will not
    // send any data to the device
    return NO_ERROR;
#endif
////////////////////////////////////////////////////////////////

    if (ErrorTerminationState)  // don't try any more IO if we previously
    {
        return JOB_CANCELED;    // terminated in an error state
    }

    if (EndJob == FALSE && dwWriteCount == 0)    // don't bother processing
    {
        return NO_ERROR;                         //  an empty Send call
    }

#ifdef APDK_BUFFER_SEND
    DWORD BytesToWrite = dwWriteCount;

    do
    {
        // we should bypass the buffering for a large Send, but don't lose what may already be buffered
        if ( (BytesToWrite >= iBuffSize) && (iCurrBuffSize == 0) )
        {
            pBuffer = pWriteBuff+(dwWriteCount-BytesToWrite);
            dwSendSize = BytesToWrite;
            BytesToWrite = 0;  // this is checked for at the end of the outer loop
        }
        else // we will buffer this data
        {
            // if it'll fit then just copy everything to the buffer
            if (BytesToWrite <= DWORD(iBuffSize-iCurrBuffSize))
            {
                memcpy((void*)(pSendBuffer+iCurrBuffSize),
                    (void*)(pWriteBuff+(dwWriteCount-BytesToWrite)),BytesToWrite);
               iCurrBuffSize += BytesToWrite;
               BytesToWrite = 0;
            }
              else // copy what we can into the buffer, we'll get the rest later
            {
              memcpy((void*)(pSendBuffer+iCurrBuffSize),
                (void*)(pWriteBuff+(dwWriteCount-BytesToWrite)),
                iBuffSize-iCurrBuffSize);
                BytesToWrite -= (iBuffSize-iCurrBuffSize);
                iCurrBuffSize = iBuffSize;
            }

            // if the buffer is now full (ready-to-send) or if we're at
            // the end of the job, then send what we have in the buffer.
            // otherwise just break (the buffer isn't ready to send)
            if ( (EndJob == FALSE) && (iCurrBuffSize != iBuffSize) )
            {
                break;  // we're not ready to send yet
            }
            else // send this buffered data
            {
                pBuffer = pSendBuffer;
                dwSendSize = iCurrBuffSize;
            }
		}
#endif

    // initialize our 'residual' to the full send size
    residual = dwSendSize;

    if (bCheckForCancelButton &&
        (ulBytesSentSinceCancelCheck >= CANCEL_BUTTON_CHECK_THRESHOLD) &&
        (pSS->IOMode.bDevID))
    {
        ulBytesSentSinceCancelCheck = 0;
        char* tmpStr;
        BYTE DevIDBuffer[DevIDBuffSize];
        DRIVER_ERROR tmpErr = pSS->GetDeviceID(DevIDBuffer, DevIDBuffSize, TRUE);
        if(tmpErr)
            return tmpErr;
        BOOL cancelcheck=FALSE;

        if((tmpStr = strstr((char*)DevIDBuffer + 2,"CNCL")))
        {
            cancelcheck=TRUE;
        }
        else
        {
            int     iVersion = pSS->GetVIPVersion ();

            if((tmpStr = strstr((char*)DevIDBuffer + 2,";S:")) &&
                iVersion < 6)    // DJ990 devID style
            {
                // point to PrinterState
/*
 *				VIPVersion = DevID Version + 1 - DevID Version no is 2 bytes following ;S:
 *				Version 00 and 01 report 12 bytes for status info
 *				Version 02 and onwards, report two additional bytes before pen info
 */

				if (iVersion < 3)
				{
					tmpStr += 17;   // 3 for ";S:", 2 for version, 12 for features = 17
				}
				else if (iVersion < 5)
				{
					tmpStr += 19;	// 17 as above plus 1 for MaxPaperSize, 1 reserved = 19
				}
                else
                {
                    tmpStr += 23;   // Crystal added 4 more nibbles
                }

                BYTE b1=*tmpStr++;
                BYTE b2=*tmpStr++;
                if (((b1=='0') && (b2=='5')) && iVersion <= 5)     // 05 = cancel
                {
                    cancelcheck=TRUE;
                }
            }
        }
        if (cancelcheck)
        {
            // Since the printer is now just throwing data away, we can bail
            // immediately w/o worrying about finishing the raster so the
            // end-of-job FF will work.
            ErrorTerminationState = TRUE;
            pSS->DisplayPrinterStatus(DISPLAY_PRINTING_CANCELED);
            return JOB_CANCELED;
        }
    }

    // If we have nothing to send, we need to bail to avoid spurious dialogs
    //  at the end of the ::send function.  I'd prefer a solution where we don't
    //  bail from a while loop but in practice this shouldn't have any ill effects.
    if (residual <= 0)
    {
        return NO_ERROR;
    }

    while (residual > 0)    // while still data to send in this request
    {
        DWORD prev_residual = residual;     // WritePort overwrites request
                                            // count, need to save

        pWritePos = (const BYTE *) &(pBuffer[dwSendSize-residual]);
        write_error = pSS->ToDevice(pWritePos, &residual);

        // The following error handling code is recommended, but is not supported
        // on several current platforms for this reason:  when the printer buffer
        // fills and declines more data, the O/S returns a BUSY error in one form
        // or another.  The real solution is to allow the code below and have the
        // derived pSS->ToDevice functions catch and ignore any BUSY error - only
        // returning a real error to ::Send.  The current workaround here is that we
        // literally ignore all errors returned from the I/O system and ultimately catch
        // them after we've reached our slow poll limit via the slow poll logic below. -TL
//        if(write_error != NO_ERROR)
//        {
//            DBG1("IO_ERROR returned from ToDevice - ABORT!\n");
//            ErrorTerminationState = TRUE;
//            return write_error;
//        }

        write_error = NO_ERROR;
        eDisplayStatus = DISPLAY_PRINTING;
        if (residual == 0) // no more data to send this time
        {
            if (m_bStatusByPJL)
            {
                status_reg = (BYTE) DISPLAY_PRINTING;
                if (IOMode.bStatus)
                    eDisplayStatus = ParseError (status_reg);
                if (eDisplayStatus != DISPLAY_PRINTING)
                    write_error = IO_ERROR;
            }

            // did we want to transition out of slow poll?
            if ( (InSlowPollMode != 0) &&
                (prev_residual > MIN_XFER_FOR_SLOW_POLL) )
            {
                InSlowPollMode = 0;
                iTotal_SLOW_POLL_Count = 0;
            }
            if (write_error == NO_ERROR)
                break;  // out of while loop
        }


        // if we are here, WritePort() was not able to
        // send the full request so start looking for errors

        // decide whether we've waited long enough to check for an error
        if (InSlowPollMode > iMax_SLOW_POLL_Count )
        {
            if (JobCanceled == TRUE)
            // Well, I/O didn't finish in time to meet the CANCEL request and avoid
            // the SlowPoll threshold.  We have to bail for prompt cancel response.
            {
                DBG1("Send(SlowPoll): Premature return w/ JOB_CANCELED\n");
                ErrorTerminationState = TRUE;
                return JOB_CANCELED;
            }

            DBG1("Printer slow poll times exceeded\n");
            // reset counter so we will not pop it next time
            InSlowPollMode = 1;
            write_error = IO_ERROR;
        }
        else
        {
            write_error = NO_ERROR;
        }

        // are we in slow poll mode?  If so, track our count
        if ( (prev_residual - residual) <= MIN_XFER_FOR_SLOW_POLL)
        {
            InSlowPollMode++;

#if defined(DEBUG) && (DBG_MASK & DBG_LVL1)
            if (InSlowPollMode == 1)
            {
                printf("entering slow poll mode\n");
            }
            else
            {
                printf("still in slow poll mode, count = %d\n",
                                    InSlowPollMode);
            }
#endif
            // give the printer some time to process
            if (pSS->BusyWait((DWORD)200) == JOB_CANCELED)
            {
                DBG1("Send: JOB_CANCELED\n");
                JobCanceled = TRUE;
                pSS->DisplayPrinterStatus(DISPLAY_PRINTING_CANCELED);
            }
        }
        else
        {
            // still busy, but taking enough data that
            // we are not in slow poll mode
            DBG1("Partial Send but not slow poll mode\n");
            InSlowPollMode = 0;
            iTotal_SLOW_POLL_Count = 0;
        }


        if (write_error != NO_ERROR || (m_bStatusByPJL && eDisplayStatus != DISPLAY_PRINTING))
        // slow poll times exceeded
        // the printer isn't taking data so let's see what's wrong...
        {
            DBG1("Parsing possible error state...\n");
            error_displayed = TRUE;

            if (m_bStatusByPJL)
            {
                status_reg = (BYTE) eDisplayStatus;
            }
            else
            {
                // go get the status of the printer
                if (IOMode.bStatus)
                {
                    pSS->GetStatusInfo(&status_reg);
                }

                // determine the error
                eDisplayStatus = ParseError(status_reg);
            }

            switch (eDisplayStatus)
            {
                case DISPLAY_PRINTING_CANCELED:

                        // user canceled in an error state,
                        // so we don't want to attempt any
                        // further communication with the printer

                        ErrorTerminationState = TRUE;
                        pSS->DisplayPrinterStatus(eDisplayStatus);
                        return JOB_CANCELED;

                case DISPLAY_ERROR_TRAP:
                case DISPLAY_COMM_PROBLEM:
                        // these are unrecoverable cases
                        // don't let any more of this job
                        // be sent to the printer

                        ErrorTerminationState = TRUE;
                        pSS->DisplayPrinterStatus(eDisplayStatus);

                        // wait for user to cancel the job,
                        // otherwise they might miss the
                        // error message
                        while (pSS->BusyWait(500) != JOB_CANCELED)
                        {
                                // nothing....
                            ;
                        }
                        return IO_ERROR;

                case DISPLAY_TOP_COVER_OPEN:

                    pSS->DisplayPrinterStatus(DISPLAY_TOP_COVER_OPEN);

                    // wait for top cover to close
                    while ( eDisplayStatus == DISPLAY_TOP_COVER_OPEN)
                    {
                        if (pSS->BusyWait((DWORD)500) == JOB_CANCELED)
                        // although we'll leave an incomplete job in the printer,
                        // we really need to bail for proper CANCEL response.
                        {
                            ErrorTerminationState = TRUE;
                            return JOB_CANCELED;
                        }

                        if (m_bStatusByPJL)
                        {
                            status_reg = (BYTE) eDisplayStatus;
                        }
                        else
                        {
                            if (IOMode.bStatus)
                            {
                                pSS->GetStatusInfo(&status_reg);
                            }
                        }

                        eDisplayStatus = ParseError(status_reg);
                    }

                    pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

                    // Wait for printer to come back online
                    if (pSS->BusyWait((DWORD)1000) == JOB_CANCELED)
                    // Since the top_cover error HAS been handled, we have
                    // the opportunity to finish the raster before we hit
                    // the next slowpoll threshold.
                    {
                        DBG1("Send: JOB_CANCELED\n");
                        JobCanceled = TRUE;
                        pSS->DisplayPrinterStatus(DISPLAY_PRINTING_CANCELED);
                    }

                    break;

                case DISPLAY_OUT_OF_PAPER:
				case DISPLAY_PHOTOTRAY_MISMATCH:
				{
					DISPLAY_STATUS	tmpStatus = eDisplayStatus;
                    pSS->DisplayPrinterStatus(eDisplayStatus);

                    // wait for the user to add paper and
                    // press resume
                    while ( eDisplayStatus == tmpStatus)
                    {
                        if (pSS->BusyWait((DWORD)500) == JOB_CANCELED)
                        // although we'll leave an incomplete job in the printer,
                        // we really need to bail for proper CANCEL response.
                        {
                            ErrorTerminationState = TRUE;
                            return JOB_CANCELED;
                        }

                        if (m_bStatusByPJL)
                        {
                            status_reg = (BYTE) eDisplayStatus;
                        }
                        else
                        {
                            if (IOMode.bStatus)
                            {
                                pSS->GetStatusInfo(&status_reg);
                            }
                        }

                        eDisplayStatus = ParseError(status_reg);
                    }

                    pSS->DisplayPrinterStatus(DISPLAY_PRINTING);

                    break;
				}

                case DISPLAY_BUSY:

                    if (pSS->BusyWait((DWORD)5000) == JOB_CANCELED)
                    {
                        ErrorTerminationState = TRUE;
                        return JOB_CANCELED;
                    }

                    pSS->DisplayPrinterStatus(DISPLAY_BUSY);

                    break;

                // other cases need no special handling, display
                // the error and try to continue
                default:
                    pSS->DisplayPrinterStatus(eDisplayStatus);
                    break;
            }// switch
        } // if

        // give printer time to digest the data and check for 'cancel' before
        // the next iteration of the loop
        if (pSS->BusyWait((DWORD)100) == JOB_CANCELED)
        {
            DBG1("Send: JOB_CANCELED\n");
            JobCanceled = TRUE;
            pSS->DisplayPrinterStatus(DISPLAY_PRINTING_CANCELED);
        }

    }   // while (residual > 0)

    // The above BusyWait's will not be checked if residual gets sent the first time, every time
    // because we 'break' at that point for efficiency.  However, we want to make sure we check
    // at least once for a CANCEL event for timely job-cancel response.
    if (pSS->BusyWait((DWORD)0) == JOB_CANCELED)
    {
        JobCanceled = TRUE;
        pSS->DisplayPrinterStatus(DISPLAY_PRINTING_CANCELED);
    }

#ifdef APDK_BUFFER_SEND
    // Our buffer is now empty: reset the size so concurrent writes start at the beginning
        iCurrBuffSize = 0;

    } while (BytesToWrite > 0);
#endif

    // restore my JOB_CANCELED error
    if (JobCanceled == TRUE)
    {
        DBG1("Send: Clean return w/ JOB_CANCELED\n");
        // ensure that display still says we're cancelling
        pSS->DisplayPrinterStatus(DISPLAY_PRINTING_CANCELED);
        return JOB_CANCELED;
    }
    else
    {
        // ensure any error message has been cleared
        pSS->DisplayPrinterStatus(DISPLAY_PRINTING);
        if (bCheckForCancelButton)
        {
            ulBytesSentSinceCancelCheck += dwWriteCount;
        }
        return NO_ERROR;
    }
} //Send


BOOL Printer::TopCoverOpen
(
    BYTE /*status_reg*/
)
{
    char * pStr;
    BYTE bDevIDBuff[DevIDBuffSize];

    if(IOMode.bDevID == FALSE)
    {
        return FALSE;
    }

    DRIVER_ERROR err = pSS->GetDeviceID(bDevIDBuff, DevIDBuffSize, TRUE);
    if (err != NO_ERROR)
    {
        return FALSE;
    }

    if( (pStr=strstr((char*)bDevIDBuff+2,"VSTATUS:")) ) //  find the VSTATUS area
    {
        pStr+=8;
        // now parse VSTATUS parameters
        // looking for UP for open, DN for closed
        if (strstr((char*)pStr,"UP"))
        {
            return TRUE;
        }
        if (strstr((char*)pStr,"DN"))
        {
            return FALSE;
        }

        DBG1("didn't find UP or DN!!\n");
        return FALSE;
    }
    else
    if (( pStr = strstr ((char*) bDevIDBuff+2, ";S")))
    {
        if ( (*(pStr+5) == '9') )
        {
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        return FALSE;  // if we can't find VSTATUS or binary status field, assume top is not open
    }
} //TopCoverOpen


DRIVER_ERROR Printer::CleanPen()
{
    DBG1("Printer::ClearPen() called\n");

    DWORD length=sizeof(PEN_CLEAN_PML);
    return pSS->ToDevice(PEN_CLEAN_PML,&length);
} //CleanPen


PrintMode* Printer::GetMode
(
    unsigned int index
)
{
    if (index >= ModeCount)
    {
        return NULL;
    }

    return pMode[index];
} //GetMode


PrintMode::PrintMode
(
    uint32_t *map1,
    uint32_t *map2
)
{
    pmQuality = QUALITY_NORMAL;
    pmMediaType = MEDIA_PLAIN;
    pmColor = COLOR;
    int iCount;

    cmap.ulMap1 = map1;
    cmap.ulMap2 = map2;
    cmap.ulMap3 = NULL;

    BaseResX = BaseResY = TextRes = 300;
    MixedRes= FALSE;

    // default setting
    for (iCount = 0; iCount < MAXCOLORPLANES; iCount++)
    {
        ResolutionX[iCount] = BaseResX;
        ResolutionY[iCount] = BaseResY;
        ColorDepth[iCount] = 1;
    }

    medium = mediaPlain;
    theQuality = qualityNormal;
    dyeCount=4;

    Config.bResSynth = TRUE;

#if defined(APDK_VIP_COLORFILTERING)
    Config.bErnie = FALSE;
#endif

    Config.bPixelReplicate = TRUE;
    Config.bColorImage = TRUE;
    Config.bCompress = TRUE;
    Config.eHT = FED;
    BlackFEDTable = GetHTBinary();
    ColorFEDTable = GetHTBinary();


    // set for most common cases
    bFontCapable = TRUE;
    CompatiblePens[0] = BOTH_PENS;
    for(iCount = 1; iCount < MAX_COMPATIBLE_PENS; iCount++)
    {
        CompatiblePens[iCount] = DUMMY_PEN;
    }

#ifdef APDK_AUTODUPLEX
    bDuplexCapable = FALSE;
    DuplexMode = DUPLEXMODE_NONE;
#endif

} //PrintMode


GrayMode::GrayMode
(
    uint32_t *map
) :
    PrintMode(map)
// grayscale uses econo, 300, 1 bit
{
    ColorDepth[K] = 1;
    dyeCount = 1;
    CompatiblePens[1] = BLACK_PEN;  // accept either black or both
    pmColor = GREY_K;
} //GrayMode


CMYGrayMode::CMYGrayMode
(
    uint32_t *map
) :
    GrayMode(map)
{
    CompatiblePens[1] = COLOR_PEN;  // accept either color or both
    dyeCount = 3;
    pmColor = GREY_CMY;
} //CMYGrayMode


KCMYGrayMode::KCMYGrayMode
(
    uint32_t *map
) :
    GrayMode(map)
{
    dyeCount = 4;
    pmColor = GREY_CMY;
} //KCMYGrayMode


DRIVER_ERROR Printer::SetPenInfo
(
    char*& pStr,
    BOOL QueryPrinter
)
{
    DRIVER_ERROR err;

    if (QueryPrinter)
    {
        // read the DevID into the stored strDevID
        err = pSS->GetDeviceID(pSS->strDevID, DevIDBuffSize, TRUE);
        ERRCHECK;

        // update the static values of the pens
        err = pSS->DR->ParseDevIDString((const char*)(pSS->strDevID),pSS->strModel,&(pSS->VIPVersion),pSS->strPens);
        ERRCHECK;

        if ((pStr = strstr((char*)pSS->strDevID,"VSTATUS:")))   //  find the VSTATUS area
        {
            pStr += 8;
        }
        else if ((pStr = strstr((char*)pSS->strDevID, ";S:00")))  // binary encoded device ID status (version 0)
        {
            pStr += 19;     // get to the number of pens field - 12 byte feature field
        }
        else if ((pStr = strstr ((char *) pSS->strDevID, ";S:01")))
        {
            pStr += 19;     // same as version 00
        }
        else if ((pStr = strstr((char*)pSS->strDevID, ";S:02")))  // binary encoded device ID status (version 2)
        {
//            pStr += 21;     // get to the number of pens field - 14 byte feature field
            pStr += 19;     // same as version 00 - see registry.cpp
        }
        else if ((pStr = strstr((char*)pSS->strDevID, ";S:03")))  // binary encoded device ID status (version 3)
        {
            pStr += 21;     // get to the number of pens field - 14 byte feature field
        }
        else if ((pStr = strstr((char*)pSS->strDevID, ";S:04")))  // binary encoded device ID status (version 3)
        {
            pStr += 25;     // get to the number of pens field - 18 byte feature field
        }
        else if ((pSS->GetVIPVersion ()) > 5)
        {
            return NO_ERROR;
        }
        else
        {
            TRACE("Printer::SetPenInfo - Unsupported DeviceID %s.\n", pSS->strDevID);
/*            ASSERT (0);  // you must have a printer with a new version that is not supported yet! */
            return BAD_DEVICE_ID;  // - code should never reach this point
        }
    }
    else
    {
        pStr = pSS->strPens;
    }
    return NO_ERROR;
} //SetPenInfo


BOOL PrintMode::Compatible
(
    PEN_TYPE pens
)
{
    BOOL res = FALSE;
    for (int i=0; i < MAX_COMPATIBLE_PENS; i++)
    {
        if (CompatiblePens[i] == pens)
        {
            res = TRUE;
        }
    }
    return res;
} //Compatible



DRIVER_ERROR Printer::SetPens
(
    PEN_TYPE eNewPen
)
{
    ASSERT(eNewPen <= MAX_PEN_TYPE);
    // We are (probably)in unidi.  We have to trust they know what pens are
    // in the printer.  We'll let them set any pen set (even if this printer
    // doesn't support it.  We'll find out during SelectPrintMode
    if (eNewPen <= MAX_PEN_TYPE)
    {
        ePen = eNewPen;
        return NO_ERROR;
    }
    else
    {
        return UNSUPPORTED_PEN;
    }
} //SetPens

void PrintMode::GetValues
(
    QUALITY_MODE& eQuality,
    MEDIATYPE& eMedia,
    COLORMODE& eColor,
    BOOL& bDeviceText
)
{
    if (&eQuality != NULL)
    {
        eQuality = pmQuality;
    }

    if (&eMedia != NULL)
    {
        eMedia = pmMediaType;
    }

    if (&eColor != NULL)
    {
        eColor = pmColor;
    }

    if (&bDeviceText != NULL)
    {
        bDeviceText = bFontCapable;
    }
} //GetValues


void Printer::SetPMIndices()
{
    for (unsigned int i=0; i < ModeCount; i++)
    {
        pMode[i]->myIndex = i;
    }
} //SetPMIndices

APDK_END_NAMESPACE


