/*****************************************************************************\
  systemservices.h : Interface for the SystemServices class

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


#ifndef APDK_SYSTEMSERVICES_H
#define APDK_SYSTEMSERVICES_H

APDK_BEGIN_NAMESPACE

struct IO_MODE
{
 //NO_FEEDBACK, STATUS_ONLY, BIDI, USB
    BOOL bDevID;
    BOOL bStatus;
    BOOL bUSB;
}; //IO_MODE


//forward declarations
class DeviceRegistry;
//class RasterSender;
class Scripter;

const int DevIDBuffSize = 512;       //!< size of buffer used by SetDevInfo

//SystemServices
//! Provides interface to host environment
/*! \class SystemServices systemservices.h "hpprintapi.h"
The SystemServices object is used to encapsulate memory-management, I/O,
clock access, user notification and UI, and other utilities provided by the
operating system or host system supporting the driver. It is an abstract base
class, requiring implementation of the necessary routines by the host.
Creation of SystemServices is the first step in the calling sequence for the
driver, followed by creation of the PrintContext and the Job. The derived
class constructor must include a call to the member function InitDeviceComm,
which establishes communication with the printer if possible.
The only reference to this object in API-calling code should be to its
constructor and destructor, which must be invoked prior to and after all other
driver calls.
******************************************************************************/
class SystemServices
{
friend class PrintContext;
friend class Printer;   // for saved device strings
#ifdef APDK_APOLLO2XXX
friend class Apollo2xxx;    // it needs to check the specific model string
#endif
friend class Tester;
friend class AsciiScripter;
friend class BinaryScripter;
public:
    SystemServices();
    virtual ~SystemServices();

    /*!
    Check this member variable for the validity of the constructed object.
    Calling code must check that the value of this variable is NO_ERROR before
    proceeding.
    \sa DRIVER_ERROR
    */
    DRIVER_ERROR constructor_error;

    //! Must include in derived class constructor (if using bi-di)
    DRIVER_ERROR InitDeviceComm();

    /////////////////////////////////////////////////////////////////////
    IO_MODE IOMode;

    //! Passes a command to the I/O system to send all data to the printer immediately.
    /*!
    A typical I/O system either buffer data or send data immediatly.  If the I/O
    system is buffering data then typically store all data sent in a buffer until it
    has some amount of data (say 1, 2, or 4 Kbytes) then will send that block of data
    to the printer. In some cases, it may be necessary that data that has just been
    sent to the I/O system reach the printer ASAP, at which point the printer driver
    will check the status or deviceID string of the printer and continue. Often,
    USB implementations will favor the block-send mechanism because it is more
    efficient through the system's call stack. Simple and traditional centronics
    I/O is usually not buffered, but this is still a system-by-system I/O preference.

    If the I/O system is buffering data into blocks before sending to the printer,
    this command should pass a command to the I/O system to stop waiting and send
    everything it has to the printer now. The base implimentation simply returns
    NO_ERROR and is appropriate for a system that does not buffer I/O.

    \note  The appropriate way to handle a FlushIO on a buffered system is to call
    the USB class driver's I/O Control routine with a flush command.  If the USB
    class driver has no flush command then check to see if it has timed flush
    (it auto-flushes every n [milli]seconds).  If it does then the base FlushIO
    method will work okay.

    \warning In the past some have implemented flush by sending 10K nulls to the USB
    driver.  Currently this does work and will cause data already in the buffer
    to be sent to the printer.  Current printers ignore the null, however, there
    may be printers in the future that cannot handle this behavior.
    */
    virtual DRIVER_ERROR FlushIO() { return NO_ERROR; }

    //! Tells the I/O system that the print job has ended prematurely.
    /*!
    This routine instructs the I/O system that this print job has ended
    prematurely, that no further data will be sent and that any data being held
    in a "send" buffer can be thrown away. This command is optional and depends
    on the implementation of the I/O system.  In some error conditions, the
    printer is incapable of accepting more data. The printer driver will sense
    this error and stop sending data, but if the I/O system has buffered data
    it may continue to wait for the printer to accept this remaining data
    before unloading. This mechanism will allow for a more efficient clean up
    of the aborted job.
    */
    virtual DRIVER_ERROR AbortIO() { return NO_ERROR; }

    /*!
    Uses message code to relay a message to the user. Must be implemented in
    derived class provided by developer. This routine will cause a corresponding
    text string (or graphic, or both) to be displayed to the user to inform them
    of printer status. This is how printer events such as out-of-paper will be
    communicated to the user.
    */
    virtual void DisplayPrinterStatus (DISPLAY_STATUS ePrinterStatus)=0;

    /*!
    Must be implemented in derived class. Waits for the specified number of
    milliseconds before returning to the caller. Gives host system a chance to
    process asynchronous events.  Derived implimentation of BusyWait MUST allow
    the processing of asynchronous events such as user interface changes, response
    to dialogs or pop-ups.  The BusyWait method should return NO_ERROR if we should
    continue processing.  The APDK core code calls busy wait within loops (when
    the printer is out of paper, has no pen, etc...) and will continue when the
    user has fixed the problem.  If the user wants to cancel the job then the
    implemented version of BusyWait must return JOB_CANCELED so that the APDK
    stops retrying and cancels the job.
    */
    virtual DRIVER_ERROR BusyWait(DWORD msec)=0;

