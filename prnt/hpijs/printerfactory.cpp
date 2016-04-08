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
// PrinterFactory.cpp - Morpheus component code base

#include "header.h"
#include <string.h>
#include "printerfactory.h"

APDK_BEGIN_NAMESPACE

PrinterFactory* PrinterFactory::s_Instance = NULL;   // enforce the singleton
PrinterFactory::ProxyListElement* PrinterFactory::s_ProxyList = NULL;
unsigned int PrinterFactory::s_uFamilyCount = 0;
unsigned int PrinterFactory::s_uPrinterCount = 0;

extern void HP_strcat(char* str1, const char* str2);

//PrinterFactory
//! Constructor for PrinterFactory
/*!
******************************************************************************/
PrinterFactory::PrinterFactory
(
)
{
    TRACE("PF::PF\n");
    // the static factory is coming on-line, do what you need here.
    TRACE("PF - static factory construction complete\n");
} //PrinterFactory


//~PrinterFactory
//! Destructor for PrinterFactory
/*!
******************************************************************************/
PrinterFactory::~PrinterFactory
(
)
{
    TRACE("PF::~PF - static factory destructing\n");
    ProxyListElement* t_ListEntry;
    while (s_ProxyList)
    {
        TRACE(" - deleting %s from factory list\n", s_ProxyList->printerProxyElement->GetFamilyName());
        t_ListEntry = s_ProxyList;
        s_ProxyList = t_ListEntry->next;
        delete t_ListEntry;
    }
    s_Instance = NULL;  // factory is gone
    TRACE("finished PF::~PF\n");
} //~PrinterFactory


//Register
//! Register proxy classes for printer families
/*!
This method is used by the constuctor of the PrinterProxy subclasses to
register themselves with the factory.  They call Register(this) to pass
themselves to the factory.
******************************************************************************/
void PrinterFactory::Register
(
    const PrinterProxy* thePrinterProxy         //!< [in]pointer to the PrinterProxy
)
{
    // use the first registration of a printer proxy to force the instantiation
    // of the only PrinterFactory
    if (s_Instance == NULL)
    {
        TRACE("PF::Register - asking for new factory\n");
        // create the singleton
        s_Instance = new PrinterFactory;
    }

    TRACE("PF::Register\n");
    // we know we'll need an entry in the list so create it first
    ProxyListElement* t_ListEntry = new(ProxyListElement);
    if (t_ListEntry == NULL)
    {
        // we have a problem - the driver is not running yet
        // the RTL is registering these static printer proxy servers and
        // the main code hasn't started yet.
        return;
    }
    // this list works so simply because we don't care about the order
    // and we always insert the next entry before the current head.
    t_ListEntry->printerProxyElement = thePrinterProxy;
    t_ListEntry->next = s_ProxyList;
    s_ProxyList = t_ListEntry;
    s_uFamilyCount++;
    s_uPrinterCount += thePrinterProxy->GetModelCount();

    TRACE("PF::Register %s registered with PrinterFactory\n", thePrinterProxy->GetFamilyName());
    TRACE("Printers supported by class are:\n");
    MODEL_HANDLE myHandle = thePrinterProxy->StartModelNameEnum();
    const char* printerName;
    while ((printerName = thePrinterProxy->GetNextModelName(myHandle)))
    {
        TRACE("     %s\n", printerName);
    }
} //Register


