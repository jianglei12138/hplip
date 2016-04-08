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
// PrinterFactory.h - Morpheus component code base

#if !defined(APDK_PRINTERFACTORY_H)
    #define APDK_PRINTERFACTORY_H

#include "printerproxy.h"           // the factory has to know about the proxies

APDK_BEGIN_NAMESPACE

typedef const void* FAMILY_HANDLE;
typedef MODEL_HANDLE PRINTER_HANDLE;

#define pPFI PrinterFactory::GetInstance()

//PrinterFactory
//! Provides the interface to enumerate models and create printer classes
/*!
PrinterFactory is a singleton that allows the caller to enumerate:
\li Families supported
\li Models supported within a family
\li Printers supported in the factory

Callers use the factory to create the appropriate printer class based on
familiy name, model name, or device ID string.
******************************************************************************/

// Specify a class prototype, otherwise "virtual printer" fails with error: ISO C++
// forbids declaration of "Printer" with no type
class Printer;

class PrinterFactory
{
	friend class dummy;
public:
    // public API

    // get class related information
    inline static const PrinterFactory* GetInstance() { return s_Instance; }
    inline const unsigned int GetPrinterCount() const { return s_uPrinterCount; }
    inline const unsigned int GetFamilyCount() const { return s_uFamilyCount; }


    // enumerate Family APIs
    FAMILY_HANDLE StartFamilyNameEnum() const { return NULL; }
    const char* GetNextFamilyName(FAMILY_HANDLE& theFamilyHandle) const;

    // enumerate models in a family APIs
    inline MODEL_HANDLE StartModelNameEnum(FAMILY_HANDLE theFamilyHandle) const;
    const char* GetNextModelName(FAMILY_HANDLE theFamilyHandle, MODEL_HANDLE& theModelHandle) const;

    // enumerate all printers in factroy APIs
    PRINTER_HANDLE StartPrinterNameEnum() const { return NULL; }
    const char* GetNextPrinterName(PRINTER_HANDLE& thePrinterHandle) const;

    // get family name based on printer name
    const char* GetFamilyName(const char* thePrinterName) const;
    const char* GetFamilyName(const FAMILY_HANDLE theFamilyHandle) const;
	const PRINTER_TYPE GetFamilyType(const FAMILY_HANDLE theFamilyHandle) const;
	const PRINTER_TYPE EnumDevices( FAMILY_HANDLE& theFamilyHandle) const;
	const unsigned int GetModelBits() const;
	const void GetModelString(char* modelstring, int modelstring_length) const;

    // get a handle to family based on device string
//    const FAMILY_HANDLE FindModelMatch(const char* szModel) const;
    const FAMILY_HANDLE FindDevIdMatch(const char* szDevIdString) const;

    // Printer (un)registration and creation APIs
    static void Register(const PrinterProxy* thePrinterProxy);
    static void UnRegister(const PrinterProxy* thePrinterProxy);
    inline Printer* CreatePrinter(SystemServices* pSys, const FAMILY_HANDLE& theFamilyHandle) const;

private:
    // constructors/destructor - Only the factory can construct or destruct itself
    PrinterFactory();
    ~PrinterFactory();                              // doesn't need to be virtual - there is only one
    PrinterFactory(const PrinterFactory& theFactory) {}; // dont' let the compiler generate a copy constructor

    // Only available to factory manager
    struct ProxyListElement
    {
        const PrinterProxy* printerProxyElement;
        ProxyListElement* next;
    };

    inline const ProxyListElement* getProxyListElement(FAMILY_HANDLE theFamilyHandle) const;
    inline const PrinterProxy* getPrinterProxy(FAMILY_HANDLE theFamilyHandle) const;
    bool nextPrinter(FAMILY_HANDLE& theFamilyHandle, MODEL_HANDLE& theModelHandle) const;
    bool nextFamily(FAMILY_HANDLE& theFamilyHandle) const;

    static PrinterFactory* s_Instance;
    static ProxyListElement* s_ProxyList;

    static unsigned int s_uPrinterCount;
    static unsigned int s_uFamilyCount;

}; //PrinterFactory


//getProxyListElement
//! Return the the correct ProxyListELement based on the FAMILY_HANDLE
inline const PrinterFactory::ProxyListElement* PrinterFactory::getProxyListElement
(
    const FAMILY_HANDLE theFamilyHandle
) const
{
    return reinterpret_cast<const ProxyListElement*>(theFamilyHandle);
}


//getPrinterProxy
//! Return the correct PrinterProxy based on the FAMILY_HANDLE
inline const PrinterProxy* PrinterFactory::getPrinterProxy
(
    const FAMILY_HANDLE theFamilyHandle
) const
{
    return theFamilyHandle ? getProxyListElement(theFamilyHandle)->printerProxyElement : NULL;
} //GetPrinterPoxy


//StartModelNameEnum
//! Prepare for a Model Name enumeration based on a family
inline MODEL_HANDLE PrinterFactory::StartModelNameEnum
(
    const FAMILY_HANDLE theFamilyHandle
) const
{
    return getPrinterProxy(theFamilyHandle)->StartModelNameEnum();
} //StartModelNameEnum


//GetFamilyName
//! Get the family name string based on the family handel
inline const char* PrinterFactory::GetFamilyName
(
    const FAMILY_HANDLE theFamilyHandle     //!< handle to the family
) const
{
    return getPrinterProxy(theFamilyHandle)->GetFamilyName();
} //GetFamilyName

//GetFamilyType
//! Get the family printer type enum based on the family handel
inline const PRINTER_TYPE PrinterFactory::GetFamilyType
(
    const FAMILY_HANDLE theFamilyHandle     //!< handle to the family
) const
{
    return getPrinterProxy(theFamilyHandle)->GetPrinterType();
} //GetFamilyName



//CreatePrinter
//! Create a printer class based on the family handle
inline Printer* PrinterFactory::CreatePrinter
(
    SystemServices* pSys,                   //!< pointer to a valid SystemSerivce object
    const FAMILY_HANDLE& theFamilyHandle    //!< handle to the family to support
) const
{
    return theFamilyHandle ? getPrinterProxy(theFamilyHandle)->CreatePrinter(pSys) : NULL;
} //CreatePrinter

APDK_END_NAMESPACE

#endif //APDK_PRINTERFACTORY_H
