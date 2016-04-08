/*****************************************************************************\
  systemservice.cpp : Implimentation for the SystemServices class

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
#ifdef APDK_CAPTURE
#include "script.h"
#endif // APDK_CAPTURE

APDK_BEGIN_NAMESPACE

// Constructor instantiates the DeviceRegistry.
// Real work done in InitDeviceComm, called from derived
// class constructor.
/*!
This method constructs a SystemServices object. This is the first step in the
calling sequence.
Check constructor_error before continuing.
*/
SystemServices::SystemServices()
    : constructor_error(NO_ERROR),
#if defined(APDK_CAPTURE)
    pScripter(NULL),
    Capturing(FALSE),
#endif
    iSendBufferSize(4096)
{

    strModel[0] = strPens[0] = '\0';
    VIPVersion = 0;

    DR = new DeviceRegistry();
    // DR can't fail

    IOMode.bDevID=FALSE;
    IOMode.bStatus=FALSE;
    IOMode.bUSB=FALSE;
    memset (strDevID, 0, DevIDBuffSize);

}

/*!
This method is a destructor, called when the instance of SystemServices is
deleted or goes out of scope.
*/
SystemServices::~SystemServices()
{

    delete DR;

#if defined(APDK_CAPTURE)
    if (pScripter)
        delete pScripter;
#endif

DBG1("deleting SystemServices\n");
}

/*!
Tests communication with printer.  It calls host supplied GetStatusInfo.
\return TRUE if communication with printer is working.
\note This implementation is appropriate for Parallel bus only.
*/
BOOL SystemServices::PrinterIsAlive()
{
    BYTE status_reg;

    // Technically, this function should not even be
    // called if IOMode.bStatus is known to be FALSE
    if (!IOMode.bStatus)
        return TRUE;
    if( GetStatusInfo(&status_reg) == FALSE )
    {
        DBG1("PrinterIsAlive: No Status-Byte Available (Default = TRUE)\n");
        return TRUE;
    }

#define DJ6XX_OFF       (0xF8)
#define DJ400_OFF       (0xC0)
// sometimes the DJ400 reports a status byte of C8 when it's turned off
#define DJ400_OFF_BOGUS (0xC8)
#define DEVICE_IS_OK(reg) (!((reg == DJ6XX_OFF) || (reg == DJ400_OFF) || (reg == DJ400_OFF_BOGUS)))

#if defined(APDK_DEBUG) && (DBG_MASK & DBG_LVL1)
    printf("status reg is 0x%02x\n",status_reg);
    if (DEVICE_IS_OK(status_reg))
        DBG1("PrinterIsAlive: returning TRUE\n");
    else
        DBG1("PrinterIsAlive: returning FALSE\n");
#endif

    return (DEVICE_IS_OK(status_reg));
}

/*!
 Same as host-supplied FreeMem, with extra safety check
 for null pointer.
*/
DRIVER_ERROR SystemServices::FreeMemory(void *ptr)
{
    if (ptr == NULL)
        return SYSTEM_ERROR;

//      printf("FreeMemory freeing %p\n",ptr);
    FreeMem((BYTE*)ptr);

 return NO_ERROR;
}