//UnRegister
//! UnRegister proxy classes for printer families
/*!
PrinterProxy subclasses use this to unregister themselves with their destuctor
executes.
******************************************************************************/
void PrinterFactory::UnRegister
(
    const PrinterProxy* thePrinterProxy     //!< [in]pointer to the PrinterProxy to unregister
)
{
    // use the first registration of a printer proxy to force the instantiation
    // of the only PrinterFactory
    if (s_Instance == NULL)
    {
        TRACE("Unregistering a PrinterProxy when factory does not exist\n");
        return;
    }

    TRACE("PF::UnRegister %s\n", thePrinterProxy->GetFamilyName());
    if (s_ProxyList == NULL)
    {
        TRACE("PF::UnRegister - Printer Factory printer list is empty!\n");
    }

    ProxyListElement* prevListEntry = NULL;
    ProxyListElement* t_ListEntry = s_ProxyList;
    bool bFound = false;

    while ((t_ListEntry) && (!bFound))
    {
        if (t_ListEntry->printerProxyElement == thePrinterProxy)
        {
            TRACE("We found the matching proxy by address\n");
            bFound = true;
            if (prevListEntry != NULL) //somewhere in list
            {
                prevListEntry->next = t_ListEntry->next;
            }
            else    // at head of list
            {
                s_ProxyList = t_ListEntry->next;
            }
            delete t_ListEntry;
            s_uFamilyCount--;
            s_uPrinterCount -= thePrinterProxy->GetModelCount();
        }
        prevListEntry = t_ListEntry;
        t_ListEntry = t_ListEntry->next;
    }
    TRACE("PF::UnRegister removed %s from PrinterFactory\n", thePrinterProxy->GetFamilyName());
    TRACE("PF::UnRegister - factory now supports %d families and %d printers\n",
        s_uFamilyCount,
        s_uPrinterCount);

    if (s_ProxyList == NULL)
    {
        TRACE("PF::UnRegister asking for deletion of empty Factory\n");
        delete PrinterFactory::GetInstance();
    }
} //UnRegister


//GetNextFamilyName
//! return the next familiy name that is in the factory
/*!
Move to the next family in the factory and then return the name of that family.
This moved to the next family first!
FAMILY_HANDLE is changed to the next family.  If there are no more families in
the factory then FAMILY_HANDLE will be NULL.

\return
name of the next family in the factory<br>
NULL if there are no more families in the factory
******************************************************************************/
const char* PrinterFactory::GetNextFamilyName
(
    FAMILY_HANDLE& theFamilyHandle          //!< [in][out]handle to the current family
) const
{
    TRACE("PF::GetNextFamilyName()\n");
    if (nextFamily(theFamilyHandle))
    {
        return getPrinterProxy(theFamilyHandle)->GetFamilyName();
    }
    else
    {
        return NULL;
    }
} //GetNextFamilyName


//GetFamilyName
//! return the family namne that supports the printer
/*!
\return
the name of the family that supports the printer name passed in.  May be NULL
if there is no family in the factory that supports the printer name.
******************************************************************************/
const char* PrinterFactory::GetFamilyName
(
    const char* thePrinterName                  //!< [in] name of the printer model
) const
{
    const char* szFamilyName = NULL;
    FAMILY_HANDLE myFamilyHandle = FindDevIdMatch(thePrinterName);
    if (myFamilyHandle)
    {
        szFamilyName = getPrinterProxy(myFamilyHandle)->GetFamilyName();
    }
    return szFamilyName;
} //GetFamilyName

//NextModel
//! return the next model name for the current family that is in the factory
/*!
MODEL_HANDLE is updated durint this processes
\return
Name of the next model that this family supports.  Will be NULL at the end of
the model list.
******************************************************************************/
const char* PrinterFactory::GetNextModelName
(
    FAMILY_HANDLE theFamilyHandle,          //!< [in] handle to the current family
    MODEL_HANDLE& theModelHandle            //!< [in][out] handle to the current model
) const
{
    TRACE("PF::GetNextModelName()\n");
    const char* szModelName = NULL;
    if (theFamilyHandle == NULL)
    {
        theModelHandle = NULL;
    }
    else
    {
        szModelName = getPrinterProxy(theFamilyHandle)->GetNextModelName(theModelHandle);
    }
    return szModelName;
} //GetNextModelName


