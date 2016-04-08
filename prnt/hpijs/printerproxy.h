/*****************************************************************************\
  Copyright (c) 2002 - 2015, HP Co.
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
// PrinterProxy.h - Morpheus component code base

#if !defined(APDK_PRINTERPROXY_H)
    #define APDK_PRINTERPROXY_H

APDK_BEGIN_NAMESPACE

typedef const void* MODEL_HANDLE;

//! Define voting ranges for the proxy voting system
typedef enum PROXY_VOTE
{
    VOTE_NO_MATCH = 0,                      //!< can't do anything for this request
    VOTE_LAST_DITCH_MATCH = 10,             //!< possible last ditch effort to do something
    VOTE_POSSIBLE_MATCH = 50,               //!< might be able to generate output
    VOTE_FAMILY_MATCH = 70,                 //!< matched by family
    VOTE_VIP_MATCH = 80,                    //!< matched because it is a VIP printer
    VOTE_SUBSTRING_MATCH = 90,              //!< a model was a substring of the full string
    VOTE_EXACT_MATCH = 100                  //!< exact match id on this printer
} PROXY_VOTE;

// Specify a class prototype, otherwise "virtual printer" fails with error: ISO C++
// forbids declaration of "Printer" with no type
class Printer;

//PrinterProxy
//!Provide act on behalf of the printer class for matching, names, voting, etc
/*!
******************************************************************************/
class PrinterProxy
{
public:
    PrinterProxy
    (
        const char* szFamilyName,
        const char* szModelNames
    );

    virtual ~PrinterProxy();                    // gcc wants it to be virtual

    // Public API
    inline const char* GetFamilyName() const { return m_szFamilyName; }
    inline unsigned int GetModelCount() const { return m_uModelCount; }
    virtual Printer* CreatePrinter(SystemServices* pSS) const = 0;
	virtual PRINTER_TYPE GetPrinterType() const = 0;
	virtual unsigned int GetModelBit() const = 0;

    inline MODEL_HANDLE StartModelNameEnum() const { return NULL; }

    const char* GetModelName(MODEL_HANDLE& theHandle) const;
    const char* GetNextModelName(MODEL_HANDLE& theHandle) const;

    PROXY_VOTE DeviceMatchQuery(const char* szDeviceString) const;
    bool ModelMatchQuery(const char* szModelString) const;

protected:
	PRINTER_TYPE m_iPrinterType;

private:
    const char* m_szFamilyName;
    const char* m_szModelNames;
    unsigned int m_uModelCount;
}; //PrinterProxy

//GetModelName
//!Get the model name based on the handle
/*!
\return
model name based on the model handle
******************************************************************************/
inline const char* PrinterProxy::GetModelName
(
    MODEL_HANDLE& theModelHandle            //!< [in][out] handle to current model
) const
{
    TRACE("PP::GetModelName() returning %s\n", reinterpret_cast<const char*>(theModelHandle));
    return reinterpret_cast<const char*>(theModelHandle);
} //GetNextModelName

APDK_END_NAMESPACE

#endif //APDK_PRINTERPROXY_H