/*!
This method tries to get the device id back from the printer and does some basic verification.
*/
DRIVER_ERROR SystemServices::GetDeviceID(BYTE* strID, int iSize, BOOL bQuery)
{

    if (iSize < 3)  // must have at least enough space for count bytes and NULL terminator
    {
        return(SYSTEM_ERROR);
    }

	memset (strID, 0, iSize);

    if (bQuery)
    {
        // initialize the first 3 bytes to NULL (1st 2 bytes may be binary count of string
        // length so need to clear them as well as the "real" start of the string)
        // so that if ReadDeviceID() does nothing with buffer we won't act upon what
        // was in the buffer before calling it
        strID[0] = strID[1] = strID[2] = '\0';

        // we are going to try more then once because some printers lie and this
        // specifically fixes problems with the DJ630 & DJ640 printers.
        int i = 0;
        for(i = 0; i < 20; i++)
        {
            // get the string
            if((ReadDeviceID(strID, iSize) != NO_ERROR))
            {
                DBG1("Error from ReadDeviceID or No DevID Available\n");
                if(BusyWait((DWORD)100) == JOB_CANCELED)
                {
                    return JOB_CANCELED;
                }
                continue;  // go back and try again
            }
            // look for the existence of either of the defined manufacturer fields in the string
            // (need to look starting at strID[0] and at strID[2] since the first 2 bytes may or
            // may not be binary count bytes, one of which could be a binary 0 (NULL) which strstr()
            // will interpret as the end of string)
            else
            {
                if ((!strstr((const char*)strID, "MFG:") &&
                    !strstr((const char*)strID+2, "MFG:") &&
                    !strstr((const char*)strID, "MANUFACTURER:") &&
                    !strstr((const char*)strID+2, "MANUFACTURER:")) ||
                    (!strstr((const char*)strID, "MDL:") &&
                    !strstr((const char*)strID+2, "MDL:") &&
                    !strstr((const char*)strID, "MODEL:") &&
                    !strstr((const char*)strID+2, "MODEL:")) ||
                    ((strID[0] == '\0') && (strID[1] == '\0')))
                {
                    DBG1("Successful' DevID request was a lie.  Retry...waiting 100 ms\n");
                    if(BusyWait((DWORD)100) == JOB_CANCELED)
                    {
                        return JOB_CANCELED;
                    }
                    continue;  // go back and try again
                }
                else
                {
                    // If either of the first two bytes is 0, byte count is there, replace them.

                    if (strID[0] == 0 || strID[1] == 0)
                    {
                        strID[0] = strID[1] = ' ';
                    }
                    //DBG1("HPPCL: ReadDeviceID [%hs]\n", strID+2);
                    break; // SUCCESS!
                }
            }
        }
        if(i >= 20)
        {
            return BAD_DEVICE_ID;
        }
    }
    else
    {
        // for use when string doesn't have to be re-fetched from printer

        if (DevIDBuffSize > iSize)
        {
            return SYSTEM_ERROR;
        }

        // the first 2 bytes may be binary so could be 0 (NULL) so can't use strcpy
        // (could get strlen of strDevID if start @ strDevID+2 and then add 2
        //  if do this it wouldn't require that caller's buffer be >=
        //  DevIDBuffSize, only that is it longer that actual devID string read)
        memcpy(strID, strDevID, DevIDBuffSize);
    }
    return NO_ERROR;

    // This is old code from before the 630, 640 loop fix was done (above).  This can
    // eventually be removed.
    // check the read (or copied) DeviceID string for validity

    // check what may be the binary count of the string length (some platforms return
    // the raw DeviceID in which the 1st 2 bytes are a binary count of the string length,
    // other platforms strip off these count bytes)
    // if they are a binary count they shouldn't be zero, and if they aren't a binary
    // count they also shouldn't be zero (NULL) since that would mean end of string
/*    if ((strID[0] == '\0') && (strID[1] == '\0'))
    {
        return BAD_DEVICE_ID;
    }

    // look for the existence of either of the defined manufacturer fields in the string
    // (need to look starting at strID[0] and at strID[2] since the first 2 bytes may or
    //  may not be binary count bytes, one of which could be a binary 0 (NULL) which strstr()
    //  will interpret as the end of string)
    if (!strstr((const char*)strID, "MFG:") &&
        !strstr((const char*)strID+2, "MFG:") &&
        !strstr((const char*)strID, "MANUFACTURER:") &&
        !strstr((const char*)strID+2, "MANUFACTURER:"))
    {
        return BAD_DEVICE_ID;
    }*/

} //GetDeviceID