//GetNextPrinterName
//! return the next printer name in the factory
/*!
This returns the next printer (model) name supported in the factory.  This
iterates over the whole factory without regard to families.  PRINTER_HANDLE is
updated during this process.

\return
Name of the printer in the factory.  Will be NULL after all the printers in the
factory have been enumerated.
******************************************************************************/
const char* PrinterFactory::GetNextPrinterName
(
    PRINTER_HANDLE& thePrinterHandle        //!< [in][out] handle to the printer
) const
{
    TRACE("PF::GetNextPrinterName()\n");
    static FAMILY_HANDLE familyHandle;
    nextPrinter(familyHandle, thePrinterHandle);
    return reinterpret_cast<const char*>(thePrinterHandle);
} //GetNextPrinterName


//nextFamily
//! moves the family handle to the next family in the factory
/*!
updates the FAMILY_HANDLE to the next family in the factory.  After the last
family in the factory the FAMILY_HANDLE will be NULL.

\return
true - there was another family in the factory (ie FAMILY_HANDLE is not NULL)<br>
false - there were no more families in the factory (ie FAMILY_HANDLE is NULL)
******************************************************************************/
bool PrinterFactory::nextFamily
(
    FAMILY_HANDLE& theFamilyHandle          //!< [in][out] handle to the family
) const
{
    TRACE("PF::nextFamily()\n");
    bool bMore = true;
    const ProxyListElement* proxyList = getProxyListElement(theFamilyHandle);
    if (proxyList == NULL)
    {
        TRACE("PF::nextFamily - reset to head of list\n");
        theFamilyHandle = s_ProxyList;
    }
    else
    {
        TRACE("PF::nextFamily - next element\n");
        theFamilyHandle = proxyList->next;
    }
    if (theFamilyHandle == NULL)
    {
        TRACE("PF::nextFamily - NULL (end of list)\n");
        bMore = false;
    }
    return bMore;
} //nextFamily


//EnumDevices
//! moves the family handle to the next family in the factory
/*!
updates the FAMILY_HANDLE to the next family in the factory.  After the last
family in the factory the FAMILY_HANDLE will be NULL.

\return
true - there was another family in the factory (ie FAMILY_HANDLE is not NULL)<br>
false - there were no more families in the factory (ie FAMILY_HANDLE is NULL)
******************************************************************************/
const PRINTER_TYPE PrinterFactory::EnumDevices
(
	FAMILY_HANDLE& theFamilyHandle
) const
{
    TRACE("PF::EnumDevices()\n");
    if (nextFamily(theFamilyHandle))
    {
        return getPrinterProxy(theFamilyHandle)->GetPrinterType();
    }
    else
    {
        return UNSUPPORTED;
    }
}

//nextPrinter
//! determines of there is another printer in the factory
/*! moves the modle handle to the next model in the family.  If there are no more
models in the family then it moves the family handle to the next family and the
model handle to the start of that families model list.
both the family and model handels can be updated during this process.  Both will
be NULL if there are no more printers in this iteration of the factory.

\return
true - there was another printer in the factory and the family/model handle references it<br>
false - there were no more printers in the factory and both handles are NULL
******************************************************************************/
bool PrinterFactory::nextPrinter
(
    FAMILY_HANDLE& theFamilyHandle,         //!< [in][out] current family handle
    MODEL_HANDLE& theModelHandle            //!< [in][out] current model handle
) const
{
    TRACE("PF::nextPrinter(,)\n");
    bool bMore = true;
    if (theFamilyHandle == NULL)   //this is the first time through
    {
        theFamilyHandle = StartFamilyNameEnum();
        if (nextFamily(theFamilyHandle))
        {
            theModelHandle = StartModelNameEnum(theFamilyHandle);
        }
        else
        {
            bMore = false;      // no families in the list
            goto EXIT;
        }
    }
    if (GetNextModelName(theFamilyHandle, theModelHandle) == NULL) //end of model list
    {
        if(nextFamily(theFamilyHandle))
        {
            theModelHandle = StartModelNameEnum(theFamilyHandle);
            if (GetNextModelName(theFamilyHandle, theModelHandle) == NULL)
            {
                bMore = false;
            }
        }
        else
        {
            bMore = false;
        }
    }

EXIT:
    {
        return bMore;
    }
} //nextPrinter