    /*!
    Retrieves the device identifier string from the printer.

    \param strID Pointer to buffer for storing string.
    \param iSize The size of buffer.
    \return IO_ERROR
    */
    virtual DRIVER_ERROR ReadDeviceID(BYTE* strID, int iSize)=0;

    /*!
    This method will allocate a contiguous block of memory at least iMemSize bytes long.

    \param iMemSize - size of block.
    \return Pointer to the memory block. Can also return NULL, if allocation failed.
    */
    virtual BYTE* AllocMem (int iMemSize)=0;

    /*!
    Frees a given block of memory previously allocated with AllocMem.

    \param pMem - pointer to the block of memory to be freed.
    */
    virtual void FreeMem (BYTE* pMem)=0;

    virtual BOOL PrinterIsAlive();

    /*!
    This method reads status register from printer. If the parallel status byte
    is returned, the function returns TRUE when it sets this value. Otherwise
    the function returns FALSE if this functionality is not supported.

    \param bStatReg Pointer to a byte into which status register will be copied.

    \return TRUE The value is set after returning the parallel status byte,
    FALSE if the functionality is not supported.
    */
    virtual BOOL GetStatusInfo(BYTE* bStatReg)=0;

    /*!
    Sends bytes of data to the printer.
    \note This method decrements the dwCount parameter (*dwCount) by the number
    of bytes that were actually sent.

    \param pBuffer pointer to buffer full of data.
    \param dwCount (in/out) Upon entry, this contains the number of bytes of
    data requested to send. Upon return from the method, dwCount is the number
    of bytes that were not sent, i.e. the remaining bytes.
    */
    virtual DRIVER_ERROR ToDevice(const BYTE* pBuffer, DWORD* dwCount)=0;

    /*!
    Gets data from printer.

    \param pReadBuff Pointer to buffer into which data is to be copied.
    \param wReadCount Output parameter (pointer to allocated DWORD) telling
    number of bytes copied.
    */
    virtual DRIVER_ERROR FromDevice(BYTE* pReadBuff, DWORD* wReadCount)=0;

    virtual DRIVER_ERROR GetECPStatus(BYTE *pStatusString,int *pECPLength, int ECPChannel);

    /*!
    This routine will query the system for the current tick count (time).
    Since it will be used primarily for retry time-outs in the driver, it only
    needs to provide a running count of elapsed time since the device was booted,
    for example.

    \return DWORD Number of ticks for current tick count.
    */
    virtual DWORD GetSystemTickCount (void)=0;

    virtual float power(float x, float y)=0;

    /*!
    Returns the model portion of the firmware ID string from the printer.
    */
    const char* PrinterModel() { return strModel; }

	/*!
	Returns the size of the send buffer.
	*/
    unsigned int GetSendBufferSize() const { return iSendBufferSize; }

#if defined (APDK_DJ3320)
    /*!
    This method return the reads Crossbow/Spear vertical alignment value from printer. 
	If the vertical alignment value is set by the user from host, 
	the function returns TRUE. Otherwise the function returns FALSE if this functionality 
	is not supported.

    \param cVertAlignVal Pointer to a byte into which vertical alignment value will be copied.

    \return TRUE The value is set after returning the vertical alignment value,
    FALSE if the functionality is not supported.
    */
    virtual BOOL GetVerticalAlignmentValue(BYTE* cVertAlignVal) { return FALSE; }

    /*!
    This method reads Crossbow/Spear vertical alignment value from the device during
    printer instantiation. The value is saved for later calls by GetVerticalAlignmentValue. 

    \return TRUE valid value read,
    FALSE invalid value or not supported.
    */

    virtual BOOL GetVertAlignFromDevice() { return FALSE; }
#endif

// utilities ///////////////////////////////////////////////////////

    // call FreeMem after checking for null ptr
    DRIVER_ERROR FreeMemory(void *ptr);
    DRIVER_ERROR GetDeviceID(BYTE* strID, int iSize, BOOL query);

	/*!
	Return the VIP version of the firmware ID from the printer.
	*/
    int    GetVIPVersion () { return VIPVersion; }

#if defined(APDK_CAPTURE)
    Scripter *pScripter;
    /*!
    Begin recording script for debugging.
    \param FileName Name of the output file which will hold the script.
    \param ascii If TRUE use ASCII implementation. Otherwise, use binary.
    */
    DRIVER_ERROR InitScript(const char* FileName, BOOL ascii, BOOL read=FALSE);

	/*!
    Stop recording script for debugging.
	*/
    DRIVER_ERROR EndScript();

    BOOL Capturing;
    BOOL replay;
#endif

    virtual int GetPJLHeaderBuffer (char **szPJLBuffer)
    {
        return 0;
    }

protected:
    virtual void AdjustIO(IO_MODE IM, const char* model = NULL);

    BYTE strDevID[DevIDBuffSize]; // save whole DevID string

    unsigned int iSendBufferSize;


    PORTID ePortID;

    DeviceRegistry* DR;

    char strModel[DevIDBuffSize]; // to contain the MODEL (MDL) from the DevID
    char strPens[132];   // to contain the VSTATUS penID from the DevID
    int  VIPVersion;    // VIP version from the DevID

    // for internal use
    virtual BYTE* AllocMem (int iMemSize, BOOL trackmemory)
    { return AllocMem(iMemSize); }

    virtual void FreeMem (BYTE* pMem, BOOL trackmemory)
    { FreeMem(pMem); }

}; //SystemServices

APDK_END_NAMESPACE


#endif //APDK_SYSTEMSERVICES_H