/*!
Mandatory call to be inserted in derived constructor.
This method tries to establish communications with printer and identify it.
The derived SystemServices constructor must call this base-class routine.
*/
DRIVER_ERROR SystemServices::InitDeviceComm()
// Must be called from derived class constructor.
// (Base class must be constructed before system calls
//  below can be made.)
// Opens the port, looks for printer and
// dialogues with user if none found;
// then attempts to read and parse device ID string --
// if successful, sets IOMode.bDevID to TRUE (strings stored
// for retrieval by PrintContext).
// Returns an error only if user cancelled. Otherwise
// no error even if unidi.
//
// Calls: OpenPort,PrinterIsAlive,DisplayPrinterStatus,BusyWait,
//   GetDeviceID,DeviceRegistry::ParseDevIDString.
// Sets:    hPort,IOMode, strModel, strPens
{
    DRIVER_ERROR err = NO_ERROR;
    BOOL ErrorDisplayed = FALSE;
    BYTE temp;

    // Check whether this system supports passing back a status-byte
    if( GetStatusInfo(&temp) == FALSE )
    {
        DBG1("InitDeviceComm:  No Status-Byte Available\n");
    }
    else IOMode.bStatus = TRUE;

    // Check whether we can get a DeviceID - this may
    // still fail if the device is just turned off
    err = GetDeviceID(strDevID, DevIDBuffSize, TRUE);

    if ( err == NO_ERROR )
    {
        DBG1("InitDeviceComm:  DevID request successful\n");
        IOMode.bDevID = TRUE;
    }


    // PrinterIsAlive is arbitrary if we can't get the status-byte.
    // This check is also critical so a true uni-di system does not sit
    // in a loop informing the user to turn on the printer.
    if ( IOMode.bStatus == TRUE )
    {
        // Make sure a printer is there, turned on and connected
        // before we go any further.  This takes some additional checking
        // due to the fact that the 895 returns a status byte of F8 when
        // it's out of paper, the same as a 600 when it's turned off.
        // 895 can get a devID even when 'off' so we'll key off that logic.
        if ( (err != NO_ERROR) && (PrinterIsAlive() == FALSE) )
        {
            // Printer is actually turned off
            while(PrinterIsAlive() == FALSE)
            {
                DBG1("PrinterIsAlive returned FALSE\n");
                ErrorDisplayed = TRUE;
                DisplayPrinterStatus(DISPLAY_NO_PRINTER_FOUND);

                if(BusyWait(500) == JOB_CANCELED)
                    return JOB_CANCELED;
            }
            if(ErrorDisplayed == TRUE)
            {
                DisplayPrinterStatus(DISPLAY_PRINTING);
                // if they just turned on/connected the printer,
                // delay a bit to let it initialize
                if(BusyWait(2000) == JOB_CANCELED)
                    return JOB_CANCELED;

                err = GetDeviceID(strDevID, DevIDBuffSize, TRUE);
                if ( err == NO_ERROR )
                {
                    DBG1("InitDeviceComm:  DevID request successful\n");
                    IOMode.bDevID = TRUE;
                }
            }
        }
        // else... we have 8xx/9xx with an out-of-paper error
        // which we will catch in the I/O handling

    }

    if (err!=NO_ERROR)
    {
        DBG1("InitDeviceComm:  No DeviceID Available\n");
        return NO_ERROR;
    }

    err = DR->ParseDevIDString((const char*)strDevID, strModel, &VIPVersion, strPens);

    if (err!=NO_ERROR)
    {
        // The DevID we got is actually garbage!
        DBG1("InitDeviceComm:  The DevID string is invalid!\n");
        IOMode.bDevID=FALSE;
    }

    return NO_ERROR;
}


/*!
This function will open an ECP I/O channel to the printer and retrieve a
given number of bytes from it. Because ECP is a 1284 protocol, this
function is only relevant for 1284 compliant parallel I/O connectivity.
Currently, only the DeskJet 4xx Series of printers requires implementation
of this function. Because of the non-standard nature of this function, it
is expected that the sample code supplied in ECPSample.cpp will be heavily
leveraged during implementation of this function in the derived SystemServices
class.

\note  The DJ400 & DJ540 code was written as a special for a specific host.
DJ400 & DJ500 are not supported by the APDK and there is no  help with
communication issues with these printers.

\param pStatusString The destination of the set of retrieved status bytes.
\param pECPLength The number of retrieved bytes.
\param ECPChannel The ECP channel number to be opened and read.
*/
DRIVER_ERROR SystemServices::GetECPStatus(BYTE *pStatusString,int *pECPLength, int ECPChannel)
{
    pStatusString = NULL;
    *pECPLength = 0;

    return UNSUPPORTED_FUNCTION;
}

//! reconcile printer's preferred settings with reality
void SystemServices::AdjustIO(IO_MODE IM, const char* model)
{
    IOMode.bStatus=IM.bStatus && IOMode.bStatus;
    IOMode.bDevID =IM.bDevID  && IOMode.bDevID;

    if (model)
        strncpy(strModel,model, sizeof(strModel));
}

APDK_END_NAMESPACE