//FindDevIdMatch
//! Find a match for a Device ID string in the factory
/*!
The device ID string can be a device ID string retrived from a printer or it
can be a simple model name or family name.  This will walk through each family
in the factory and will check to see if it is a simple family or model string
and if not then will check for a valid device ID string.

\return
A family handle based on the device ID string passed in.<br>
NULL if there is no match.
******************************************************************************/
const FAMILY_HANDLE PrinterFactory::FindDevIdMatch
(
    const char* szDevIdString           //!< [in] well formed device id string
) const
{
    TRACE("PF::FindDevIdMatch(%s)\n", szDevIdString);
    FAMILY_HANDLE myFamilyHandle = StartFamilyNameEnum();

	bool  bDevIDString = TRUE;
    if ((!strstr(szDevIdString, "MFG:") &&
        !strstr(szDevIdString+2, "MFG:") &&
        !strstr(szDevIdString, "MANUFACTURER:") &&
        !strstr(szDevIdString+2, "MANUFACTURER:")) ||
        (!strstr(szDevIdString, "MDL:") &&
        !strstr(szDevIdString+2, "MDL:") &&
        !strstr(szDevIdString, "MODEL:") &&
        !strstr(szDevIdString+2, "MODEL:")) ||
        ((szDevIdString[0] == '\0') && (szDevIdString[1] == '\0')))
    {
        bDevIDString = FALSE;
    }

    while (nextFamily(myFamilyHandle))
    {
		if (bDevIDString)
		{
			// check to see if a full device ID String match
			if (getPrinterProxy(myFamilyHandle)->DeviceMatchQuery(szDevIdString))
			{
				break;  // found it!
			}
		}
		else
		{
			// check the family and see if they passed a simple model string
			if (getPrinterProxy(myFamilyHandle)->ModelMatchQuery(szDevIdString))
			{
				break;  // found it!
			}
		}
    }
    // if we never had a match (i.e. did the break) then myFamilyHandle is NULL
    return myFamilyHandle;
} //FindDevIdMatch

//GetModelBits
//! Get the bitwise return based on the registered family
/*!

\return
A bitwise int based on the family registered.<br
0 if there is no printer registered.
******************************************************************************/
const unsigned int PrinterFactory::GetModelBits
(
) const
{
	unsigned int bits=0;
    TRACE("PF::GetModelBits\n");
    FAMILY_HANDLE myFamilyHandle = StartFamilyNameEnum();

    while (nextFamily(myFamilyHandle))
    {
		if (myFamilyHandle)
		{
			bits = bits | getPrinterProxy(myFamilyHandle)->GetModelBit();
		}
    }
    return bits;
} //GetModelBits

//GetModelString
//! Get the bitwise return based on the registered family
/*!

\return
concatnated string based on the family registered.<br
null string if there is no printer registered.
******************************************************************************/
const void PrinterFactory::GetModelString
(
	char* mresult,
    int   mresult_length
) const
{
	assert(mresult);
	mresult[0] = '\0';

    TRACE("PF::GetModelString\n");
    FAMILY_HANDLE myFamilyHandle = StartFamilyNameEnum();
    int    i = 2;
    const char   *p;
    while (nextFamily(myFamilyHandle))
    {
        p = getPrinterProxy(myFamilyHandle)->GetFamilyName();
        i += strlen(p) + 1;
        if (i > mresult_length) break;
		HP_strcat(mresult, p);
		HP_strcat(mresult, " ");
    }
} //GetModelBits


APDK_END_NAMESPACE
